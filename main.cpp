#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include "rsync.h"

using namespace std;

vector<uint8_t> readFile(const string& path) {
    ifstream in_file(path, ifstream::binary);
    auto vec = vector<uint8_t>();
    const int size = 1024;
    auto buf = new char[size];
    assert(in_file.is_open());
    while (!in_file.eof()) {
        in_file.read(buf, size);
        vec.insert(vec.end(), buf, buf + in_file.gcount());
    }
    delete[] buf;
    in_file.close();
    return vec;
}

void printPackage(const vector<Package>& packages) {
    for (const auto& p : packages) {
        if (p.type == 1) {
            cout << "package type is chunk, id " << p.chunk.id << std::endl;
        } else {
            cout << "package type is data, length " << p.data.size() << std::endl;
        }
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 3) {
        cout << "usage: rsync [file1] [file2]" << std::endl;
        return 1;
    }
    auto file1 = argv[1];
    auto file2 = argv[2];
    auto buf1 = readFile(file1);
    const int size = 2048;
    auto chunks = makeChunk(buf1, size);
    buf1.clear();
    auto buf2 = readFile(file2);

    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::microseconds ms;
    typedef std::chrono::duration<float> fsec;
    auto t0 = Time::now();

    auto result = checksum(buf2, chunks, size);

    auto t1 = Time::now();
    fsec fs = t1 - t0;
    ms d = std::chrono::duration_cast<ms>(fs);
    cout << "cost " << d.count() << " microsecond\n";

    printPackage(result);
    return 0;
}