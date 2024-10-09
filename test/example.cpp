#include "../include/deflate.hpp"
#include "../include/inflate.hpp"

int main(){
    /* HuffmanTree hf;
    hf.encode(
        {{'a', 3}, {'b', 3}, {'c', 3}, {'d', 3}, {'e', 3}, {'f', 2}, {'g', 4}, {'h', 4}}
        ); */
    std::ofstream of;
    of.open("uncompressed.test", std::ios::binary);
    //first byte
    uint8_t b = 1;
    of.write(reinterpret_cast<char*>(&b), sizeof(uint8_t));
    //len = 100 and nlen = 27
    uint32_t p = (100) | (27 << 16); 
    of.write(reinterpret_cast<char*>(&p), sizeof(uint32_t));
    for (uint8_t i = 0; i < 100; i++) {
        of << i;
    }
    of.close();
    inflate::decompress("uncompressed.test", "uncompressed.parsed");

    deflate::compress("lz.test", "lz.compressed");
    
    deflate::compress("Test-export.txt", "Test-export.comp");

}
