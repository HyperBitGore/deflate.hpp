#pragma once
#include "common.hpp"



//decode dynamic tree
//decode length distance pairs
//make sure proper inflation of data

class inflate : deflate_compressor {
    private:

    static HuffmanTree readCodeLengthTree (Bitwrapper& data, uint32_t hclen) {
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
        std::vector<Code> pruned_codes;
        for (auto& i : codes) {
            if (i.len > 0) {
                pruned_codes.push_back(i);
            }
        }
        return HuffmanTree(pruned_codes);
    }

    static std::pair<HuffmanTree, HuffmanTree> decodeTree (Bitwrapper& data) {
        std::vector<Code> codes;
        std::vector<Code> dist_codes;
        uint32_t hlit = (data.readBits(5));
        uint32_t hdist = data.readBits(5);
        uint32_t hclen = data.readBits(4);
        HuffmanTree code_len = readCodeLengthTree(data, hclen);
        std::vector<Code> cod = code_len.decode();

        for (uint32_t i = 0; i < 257 + hlit; i++) {
            
        }
        return std::pair<HuffmanTree, HuffmanTree>();
    }

    static std::vector<uint8_t> decompressHuffmanBlock (Bitwrapper& data, HuffmanTree tree, HuffmanTree dist_tree) {
        std::vector<uint8_t> buffer;
        uint32_t code = 0;
        uint8_t cur_bit = 0;
        while (true) {
            code |= data.readBits(1) << cur_bit;
            cur_bit++;
            Code c = tree.getUncompressedCode(code);
            if (c.value < 256 && cur_bit == c.len) {
                buffer.push_back(c.value);
                code = 0;
                cur_bit = 0;
            }
            else if (c.value == 256) {
                break;
            } else {
                // distance code processing
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
        HuffmanTree fixed_huffman(fixed_codes);
        HuffmanTree fixed_dist_huffman(fixed_dist_codes);
        
        uint32_t it = 0;
        uint32_t ot = 0;
        while (true) {
            uint8_t final = dat.readBits(1);
            uint8_t type = dat.readBits(2);
            std::vector<uint8_t> buffer;
            HuffmanTree used_tree = fixed_huffman;
            HuffmanTree used_dist = fixed_dist_huffman;
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
                        std::pair<HuffmanTree, HuffmanTree> trees = decodeTree(dat);
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
        HuffmanTree fixed_huffman(fixed_codes);
        HuffmanTree fixed_dist_huffman(fixed_dist_codes);
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