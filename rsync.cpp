#include "rsync.h"
#include <map>
#include <openssl/md5.h>
#include <iostream>

using namespace std;

static const uint32_t M = 1u << 16u;

/*
string md5str(const vector<char>& buf, int offset, int size) {
    if (size + offset >= buf.size()) {
        size = buf.size() - offset;
    }
    MD5 md5(buf.data() + offset, size);
    return md5.toStr();
}
 */
string md5str(const vector<unsigned char>& buf, size_t offset, size_t size) {
    if (size + offset >= buf.size()) {
        size = buf.size() - offset;
    }
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, buf.data()+offset, size);
    auto *output = new unsigned char[MD5_DIGEST_LENGTH+1];
    MD5_Final(output, &md5_ctx);
    output[MD5_DIGEST_LENGTH] = 0;
    auto result = string((char*)output);
    delete[] output;
    return result;
}

AdlerResult adler32(const vector<unsigned char>& buf, size_t offset, size_t size) {
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

AdlerResult rolling_adler32(const vector<unsigned char>& buf, size_t offset, size_t size, const AdlerResult& pre) {
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

vector<Package> checksum(const vector<unsigned char>& buf, const vector<Chunk>& original, size_t size) {
    auto table = new vector<Chunk>[65536];
    for (const auto& chunk : original) {
        table[chunk.ad32.a].push_back(chunk);
    }

    auto result = vector<Package>();
    result.reserve(original.size());
    auto stackData = vector<unsigned char>();
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
                        vector<unsigned char>(),
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

vector<Chunk> makeChunk(const vector<unsigned char>& data, size_t size) {
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