#include "rsync.h"
#include <map>
#include <openssl/md5.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>
#include <msgpack.hpp>

using namespace std;
using namespace rapidjson;

static const uint32_t M = 1u << 16u;


/**
 * 循环buffer
 * 每次加载10倍的分块大小
 * 加载更多的时候会将之前末尾分块大小的数据复制到开头
 */
class CircleFileBuffer {
    RChar *m_buf;
    RChar *m_cache;
    size_t m_size;
    size_t m_bufLen;
    string m_path;
    size_t m_filesize;
    size_t m_bufInFile;
    ifstream m_file;
    thread m_thread;
public:
    CircleFileBuffer(const string& path, size_t size) {
        m_size = size;
        m_bufLen = size * 10;
        m_path = path;
        m_buf = nullptr;
        m_cache = nullptr;
        m_filesize = 0;
        m_bufInFile = 0;
    }
    ~CircleFileBuffer() {
        delete[] m_buf;
        delete[] m_cache;
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    bool loadFile() {
        if (m_buf != nullptr) {
            return false;
        }
        m_buf = new RChar[m_size * 10];
        m_cache = new RChar[m_size * 10];
        m_file = ifstream(m_path, ifstream::ate | ifstream::binary);
        if (!m_file.is_open()) {
            return false;
        }
        m_filesize = m_file.tellg();
        cout << "read file size " << m_filesize << endl;
        m_file.seekg(0, ios::beg);
        m_file.read(m_buf, m_bufLen);
        readToCache();
        return true;
    }

    size_t fileSize() {
        return m_filesize;
    }

    /**
     * 获取从offset开始的数据
     * @param offset offset
     * @return buf
     */
    const RChar* bufFrom(size_t offset) {
        assert(offset >= m_bufInFile);
        if (offset + m_size > m_bufInFile + m_bufLen) {
            loadMore();
        }
        size_t pos = offset - m_bufInFile;
        return m_buf + pos;
    }

    void loadMore() {
        m_thread.join();
        std::swap(m_buf, m_cache);
        m_bufInFile += m_bufLen - m_size;
        if (!m_file.eof()) {
            readToCache();
        }
    }
private:
    void readToCache() {
        m_thread = thread([this] {
            if (this->m_file.eof()) {
                return false;
            }
            std::memcpy(m_cache, m_buf + m_bufLen - m_size, m_size);
            m_file.read(m_cache + m_size, m_bufLen - m_size);
            return true;
        });
    }
};

static const char HEX16[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

string md5str(const RChar *buf, size_t offset, size_t size) {
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, buf + offset, size);
    auto *output = new unsigned char[MD5_DIGEST_LENGTH];
    MD5_Final(output, &md5_ctx);
    string result;
    result.reserve(MD5_DIGEST_LENGTH * 2 + 1);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        auto h = output[i] / 16;
        auto l = output[i] % 16;
        result.append(1, HEX16[h]);
        result.append(1, HEX16[l]);
    }
    delete[] output;
    return result;
}

string md5str(const vector<RChar> &buf, size_t offset, size_t size) {
    if (size + offset >= buf.size()) {
        size = buf.size() - offset;
    }
    return md5str(buf.data(), offset, size);
}

AdlerResult adler32(const vector<RChar> &buf, size_t offset, size_t size) {
    return adler32(buf.data(), offset, size);
}

AdlerResult adler32(const RChar *buf, size_t offset, size_t size) {
    uint32_t a = 0;
    uint32_t b = 0;
    size_t k = offset;
    size_t l = offset + size - 1;
    for (size_t i = k; i <= l; i++) {
        int v = buf[i];
        a += v;
        b += (l - i + 1) * v;
    }
    a = a % M;
    b = b % M;
    uint32_t s = a + (b << 16u);
    auto adlerResult = AdlerResult{
            a,
            b,
            s,
    };
    return adlerResult;
}


AdlerResult rolling_adler32(const RChar* buf, size_t bufLen, size_t offset, size_t size, const AdlerResult &pre) {
    size_t k = offset - 1;
    size_t l = k + size - 1;
    uint32_t ak = buf[k];
    uint32_t al1 = 0;
    if (l + 1 >= bufLen) {
        al1 = 0;
    } else {
        al1 = buf[l + 1];
    }

    uint32_t a = (pre.a - ak + al1) % M;
    uint32_t b = (pre.b - (l - k + 1) * ak + a) % M;
    uint32_t s = a + (b << 16u);
    auto result = AdlerResult{
            a, b, s
    };
    return result;
}

