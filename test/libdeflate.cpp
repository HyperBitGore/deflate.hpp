#include "../include/deflate.hpp"
#include "../include/inflate.hpp"
#include <cstddef>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include "../build/external/include/libdeflate.h"

struct File {
    char* data;
    size_t size;
    File (size_t size) {
        data = static_cast<char*>(std::malloc(size));
        this->size = size;
    }
    ~File() {
        std::free(data);
    }
};

void writeBufferToFile (char* data, size_t size, std::string file_path) {
    std::ofstream file;
    file.open(file_path, std::ios::binary);
    file.write(data, size);
    file.close();
}

bool sameData (File* f1, File* f2) {
    if (f1->size != f2->size) {
        return false;
    }
    for (size_t i = 0; i < f1->size; i++) {
        if (f1->data[i] != f2->data[i]) {
            std::cout << "failed at index " << i << "\n";
            return false;
        }
    }

    return true;
}

size_t fileSize (std::string name) {
    std::filesystem::path path = name;
    return static_cast<size_t>(std::filesystem::file_size(path));
}

File readFile (std::string name) {
    std::ifstream f;
    f.open(name, std::ios::binary);
    size_t sizef = fileSize(name);
    File file (sizef);
    f.read(file.data, sizef);
    f.close();
    return file;
}

bool testDecompressionFile (std::string path) {
    File file = readFile(path);
    std::cout << path << " is " << file.size << " bytes!\n";
    libdeflate_compressor* compressor = libdeflate_alloc_compressor(1);
    File out_data_lib(file.size + 100);
    //testing the two
    size_t size_lib = libdeflate_deflate_compress(compressor, file.data, file.size, out_data_lib.data, file.size + 100);
    if (size_lib == 0) {
        return false;
    }
    std::cout << "size of libdeflate for " << path << ": " << size_lib << "\n";
    writeBufferToFile(out_data_lib.data, size_lib, path + "libtestdeflate.txt");

    File out_data_inhpp(file.size);
    size_t sizein_hpp = inflate::decompress(out_data_lib.data, size_lib, out_data_inhpp.data, file.size);
    if (sizein_hpp == 0) {
        return false;
    }
    std::cout << "size of inflate.hpp for " << path << ": " << sizein_hpp << "\n";
    writeBufferToFile(out_data_inhpp.data, sizein_hpp, path + "hpptestinflate.bmp");
    bool same = sameData(&out_data_inhpp, &file);
    std::cout << path << " inflate.hpp is the same as original?: " << ((same) ?  "true" : "false")  << "\n"; 

    std::vector<uint8_t> out_data_hpp = deflate::compress(file.data, file.size);
    if (out_data_hpp.size() == 0) {
        return false;
    }
    writeBufferToFile(reinterpret_cast<char*>(out_data_hpp.data()), out_data_hpp.size(), path + "hpptestdeflate.txt");
    std::cout << "size of deflate.hpp for " << path << " : " << out_data_hpp.size() << "\n";
    //decompressing the data
    libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    File out_inflate_lib(file.size);
    size_t out_lib_deflate = 0;
    libdeflate_result result = libdeflate_deflate_decompress(decompressor, out_data_hpp.data(), out_data_hpp.size(), out_inflate_lib.data, file.size, &out_lib_deflate);
    std::cout << "size of libdeflate inflate for " << path << " : " << out_lib_deflate << "\n"; 
    if (result != LIBDEFLATE_SUCCESS) {
        return false;
    }
    writeBufferToFile(out_inflate_lib.data, out_lib_deflate, path + "libtestinflate.bmp");
    // test inflate on hppdeflate
    File out_inflate_hpp_hpp(file.size);
    size_t sizein_hpp_hpp = inflate::decompress(out_data_hpp.data(), out_data_hpp.size(), out_inflate_hpp_hpp.data, file.size);
    std::cout << "size of inflate.hpp (inflate of deflate.hpp) for " << path << ": " << sizein_hpp << "\n";
    if (sizein_hpp_hpp == 0) {
        return false;
    }
    writeBufferToFile(out_inflate_hpp_hpp.data, sizein_hpp_hpp, path + "hpptestinflatehpp.bmp");
    return true;
}

void testDeflateSpeed(std::string path, size_t n) {
    File file = readFile(path);
    double total = 0;
    for (size_t i = 0; i < n; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<uint8_t> out_data_hpp = deflate::compress(file.data, file.size);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        total += elapsed.count();
        std::cout << "hppdeflate execution time: " << elapsed.count() << " ms\n";
    }
    std::cout << "Average: " << (total / n) << "\n";
}

int main () {

    std::cout << "Libdeflate test!\n";
    testDeflateSpeed("test.bmp", 3);
    testDeflateSpeed("large.bmp", 1);
    testDecompressionFile("test.bmp");
    testDecompressionFile("tiny.bmp");
    testDecompressionFile("large.bmp");


    return 0;
}