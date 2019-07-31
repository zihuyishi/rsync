//
// Created by saye on 2019-07-24.
//

#ifndef RSYNC_RSYNC_H
#define RSYNC_RSYNC_H

#include <cstdint>
#include <vector>
#include <forward_list>
#include <list>
#include <string>

typedef char RChar;

typedef struct {
    uint32_t a;
    uint32_t b;
    uint32_t s;
} AdlerResult;

typedef struct {
    int64_t id;
    AdlerResult ad32;
    std::string md5;
    size_t offset;
    size_t size;
} Chunk;

class Package {
public:
    int type; // 1 - chunk, 2 - data
    Chunk chunk;
    std::vector<RChar> data;
    Package(int _type, Chunk _chunk, std::vector<RChar>&& _data):
        type(_type), chunk(std::move(_chunk)), data(std::move(_data))
    {}
    Package(Package&& o) noexcept :
        type(o.type), chunk(std::move(o.chunk)), data(std::move(o.data))
    {}
};

AdlerResult adler32(const std::vector<RChar>& buf, size_t offset, size_t size);
AdlerResult adler32(const RChar *buf, size_t offset, size_t size);
AdlerResult rolling_adler32(const std::vector<RChar>& buf, size_t offset, size_t size, const AdlerResult& pre);
std::list<Package> checksum(const std::vector<RChar>& buf, const std::forward_list<Chunk>& original, size_t size);
std::forward_list<Chunk> makeChunk(const std::vector<RChar>& data, size_t size);
std::forward_list<Chunk> makeChunkFromFile(const std::string& path, size_t size);
void writeResultToFile(const std::string& sourceFile, const std::string& topath, const std::list<Package>& result, size_t size);

#endif //RSYNC_RSYNC_H
