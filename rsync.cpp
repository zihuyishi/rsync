#include "rsync.h"
#include <map>
#include <openssl/md5.h>
#include <iostream>

using namespace std;

static const uint32_t M = 1u << 16u;

class ChunkHashTable {
    map<uint32_t, vector<Chunk>> m_table;
public:
    ChunkHashTable() = default;
    vector<Chunk>& get(const AdlerResult& key) {
        auto result = m_table.find(key.s);
        if (result == m_table.end()) {
            m_table.insert(make_pair(key.s, vector<Chunk>()));
            return m_table[key.s];
        }
        return result->second;
    }
    bool exists(const AdlerResult& key) {
        return m_table.find(key.s) != m_table.end();
    }
};

class ChunkArrayTable {
    vector<Chunk> *m_table;
public:
    ChunkArrayTable() {
        m_table = new vector<Chunk>[65536];
    }
    ~ChunkArrayTable() {
        delete[] m_table;
    }

    vector<Chunk>& get(const AdlerResult& key) {
        return m_table[key.a];
    }
    bool exists(const AdlerResult& key) {
        return !m_table[key.a].empty();
    }
};

/*
string md5str(const vector<char>& buf, int offset, int size) {
    if (size + offset >= buf.size()) {
        size = buf.size() - offset;
    }
    MD5 md5(buf.data() + offset, size);
    return md5.toStr();
}
 */
static const char HEX16[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
string md5str(const vector<RChar>& buf, size_t offset, size_t size) {
    if (size + offset >= buf.size()) {
        size = buf.size() - offset;
    }
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, buf.data()+offset, size);
    auto *output = new unsigned char[MD5_DIGEST_LENGTH];
    MD5_Final(output, &md5_ctx);
    string result;
    result.reserve(MD5_DIGEST_LENGTH * 2+1);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        auto l = output[i] % 16;
        auto h = output[i] / 16;
        result.append(1, HEX16[h]);
        result.append(1, HEX16[l]);
    }
    delete[] output;
    return result;
}

AdlerResult adler32(const vector<RChar>& buf, size_t offset, size_t size) {
    uint32_t a = 0;
    uint32_t b = 0;
    size_t k = offset;
    size_t l = offset + size - 1;
    for (size_t i = k; i <= l; i++) {
        if (i >= buf.size()) {
            break;
        }
        int v = buf[i];
        a += v;
        b += (l - i + 1) * v;
    }
    a = a % M;
    b = b % M;
    uint32_t s = a + (b << 16u);
    auto adlerResult = AdlerResult {
        a,
        b,
        s,
    };
    return adlerResult;
}

AdlerResult rolling_adler32(const vector<RChar>& buf, size_t offset, size_t size, const AdlerResult& pre) {
    size_t k = offset - 1;
    size_t l = k + size - 1;
    uint32_t ak = buf[k];
    uint32_t al1 = 0;
    if (l+1>=buf.size()) {
        al1 = 0;
    } else {
        al1 = buf[l+1];
    }

    uint32_t a = (pre.a - ak + al1) % M;
    uint32_t b = (pre.b - (l-k+1)*ak + a) % M;
    uint32_t s = a + (b << 16u);
    auto result = AdlerResult {
        a, b, s
    };
    return result;
}

vector<Package> checksum(const vector<RChar>& buf, const vector<Chunk>& original, size_t size) {
    auto table = new vector<Chunk>[65536];
    for (const auto& chunk : original) {
        table[chunk.ad32.a].push_back(chunk);
    }

    auto result = vector<Package>();
    result.reserve(original.size());
    auto stackData = vector<RChar>();
    bool hasPre = false;
    AdlerResult pre;
    size_t ad32_i = 0;
    size_t md5_i = 0;
    for (size_t k = 0; k < buf.size(); k++) {
        AdlerResult adler;
        if (hasPre) {
            adler = rolling_adler32(buf, k, size, pre);
        } else {
            adler = adler32(buf, k, size);
        }
        pre = adler;
        hasPre = true;
        if (!table[adler.a].empty()) {
            const auto& chunks = table[adler.a];
            auto vmd5 = string();
            auto found = false;
            for (const auto& chunk : chunks) {
                if (chunk.ad32.s != adler.s) {
                    continue;
                }
                ad32_i++;
                if (vmd5.length() == 0) {
                    vmd5 = md5str(buf, k, size);
                }
                if (vmd5 == chunk.md5) {
                    md5_i++;
                    if (!stackData.empty()) {
                        auto pack = Package (
                            2,
                            Chunk{},
                            std::move(stackData)
                        );
                        result.push_back(pack);
//                        stackData = vector<char>();
                    }
                    auto chunkPack = Package {
                        1,
                        chunk,
                        vector<RChar>(),
                    };
                    result.push_back(chunkPack);
                    k += size - 1;
                    hasPre = false;
                    found = true;
                    break;
                }
            }
            if (!found) {
                stackData.push_back(buf[k]);
            }
        } else {
            stackData.push_back(buf[k]);
        }
    }
    delete[] table;
    if (!stackData.empty()) {
        auto pack = Package (
            2,
            Chunk{},
            std::move(stackData)
        );
        result.push_back(pack);
    }
    std::cout << "ad32 " << ad32_i << " md5 " << md5_i << std::endl;
    return result;
}

vector<Chunk> makeChunk(const vector<RChar>& data, size_t size) {
    size_t len = data.size();
    size_t count = (len-1) / size + 1;
    auto result = vector<Chunk>();
    for (size_t i = 0; i < count; i++) {
        size_t realSize = size;
        if ((i+1) * size >= data.size()) {
            realSize = data.size() - i * size;
        }
        auto adler = adler32(data, i * size, realSize);
        auto md5 = md5str(data, i * size, realSize);
        auto chunk = Chunk {
            (int64_t)i,
            adler,
            std::move(md5),
            i * size,
            realSize
        };
        result.push_back(chunk);
    }
    return result;
}