AdlerResult rolling_adler32(const vector<RChar> &buf, size_t offset, size_t size, const AdlerResult &pre) {
    return rolling_adler32(buf.data(), buf.size(), offset, size, pre);
}

list<Package> checksum(const string &path, forward_list<Chunk> &original, size_t size) {
    auto table = new forward_list<Chunk>[65536];
    for (auto &chunk : original) {
        table[chunk.ad32.a].push_front(std::move(chunk));
    }
    CircleFileBuffer file(path, size);
    auto ret = file.loadFile();
    assert(ret);

    auto result = list<Package>();
    size_t stackStart = 0;
    size_t stackEnd = 0;
    bool hasPre = false;
    AdlerResult pre = AdlerResult();
    size_t ad32_i = 0;
    size_t md5_i = 0;
    for (size_t k = 0; k < file.fileSize(); k++) {
        const auto *buf = file.bufFrom(k);
        size_t realSize = std::min(size, file.fileSize() - k);
        AdlerResult adler;
        if (hasPre) {
            adler = rolling_adler32(buf, realSize, 0, size, pre);
        } else {
            adler = adler32(buf, 0, realSize);
        }
        pre = adler;
        hasPre = true;
        if (!table[adler.a].empty()) {
            const auto &chunks = table[adler.a];
            auto vmd5 = string();
            for (const auto &chunk : chunks) {
                if (chunk.ad32.s != adler.s) {
                    continue;
                }
                ad32_i++;
                if (vmd5.length() == 0) {
                    vmd5 = md5str(buf, 0, size);
                }
                if (vmd5 == chunk.md5) {
                    md5_i++;
                    if (stackEnd > stackStart) {
                        auto pack = Package(
                                2,
                                Chunk{},
                                VectorView{
                                        stackStart,
                                        stackEnd
                                }
                        );
                        result.push_back(std::move(pack));
                    }
                    auto chunkPack = Package{
                            1,
                            chunk,
                            VectorView{},
                    };
                    result.push_back(std::move(chunkPack));
                    k += size - 1;
                    stackStart = k + 1;
                    hasPre = false;
                    break;
                }
            }
        }
        stackEnd = k + 1;
        // make pack data not large than size
        if (stackEnd - stackStart >= size * 5) {
            assert(stackEnd-stackStart == size * 5);
            auto pack = Package(
                    2,
                    Chunk{},
                    VectorView{
                            stackStart,
                            stackEnd
                    }
            );
            result.push_back(std::move(pack));
            stackStart = stackEnd;
        }
    }
    delete[] table;
    if (stackEnd > stackStart) {
        auto pack = Package(
                2,
                Chunk{},
                VectorView{
                        stackStart,
                        stackEnd
                }
        );
        result.push_back(std::move(pack));
    }
    std::cout << "ad32 " << ad32_i << " md5 " << md5_i << std::endl;
    return result;
}

forward_list<Chunk> makeChunk(const vector<RChar> &data, size_t size) {
    size_t len = data.size();
    size_t count = (len - 1) / size + 1;
    auto result = forward_list<Chunk>();
    for (size_t i = 0; i < count; i++) {
        size_t realSize = size;
        if ((i + 1) * size >= data.size()) {
            realSize = data.size() - i * size;
        }
        auto adler = adler32(data, i * size, realSize);
        auto md5 = md5str(data, i * size, realSize);
        auto chunk = Chunk(
                (int64_t) i,
                adler,
                std::move(md5),
                i * size,
                realSize
        );
        result.push_front(std::move(chunk));
    }
    return result;
}

forward_list<Chunk> makeChunkFromFile(const string &path, size_t size) {
    ifstream in_file(path, ifstream::binary);
    assert(in_file.is_open());
    auto buf = new char[size];
    int64_t i = 0;
    size_t offset = 0;
    auto result = forward_list<Chunk>();
    while (!in_file.eof()) {
        in_file.read(buf, size);
        auto realSize = in_file.gcount();
        auto adler = adler32((const RChar *) buf, 0, realSize);
        auto md5 = md5str((const RChar *) buf, 0, realSize);
        auto chunk = Chunk{
                i,
                adler,
                std::move(md5),
                offset,
                static_cast<size_t>(realSize),
        };
        result.push_front(std::move(chunk));
        offset += realSize;
        i++;
    }
    delete[] buf;
    in_file.close();
    return result;
}

