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

typedef struct Chunk {
    int64_t id;
    AdlerResult ad32;
    std::string md5;
    size_t offset;
    size_t size;

    Chunk() = default;

    Chunk(const Chunk &o) = default;

    Chunk(int64_t _id, AdlerResult _ad32, std::string &&_md5, size_t _offset, size_t _size) :
            id(_id), ad32(_ad32), md5(std::move(_md5)), offset(_offset), size(_size) {}

    Chunk(Chunk &&o) noexcept :
            id(o.id), ad32(o.ad32), md5(std::move(o.md5)),
            offset(o.offset), size(o.size) {}
} Chunk;

typedef struct {
    size_t start;
    size_t end;
} VectorView;

typedef struct JsonChunk {
     std::string fileRefId;
     size_t size;
     std::forward_list<Chunk> data;
     JsonChunk(std::string&& _fileRefId, size_t _size, std::forward_list<Chunk>&& _data) :
        fileRefId(std::move(_fileRefId)), size(_size), data(std::move(_data))
     {}
     JsonChunk(JsonChunk &&o) :
        fileRefId(std::move(o.fileRefId)), size(o.size), data(std::move(o.data))
     {}
} JsonChunk;

class Package {
public:
    int type; // 1 - chunk, 2 - data
    Chunk chunk;
    VectorView data;

    Package(int _type, Chunk _chunk, const VectorView &_data) :
            type(_type), chunk(std::move(_chunk)), data(_data) {}

    Package(Package &&o) noexcept :
            type(o.type), chunk(std::move(o.chunk)), data(o.data) {}
};

AdlerResult adler32(const std::vector<RChar> &buf, size_t offset, size_t size);

AdlerResult adler32(const RChar *buf, size_t offset, size_t size);

AdlerResult rolling_adler32(const std::vector<RChar> &buf, size_t offset, size_t size, const AdlerResult &pre);

std::list<Package> checksum(const std::vector<RChar> &buf, std::forward_list<Chunk> &original, size_t size);

std::forward_list<Chunk> makeChunk(const std::vector<RChar> &data, size_t size);

std::forward_list<Chunk> makeChunkFromFile(const std::string &path, size_t size);

void writeResultToFile(const std::string &sourceFile, const std::string &topath, const std::vector<RChar> &data,
                       const std::list<Package> &result, size_t size);

JsonChunk loadJsonChunks(const std::string& path);
void writeResultToJson(const std::string &path, const std::list<Package> &result, const std::vector<RChar> buf);

#endif //RSYNC_RSYNC_H
