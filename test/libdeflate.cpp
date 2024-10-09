#include "../include/deflate.hpp"
#include "../include/inflate.hpp"
#include <libdeflate.h>


void writeBufferToFile (char* data, size_t size, std::string file_path) {
    std::ofstream file;
    file.open(file_path, std::ios::binary);
    for (size_t i = 0; i < size; i++) {
        file << data[i];
    }
    file.close();
}



int main () {

    std::cout << "Libdeflate test!\n";

    std::ifstream file;
    file.open("test.bmp", std::ios::binary);

    if (!file) {
        return -1;
    }
    char data[32490];
    file.read(data, 32490);

    size_t sizef = file.gcount();
    libdeflate_compressor* compressor = libdeflate_alloc_compressor(1);
    char out_data_lib[32490];
    //testing the two
    size_t size_lib = libdeflate_deflate_compress(compressor, data, sizef, out_data_lib, 32490);
    std::cout << "size of libdeflate: " << size_lib << "\n";

    char out_data_inhpp[32490];

    size_t sizein_hpp = inflate::decompress(out_data_lib, size_lib, out_data_inhpp, 32490);
    std::cout << "size of inflate.hpp: " << sizein_hpp << "\n";
    writeBufferToFile(out_data_inhpp, sizein_hpp, "hpptestinflate.bmp");
    char out_data_hpp[32490];
    size_t size_hpp = deflate::compress(data, sizef, out_data_hpp, 32490);
    std::cout << "size of deflate.hpp: " << size_hpp << "\n";
    //decompressing the data
    libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    char out_inflate_lib[32490];
    libdeflate_result result = libdeflate_deflate_decompress(decompressor, out_data_hpp, size_hpp, out_inflate_lib, 32490, NULL);


    return 0;
}