void writeResultToFile(const string &sourceFile, const string &topath, const string &diffPath,
                       const list<Package> &result, size_t size) {
    ifstream in_file(sourceFile, ifstream::binary);
    assert(in_file.is_open());
    ofstream out_file(topath, ofstream::binary);
    assert(out_file.is_open());
    ifstream diff_file(diffPath, ifstream::binary);
    assert(diff_file.is_open());
    auto buf = new char[size*5];
    for (const auto &package : result) {
        if (package.type == 1) {
            // chunk
            in_file.seekg(package.chunk.offset);
            in_file.read(buf, package.chunk.size);
            out_file.write(buf, package.chunk.size);
        } else {
            // data
            auto len = package.data.end - package.data.start;
            diff_file.seekg(package.data.start);
            diff_file.read(buf, len);
            out_file.write(buf, len);
        }
    }
    delete[] buf;
    out_file.flush();
    in_file.close();
    out_file.close();
    diff_file.close();
    cout << "write to file " << topath << endl;
}

JsonChunk loadJsonChunks(const string &path) {
    FILE *fp = fopen(path.c_str(), "r");
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);
    assert(d.IsObject());
    assert(d.HasMember("fileRefId"));
    assert(d["fileRefId"].IsString());
    string fileRefId = d["fileRefId"].GetString();
    assert(d.HasMember("size"));
    assert(d["size"].IsInt());
    size_t size = d["size"].GetInt();
    assert(d.HasMember("data"));
    assert(d["data"].IsArray());
    auto data = d["data"].GetArray();
    auto chunks = forward_list<Chunk>();
    for (SizeType i = 0; i < data.Size(); i++) {
        auto v = data[i].GetObject();
        assert(v.HasMember("id"));
        assert(v["id"].IsInt64());
        int64_t id = v["id"].GetInt64();
        assert(v.HasMember("adler32"));
        assert(v["adler32"].IsUint());
        uint32_t adler32 = v["adler32"].GetUint();
        assert(v.HasMember("md5"));
        assert(v["md5"].IsString());
        string md5 = v["md5"].GetString();
        auto adler = AdlerResult{
                adler32 % M,
                adler32 / M,
                adler32,
        };
        Chunk chunk(id, adler, std::move(md5), 0, 0);
        chunks.push_front(std::move(chunk));
    }
    return JsonChunk(std::move(fileRefId), size, std::move(chunks));
}

void writeResultToJson(const string &path, const list<Package> &result, const vector<RChar> &buf) {
    FILE* fp = fopen(path.c_str(), "w");
    char writeBuffer[65536];
    FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
    Writer<FileWriteStream> writer(os);

    writer.StartObject();
    writer.Key("fileId");
    writer.String("123456");
    writer.Key("version");
    writer.Int(0);
    writer.Key("data");
    writer.StartArray();
    for (const auto& pack : result) {
        writer.StartObject();
        writer.Key("type");
        writer.Int(pack.type);
        if (pack.type == 1) {
            // chunk
            writer.Key("chunkId");
            writer.Int64(pack.chunk.id);
        } else {
            // data
            writer.Key("data");
            writer.StartArray();
            for (auto i = pack.data.start; i < pack.data.end; i++) {
                writer.Int(buf[i]);
            }
            writer.EndArray();
        }
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();

    fclose(fp);
    cout << "write result json to " << path << endl;
}


void writeResultToStream(const list<Package> &result, const string &diffPath, ostream &os, size_t size) {
    ifstream diff_file(diffPath, ifstream::binary);
    msgpack::packer<ostream> pk(&os);
    // todo: 需要传入文件信息
    pk.pack(std::string("123456")); // fileId
    pk.pack(0); // version
    pk.pack_array(result.size());
    char *buf = new char[size * 5];
    for (const auto& pack : result) {
        pk.pack_map(2);
        pk.pack(string("type"));
        pk.pack(pack.type);
        if (pack.type == 1) {
            pk.pack(string("chunkId"));
            pk.pack_int64(pack.chunk.id);
        } else {
            pk.pack(string("data"));
            auto len = pack.data.end-pack.data.start;
            diff_file.seekg(pack.data.start);
            diff_file.read(buf, len);
            pk.pack_bin(len);
            pk.pack_bin_body(buf, len);
        }
    }
    diff_file.close();
    cout << "write result to stream" << endl;
}