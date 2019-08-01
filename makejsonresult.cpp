//
// Created by saye on 2019-08-01.
//

#include <iostream>
#include <fstream>
#include "rsync.h"

using namespace std;

vector<RChar> readFile(const string &path) {
    ifstream in_file(path, ifstream::binary);
    assert(in_file.is_open());
    auto vec = vector<RChar>();
    const int size = 1024 * 1024;
    auto buf = new char[size];
    while (!in_file.eof()) {
        in_file.read(buf, size);
        vec.insert(vec.end(), buf, buf + in_file.gcount());
    }
    delete[] buf;
    in_file.close();
    return vec;
}

int main(int argc, const char *argv[]) {
    if (argc < 3) {
        cout << "usage: jsonresult [jsonFile] [newFile]" << endl;
        return 1;
    }

    auto sourceFile = argv[1];
    auto newFile = argv[2];
    auto jsonChunk = loadJsonChunks(sourceFile);
    auto size = jsonChunk.size;
    auto buf2 = readFile(newFile);
    auto result = checksum(buf2, jsonChunk.data, size);
    writeResultToJson("result.json", result, buf2);
    return 0;
}