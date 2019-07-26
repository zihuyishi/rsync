#include "rsync.h"
#include <map>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md5.h>

using namespace std;
using namespace CryptoPP;

static const uint32_t M = 1u << 16u;

/*
string md5str(const vector<uint8_t>& buf, int offset, int size) {
    if (size + offset >= buf.size()) {
        size = buf.size() - offset;
    }
    MD5 md5(buf.data() + offset, size);
    return md5.toStr();
}
 */
string md5str(const vector<uint8_t>& buf, int offset, int size) {
    if (size + offset >= buf.size()) {
        size = buf.size() - offset;
    }
    auto md5 = Weak1::MD5();
    md5.Update(buf.data() + offset, size);
    char *output = new char[md5.DigestSize()+1];
    md5.Final((byte *)output);
    output[md5.DigestSize()] = 0;
    auto result = string(output);
    delete[] output;
    return result;
}

AdlerResult adler32(const vector<uint8_t>& buf, int offset, int size) {
    uint32_t a = 0;
    uint32_t b = 0;
    int k = offset;
    int l = offset + size - 1;
    for (int i = k; i <= l; i++) {
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

AdlerResult rolling_adler32(const vector<uint8_t>& buf, int offset, int size, const AdlerResult& pre) {
    int k = offset - 1;
    int l = k + size - 1;
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

vector<Package> checksum(const vector<uint8_t>& buf, const vector<Chunk>& original, int size) {
    auto table = new vector<Chunk>[65536];
    for (const auto& chunk : original) {
        table[chunk.ad32.a].push_back(chunk);
    }

    auto result = vector<Package>();
    auto stackData = vector<uint8_t>();
    bool hasPre = false;
    AdlerResult pre;
    for (auto k = 0; k < buf.size(); k++) {
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
                if (vmd5.length() == 0) {
                    vmd5 = md5str(buf, k, size);
                }
                if (chunk.md5 == vmd5) {
                    if (!stackData.empty()) {
                        auto pack = Package (
                            2,
                            Chunk{},
                            std::move(stackData)
                        );
                        result.push_back(pack);
//                        stackData = vector<uint8_t>();
                    }
                    auto chunkPack = Package {
                        1,
                        chunk,
                        vector<uint8_t>(),
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
    return result;
}

vector<Chunk> makeChunk(const vector<uint8_t>& data, int size) {
    auto len = data.size();
    auto count = (len-1) / size + 1;
    auto result = vector<Chunk>();
    for (auto i = 0; i < count; i++) {
        auto realSize = size;
        if ((i+1) * size >= data.size()) {
            realSize = data.size() - i * size;
        }
        auto adler = adler32(data, i * size, realSize);
        auto md5 = md5str(data, i * size, realSize);
        auto chunk = Chunk {
            i,
            adler,
            std::move(md5),
            i * size,
            realSize
        };
        result.push_back(chunk);
    }
    return result;
}