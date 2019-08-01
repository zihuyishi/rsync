#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
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

void writeResult(const string &topath, const vector<RChar> &originFile, const vector<RChar> &buf,
                 const list<Package> &result) {
    ofstream out_file(topath, ofstream::binary);
    assert(out_file.is_open());
    for (const auto &package : result) {
        if (package.type == 1) {
            // chunk
            auto offset = package.chunk.offset;
            out_file.write((char *) (originFile.data() + offset), package.chunk.size);
        } else {
            // data
            auto size = package.data.end - package.data.start;
            out_file.write((char *) buf.data() + package.data.start, size);
        }
    }
    out_file.flush();
    out_file.close();
    cout << "write to file " << topath << endl;
}

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
//    auto buf1 = readFile(file1);
//    cout << "file1 length " << buf1.size() << endl;
    const int size = 2048 * 1024;
    auto chunks = makeChunkFromFile(file1, size);
    cout << "file1 make chunks \n";
//    buf1.clear();
    auto buf2 = readFile(file2);
    cout << "file2 length " << buf2.size() << endl;

    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::microseconds ms;
    typedef std::chrono::duration<float> fsec;
    auto t0 = Time::now();

    auto result = checksum(buf2, chunks, size);

    auto t1 = Time::now();
    fsec fs = t1 - t0;
    ms d = std::chrono::duration_cast<ms>(fs);
    cout << "cost " << d.count() / 1000.f << " ms\n";

//    printPackage(result);
    writeResultToFile(file1, "output", buf2, result, size);
    return 0;
}