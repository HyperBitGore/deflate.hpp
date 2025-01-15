#pragma once
#include "common.hpp"
#include <cstddef>
#include <iostream>

/*
In other words, if one were to print out the compressed data as
a sequence of bytes, starting with the first byte at the
*right* margin and proceeding to the *left*, with the most-
significant bit of each byte on the left as usual, one would be
able to parse the result from right to left, with fixed-width
elements in the correct MSB-to-LSB order and Huffman codes in
bit-reversed order (i.e., with the first bit of the code in the
relative LSB position).
*/
//this is best explanation here
//https://stackoverflow.com/questions/32419086/the-structure-of-deflate-compressed-block
//https://github.com/ebiggers/libdeflate/blob/master/lib/decompress_template.h
//https://github.com/Artexety/inflatecpp/blob/main/source/huffman_decoder.cc#L237
//https://github.com/madler/infgen
//https://nevesnunes.github.io/blog/2021/09/29/Decompression-Meddlings.html
//https://blog.za3k.com/understanding-gzip-2/

// I might need to be decoding the tree bits, right to left???
    //header, extra bits LSB-MSB
    //huffman code MSB-LSB

//make sure proper inflation of data
//  -optimize code reading, make one function in bitwrapper???
//  -reduce cycles by reading larger blocks of bits?? min length of bits for tree???

class inflate : deflate_compressor {
    private:

    static FlatHuffmanTree readCodeLengthTree (Bitwrapper& data, uint32_t hclen) {
         std::vector<Code> codes = {
                {0, 0, 2, 16},
                {0, 0, 3, 17},
                {0, 0, 7, 18},
                {0, 0, 0, 0},
                {0, 0, 0, 8},
                {0, 0, 0, 7},
                {0, 0, 0, 9},
                {0, 0, 0, 6},
                {0, 0, 0, 10},
                {0, 0, 0, 5},
                {0, 0, 0, 11},
                {0, 0, 0, 4},
                {0, 0, 0, 12},
                {0, 0, 0, 3},
                {0, 0, 0, 13},
                {0, 0, 0, 2},
                {0, 0, 0, 14},
                {0, 0, 0, 1},
                {0, 0, 0, 15},
            };
        for (uint32_t i = 0; i < hclen + 4; i++) {
                uint32_t val = data.readBits(3);
                codes[i].len = val;
        }
        std::vector<Code> sorted_codes;
       
        return FlatHuffmanTree(codes);
    }

    static std::vector<Code> readDynamicTreeCodes (Bitwrapper& data, FlatHuffmanTree code_len, size_t iterations) {
        std::vector<Code> codes;
        uint32_t code = 0;
        int32_t cur_bit = 0;
        Code last_code = {0, 0, 0, 0};
        for (uint32_t i = 0; i < iterations;) {
            code = (code << 1) | data.readBits(1);
            cur_bit++;
            Code cc = code_len.getCodeEncoded(code, cur_bit);
            if (cc.code == code && cc.len == cur_bit && cc.len > 0) {
                uint32_t repeat;
                std::cout << cc.value << "\n";
                switch (cc.value) {
                    case 16:
                        repeat = data.readBits(2);
                        for (size_t j = 0; j < (repeat + 3); j++, i++) {
                            codes.push_back({0, last_code.value, 0, (uint16_t)i});
                        }
                    break;
                    case 17:
                        repeat = data.readBits(3);
                        for (size_t j = 0; j < (repeat + 3); j++, i++) {
                            codes.push_back({0, 0, 0, (uint16_t)i});
                        }
                    break;
                    case 18:
                        repeat = data.readBits(7);
                        for (size_t j = 0; j < (repeat + 11); j++, i++) {
                            codes.push_back({0, 0, 0, (uint16_t)i});
                        }
                    break;
                    default:
                        codes.push_back({0, cc.value, 0, (uint16_t)i});
                        i++;
                }
                code = 0;
                cur_bit = 0;
                last_code = cc;
            }
        }
        return codes;
    }

    static std::pair<FlatHuffmanTree, FlatHuffmanTree> decodeTree (Bitwrapper& data) {
        uint32_t hlit = (data.readBits(5));
        uint32_t hdist = data.readBits(5);
        uint32_t hclen = data.readBits(4);
        FlatHuffmanTree code_len = readCodeLengthTree(data, hclen);
        std::vector<Code> cod = code_len.decode();

        // literal/length
        std::vector<Code> litlength = readDynamicTreeCodes(data, code_len, 257 + hlit);
        FlatHuffmanTree littree = FlatHuffmanTree(litlength);
        std::vector<Code> lit_codes = littree.decode();
        // distance
        std::vector<Code> distcodes = readDynamicTreeCodes(data, code_len, 1 + hdist);

        FlatHuffmanTree disttree = FlatHuffmanTree(distcodes);
        return std::pair<FlatHuffmanTree, FlatHuffmanTree>(littree, disttree);
    }

