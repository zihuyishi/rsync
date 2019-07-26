//
// Created by saye on 2019-07-24.
//

#ifndef RSYNC_RSYNC_H
#define RSYNC_RSYNC_H

#include <cstdint>
#include <vector>
#include <string>

typedef struct {
    uint32_t a;
    uint32_t b;
    uint32_t s;
} AdlerResult;

typedef struct {
    int id;
    AdlerResult ad32;
    std::string md5;
    int offset;
    int size;
} Chunk;

class Package {
public:
    int type; // 1 - chunk, 2 - data
    Chunk chunk;
    std::vector<uint8_t> data;
    Package(int _type, Chunk _chunk, std::vector<uint8_t>&& _data):
        type(_type), chunk(std::move(_chunk)), data(std::move(_data))
    {}
};

AdlerResult adler32(const std::vector<uint8_t>& buf, int offset, int size);
AdlerResult rolling_adler32(const std::vector<uint8_t>& buf, int offset, int size, const AdlerResult& pre);
std::vector<Package> checksum(const std::vector<uint8_t>& buf, const std::vector<Chunk>& original, int size);
std::vector<Chunk> makeChunk(const std::vector<uint8_t>& data, int size);

#endif //RSYNC_RSYNC_H
