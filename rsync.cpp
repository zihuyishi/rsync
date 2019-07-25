#include "rsync.h"
#include "md5.h"
#include <map>
using namespace std;

static const uint32_t M = 1u << 16u;

string md5str(const vector<uint8_t>& buf, int offset, int size) {
    auto *cstr = new char[size + 1];
    int i = offset;
    for (; i < offset + size; i++) {
        if (i >= buf.size()) {
            break;
        }
        char c = buf[i];
        cstr[i-offset] = c;
    }
    cstr[i] = 0;
    string result = string(cstr);
    delete[] cstr;
    MD5 md5(result);
    return md5.toStr();
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

AdlerResult rolling_adler32(const vector<uint8_t>& buf, int offset, int size, AdlerResult pre) {
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
    auto table = map<uint32_t, vector<Chunk>>();
    for (const auto& chunk : original) {
        if (table.find(chunk.ad32) == table.end()) {
            table[chunk.ad32] = vector<Chunk>();
        }
        table[chunk.ad32].push_back(chunk);
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
        if (table.find(adler.s) != table.end()) {
            auto vmd5 = md5str(buf, k, size);
            auto chunks = table[adler.s];
            for (const auto& chunk : chunks) {
                if (chunk.md5 == vmd5) {
                    if (!stackData.empty()) {
                        auto pack = Package {
                            2,
                            Chunk{},
                            stackData,
                        };
                        result.push_back(pack);
                        stackData = vector<uint8_t>();
                    }
                    auto chunkPack = Package {
                        1,
                        chunk,
                        vector<uint8_t>(),
                    };
                    result.push_back(chunkPack);
                    k += size - 1;
                    hasPre = false;
                    break;
                } else {
                    stackData.push_back(buf[k]);
                }
            }
        } else {
            stackData.push_back(buf[k]);
        }
    }
    if (!stackData.empty()) {
        auto pack = Package {
            2,
            Chunk{},
            stackData,
        };
        result.push_back(pack);
    }
    return result;
}

vector<Chunk> makeChunk(const vector<uint8_t>& data, int size) {
    auto len = data.size();
    auto count = (len-1) / size + 1;
    auto result = vector<Chunk>();
    for (auto i = 0; i < count; i++) {
        auto adler = adler32(data, i * size, size);
        auto md5 = md5str(data, i * size, size);
        auto chunk = Chunk {
            i,
            adler.s,
            md5,
        };
        result.push_back(chunk);
    }
    return result;
}