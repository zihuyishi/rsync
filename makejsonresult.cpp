//
// Created by saye on 2019-08-01.
//

#include <iostream>
#include <fstream>
#include <chrono>
#include "rsync.h"

using namespace std;



int main(int argc, const char *argv[]) {
    if (argc < 3) {
        cout << "usage: jsonresult [jsonFile] [newFile]" << endl;
        return 1;
    }

    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::microseconds ms;
    typedef std::chrono::duration<float> fsec;
    auto t0 = Time::now();


    auto sourceFile = argv[1];
    auto newFile = argv[2];
    auto jsonChunk = loadJsonChunks(sourceFile);
    auto size = jsonChunk.size;
    auto result = checksum(newFile, jsonChunk.data, size);
    ofstream os("result.msg", ofstream::binary);
    writeResultToStream(result, newFile, os, size);
    os.close();

    auto t1 = Time::now();
    fsec fs = t1 - t0;
    ms d = std::chrono::duration_cast<ms>(fs);
    cout << "cost " << d.count() / 1000.f << " ms\n";

    return 0;
}