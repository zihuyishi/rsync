#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include "rsync.h"

using namespace std;

void printPackage(const list<Package> &packages) {
    for (const auto &p : packages) {
        if (p.type == 1) {
            cout << "package type is chunk, id " << p.chunk.id << std::endl;
        } else {
            cout << "package type is data, length " << p.data.end - p.data.start << std::endl;
        }
    }
}

int main(int argc, const char *argv[]) {
    if (argc < 3) {
        cout << "usage: rsync [file1] [file2]" << std::endl;
        return 1;
    }
    auto file1 = argv[1];
    auto file2 = argv[2];
    const int size = 2048 * 1024;
    auto chunks = makeChunkFromFile(file1, size);
    cout << "file1 make chunks \n";

    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::microseconds ms;
    typedef std::chrono::duration<float> fsec;
    auto t0 = Time::now();

    auto result = checksum(file2, chunks, size);

    auto t1 = Time::now();
    fsec fs = t1 - t0;
    ms d = std::chrono::duration_cast<ms>(fs);
    cout << "cost " << d.count() / 1000.f << " ms\n";

//    printPackage(result);
    writeResultToFile(file1, "output", file2, result, size);
    return 0;
}