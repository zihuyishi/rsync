//
// Created by saye on 2019/8/26.
//
#include <iostream>
#include <string>
#include <fstream>

using namespace std;

int main(int argc, const char **argv) {
    if (argc != 3) {
        cout << "usage: div [file] size" << endl;
        return 1;
    }
    const char *inputFile = argv[1];
    const char *strSize = argv[2];
    const int size = strtol(strSize, nullptr, 10);
    ifstream infile(inputFile, ifstream::binary);

    int count = 0;
    char *buf = new char[size+1];
    while (!infile.eof()) {
        infile.read(buf, size);
        auto realSize = infile.gcount();
        char topath[40];
        sprintf(topath, "output_%d", count);
        count++;
        ofstream outfile(topath, ofstream::binary);
        outfile.write(buf, realSize);
        outfile.close();
    }
    infile.close();
    delete[] buf;
    return 0;
}