    static std::vector<uint8_t> decompressHuffmanBlock (Bitwrapper& data, FlatHuffmanTree tree, FlatHuffmanTree dist_tree) {
        std::vector<uint8_t> buffer;
        uint32_t code = 0;
        uint8_t cur_bit = 0;
        RangeLookup rl = generateLengthLookup();
        RangeLookup dl = generateDistanceLookup();
        while (true) {
            code = (code << 1) | data.readBits(1);
            cur_bit++;
            Code c = tree.getCodeEncoded(code, cur_bit);
            if (c.value < 256 && cur_bit == c.len) {
                buffer.push_back(c.value);
                code = 0;
                cur_bit = 0;
            }
            else if (c.value == 256) {
                break;
            } else if (c.value > 256 && cur_bit == c.len) {
                Range r = rl.findCode(c.value);
                uint32_t length = r.start;
                if (r.extra_bits > 0) {
                    uint32_t extra = data.readBits(r.extra_bits);
                    length += extra;
                }
                // decode dist code
                uint32_t dist_code = 0;
                Code dss;
                for (size_t bits = 0; bits < 16; bits++) {
                    dist_code = (dist_code << 1) | data.readBits(1);
                    Code ds = dist_tree.getCodeEncoded(dist_code, bits+1);
                    if (ds.len == (bits+1)) {
                        dss = ds;
                        break;
                    }
                }
                // read full distance code
                Range d = dl.findCode(dss.value);
                uint32_t distance = d.start;
                if (d.extra_bits > 0) {
                    uint32_t extra = data.readBits(d.extra_bits);
                    distance += extra;
                }
                // copy the uncompressed data here using the distance length pair
                for (size_t i = buffer.size() - distance, j = 0; j < length && i < buffer.size(); j++, i++) {
                    buffer.push_back(buffer[i]);
                }
                cur_bit = 0;
            }

        }

        return buffer;
    }
    public:

    // not done
    static size_t decompress (void* in, size_t in_size, void* out, size_t out_size) {
        Bitwrapper dat = Bitwrapper(in, in_size);
        uint8_t* out_data = (uint8_t*)out;
        //creating default huffman tree
        std::vector <Code> fixed_codes = generateFixedCodes();
        std::vector <Code> fixed_dist_codes = generateFixedDistanceCodes();
        FlatHuffmanTree fixed_huffman(fixed_codes);
        FlatHuffmanTree fixed_dist_huffman(fixed_dist_codes);
        
        uint32_t it = 0;
        uint32_t ot = 0;
        while (true) {
            uint8_t final = dat.readBits(1);
            uint8_t type = dat.readBits(2);
            std::vector<uint8_t> buffer;
            FlatHuffmanTree used_tree = fixed_huffman;
            FlatHuffmanTree used_dist = fixed_dist_huffman;
            switch (type) {
                case 0:
                {
                    dat.moveByte(true);
                    uint16_t len = dat.readBits(16);
                    it += 2;
                    uint16_t nlen = dat.readBits(16);
                    it += 2;
                    for (size_t i = 0; i < len; i++) {
                        buffer.push_back(dat.readByte());
                    }
                }
                break;
                case 2:
                    {
                        std::pair<FlatHuffmanTree, FlatHuffmanTree> trees = decodeTree(dat);
                        used_tree = trees.first;
                        used_dist = trees.second;
                    }
                case 1:
                    buffer = decompressHuffmanBlock(dat, used_tree, used_dist);
                break;
            }
            for (auto& i : buffer) {
                out_data[ot++] = i;
            }
            if (final) {
                break;
            }
            else if (ot >= out_size || it >= in_size) {
                return 0;
            }
        }
        return ot;
    }

        // not done
    static void decompress (std::string file_path, std::string new_file) {
        //creating default huffman tree
        std::vector <Code> fixed_codes = generateFixedCodes();
        std::vector <Code> fixed_dist_codes = generateFixedDistanceCodes();
        FlatHuffmanTree fixed_huffman(fixed_codes);
        FlatHuffmanTree fixed_dist_huffman(fixed_dist_codes);
        //starting parse of file
        std::ofstream nf;
        nf.open(new_file, std::ios::binary);
        std::ifstream f;
        f.open(file_path, std::ios::binary);
        //reading bits
        bool e = true;
        while(e) {
            //read the first three bits
            char c = f.get();
            uint16_t len;
            uint16_t nlen;
            uint8_t bfinal = extract1Bit(c, 0);
            uint8_t btype = (extract1Bit(c, 1) | extract1Bit(c, 2));
            //rest of block parsing
            switch (btype) {
                //uncompressed block
                case 0:
                    //skip remaining bits
                    //read len bytes
                    len = f.get();
                    len |= (f.get() << 8);
                    //read nlen bytes
                    nlen = f.get();
                    // NOLINTNEXTLINE
                    nlen |= (f.get() << 8);
                    //read the rest of uncompressed block
                    for (uint32_t i = 0; i < len; i++) {
                        uint8_t n = f.get();
                        nf << n;
                    }
                break;
                //compressed alphabet
                    //0-255 literals
                    //256 end of block
                    //257-285 length codes
                //parse the blocks with uint16_ts since the values go up to 287, technically but the 286-287 don't participate
                //compressed with fixed huffman codes
                case 1:
                    
                break;
                //compressed with dynamic huffman codes
                case 2:
                
                break;
                //error block (reserved)
                case 3:
                    e = false;
                break;
            }
            //means we finished decoding the final block
            if (bfinal) {
                e = false;
            }
        }

        f.close();
        nf.close();
    }
};