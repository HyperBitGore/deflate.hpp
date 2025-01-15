#include "../include/deflate.hpp"
#include "../include/inflate.hpp"
#include <fstream>
#include <iostream>
#include "../build/external/include/libdeflate.h"


void writeBufferToFile (char* data, size_t size, std::string file_path) {
    std::ofstream file;
    file.open(file_path, std::ios::binary);
    for (size_t i = 0; i < size; i++) {
        file << data[i];
    }
    file.close();
}

char data[32490];
char tiny_data[500];
char out_data_lib[32490];
char out_data_lib_tiny [500];
char out_data_inhpp[32490];
char out_data_hpp[32490];
char out_inflate_lib[32490];

size_t readFile (std::string name, char* c, size_t size) {
    std::ifstream f;
    f.open(name, std::ios::binary);
    f.read(c, size);
    size_t sizef = static_cast<size_t>(f.gcount());
    f.close();
    return sizef;
}

int main () {

    std::cout << "Libdeflate test!\n";

    size_t sizef = readFile("test.bmp", data, 32490);
    size_t sizetiny = readFile("tiny.bmp", tiny_data, 500);
    libdeflate_compressor* compressor = libdeflate_alloc_compressor(1);
    //testing the two
    size_t size_lib = libdeflate_deflate_compress(compressor, data, sizef, out_data_lib, 32490);
    std::cout << "size of libdeflate: " << size_lib << "\n";
    writeBufferToFile(out_data_lib, size_lib, "libtestdeflate.txt");
    // to get fixed huffman used
    size_t size_lib_tiny = libdeflate_deflate_compress(compressor, tiny_data, sizetiny, out_data_lib_tiny, 500);
    writeBufferToFile(out_data_lib_tiny, size_lib_tiny, "libtestdeflate_tiny.txt");

    size_t sizein_hpp = inflate::decompress(out_data_lib, size_lib, out_data_inhpp, 32490);
    std::cout << "size of inflate.hpp: " << sizein_hpp << "\n";
    writeBufferToFile(out_data_inhpp, sizein_hpp, "hpptestinflate.bmp");

    size_t size_hpp = deflate::compress(data, sizef, out_data_hpp, 32490);
    std::cout << "size of deflate.hpp: " << size_hpp << "\n";
    //decompressing the data
    libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    char out_inflate_lib[32490];
    libdeflate_result result = libdeflate_deflate_decompress(decompressor, out_data_hpp, size_hpp, out_inflate_lib, 32490, NULL);


    return 0;
}