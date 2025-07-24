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
        // Copy constructor
    File(const File& other) {
        size = other.size;
        data = static_cast<char*>(std::malloc(size));
        if (!data) throw std::runtime_error("Copy allocation failed");
        std::memcpy(data, other.data, size);
    }

    // Move constructor
    File(File&& other) noexcept {
        data = other.data;
        size = other.size;
        other.data = nullptr;
        other.size = 0;
    }

    // Copy assignment
    File& operator=(const File& other) {
        if (this != &other) {
            std::free(data);
            size = other.size;
            data = static_cast<char*>(std::malloc(size));
            if (!data) throw std::runtime_error("Copy assignment allocation failed");
            std::memcpy(data, other.data, size);
        }
        return *this;
    }

    // Move assignment
    File& operator=(File&& other) noexcept {
        if (this != &other) {
            std::free(data);
            data = other.data;
            size = other.size;
            other.data = nullptr;
            other.size = 0;
        }
        return *this;
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
            std::cerr << "failed at index " << i << "\n";
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
    if (!f) {
        throw std::runtime_error("Failed to read file " + name);
    }
    size_t sizef = fileSize(name);
    File file (sizef);
    f.read(file.data, sizef);
    f.close();
    return file;
}

bool testDecompressionFile (std::string path, bool compression_higher) {
    File file = readFile(path);
    std::cerr << path << " is " << file.size << " bytes!\n";
    libdeflate_compressor* compressor = libdeflate_alloc_compressor(1);
    File out_data_lib(file.size + 100);
    //testing the two
    size_t size_lib = libdeflate_deflate_compress(compressor, file.data, file.size, out_data_lib.data, file.size + 100);
    if (size_lib == 0) {
        return false;
    }
    std::cerr << "size of libdeflate for " << path << ": " << size_lib << "\n";
    writeBufferToFile(out_data_lib.data, size_lib, path + "libtestdeflate.txt");

    File out_data_inhpp(file.size);
    size_t sizein_hpp = inflate::decompress(out_data_lib.data, size_lib, out_data_inhpp.data, file.size);
    if (sizein_hpp == 0) {
        return false;
    }
    std::cerr << "size of inflate.hpp for " << path << ": " << sizein_hpp << "\n";
    writeBufferToFile(out_data_inhpp.data, sizein_hpp, path + "hpptestinflate.bmp");
    bool same = sameData(&out_data_inhpp, &file);
    std::cerr << path << " inflate.hpp is the same as original?: " << ((same) ?  "true" : "false")  << "\n"; 

    std::vector<uint8_t> out_data_hpp = deflate::compress(file.data, file.size, compression_higher);
    if (out_data_hpp.size() == 0) {
        return false;
    }
    writeBufferToFile(reinterpret_cast<char*>(out_data_hpp.data()), out_data_hpp.size(), path + "hpptestdeflate.txt");
    std::cerr << "size of deflate.hpp for " << path << " : " << out_data_hpp.size() << "\n";
    //decompressing the data
    libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    File out_inflate_lib(file.size*2);
    size_t out_lib_deflate = 0;
    libdeflate_result result = libdeflate_deflate_decompress(decompressor, out_data_hpp.data(), out_data_hpp.size(), out_inflate_lib.data, file.size*2, &out_lib_deflate);
    std::cerr << "size of libdeflate inflate for " << path << " : " << out_lib_deflate << "\n"; 
    if (result != LIBDEFLATE_SUCCESS) {
        return false;
    }
    writeBufferToFile(out_inflate_lib.data, out_lib_deflate, path + "libtestinflate.bmp");
    // test inflate on hppdeflate
    File out_inflate_hpp_hpp(file.size);
    size_t sizein_hpp_hpp = inflate::decompress(out_data_hpp.data(), out_data_hpp.size(), out_inflate_hpp_hpp.data, file.size);
    std::cerr << "size of inflate.hpp (inflate of deflate.hpp) for " << path << ": " << sizein_hpp << "\n";
    if (sizein_hpp_hpp == 0) {
        return false;
    }
    writeBufferToFile(out_inflate_hpp_hpp.data, sizein_hpp_hpp, path + "hpptestinflatehpp.bmp");
    return true;
}

void testDeflateSpeed(std::string path, size_t n, bool compress_better) {
    File file = readFile(path);
    double total = 0;
    for (size_t i = 0; i < n; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<uint8_t> out_data_hpp = deflate::compress(file.data, file.size, compress_better);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        total += elapsed.count();
        std::cerr << "hppdeflate execution time: " << elapsed.count() << " ms\n";
    }
    std::cerr << "Average: " << (total / n) << " ms\n";
}

void testInflateSpeed (std::string path, size_t n) {
    File file = readFile(path);
    double total = 0;
    for (size_t i = 0; i < n; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> out_data_hpp = inflate::decompress(file.data, file.size);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        total += elapsed.count();
        std::cerr << "hppinflate execution time: " << elapsed.count() << " ms\n";
    }
    std::cerr << "Average: " << (total / n) << " ms\n";
}

void compareInflateLibVector (std::string path) {
    File file = readFile(path);
    libdeflate_compressor* compressor = libdeflate_alloc_compressor(1);
    File out_data_lib(file.size + 100);
    //testing the two
    size_t size_lib = libdeflate_deflate_compress(compressor, file.data, file.size, out_data_lib.data, file.size + 100);
    std::vector<uint8_t> decomp = inflate::decompress(out_data_lib.data, size_lib);
    for (size_t i = 0; i < decomp.size(); i++) {
        uint8_t f = file.data[i];
        uint8_t d = decomp[i];
        if (f != d) {
            std::cerr << "inflate.hpp was different from original file: " << i << " , " << path << "\n";
            return;
        }
    }
    std::cerr << "inflate.hpp was the same as original file!\n";
    std::cerr << path << "\n";
}

void testInflateZlibFile (std::string path) {
    File file = readFile(path);
    std::vector<uint8_t> decomp = inflate::decompressZlib(file.data, file.size);
    libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    File lib(decomp.size());
    size_t ret = 0;
    libdeflate_result result = libdeflate_deflate_decompress(decompressor, file.data + 2, file.size - 2, lib.data, lib.size, &ret);
    if (result != LIBDEFLATE_SUCCESS) {
        std::cerr << "inflate file failed " << result << "\n";
        return;
    }
    for (size_t i = 0; i < ret && i < decomp.size(); i++) {
        if (decomp[i] != (uint8_t)lib.data[i]) {
            std::cerr << "inflate file failed!\n";
            std::cerr << i << ", " << path << "\n";
            return;
        }
    }
    std::cerr << "inflate file matched!\n";
    std::cerr << path << "\n";
}

// inflate issue was the file reader not having a copy construcor!
// deflate copy constructor offset was wrong!

int main () {
    std::cerr << "Libdeflate test!\n";

    compareInflateLibVector("test.bmp");
    testInflateZlibFile("weird.dat");
    /* testDeflateSpeed("test.bmp", 10, false);
    testDeflateSpeed("test.bmp", 1, true);
    testDeflateSpeed("large.bmp", 1, false); */
    testDecompressionFile("test.bmp", false);
    testDecompressionFile("tiny.bmp", false);
    testDecompressionFile("test.bmp", true);
    testDecompressionFile("tiny.bmp", true);
    testDecompressionFile("large.bmp", false);
    deflate::compress("test.bmp", "hppdeflate_testbmp", true);
    inflate::decompress("hppdeflate_testbmp", "hppinflate_test.bmp");
    testInflateSpeed("large.bmplibtestdeflate.txt", 1);
    return 0;
}