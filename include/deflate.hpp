#pragma once
#include "common.hpp"
#include <cstdint>
#include <iostream>
#include <utility>
#define MAX_LITLEN_CODE_LEN 15
#define MAX_DIST_CODE_LEN 15
#define MAX_PRE_CODE_LEN 7
#define KB32 32768
#define CHAR_BITS 0b111111111
#define LENGTH_BITS 0b11111000000000
#define DISTANCE_BITS 0b11111111111111111100000000000000

// so reading huffman codes we read left to right versus regular data which is the basic right to left bit read
// https://www.rfc-editor.org/rfc/rfc1951#page-6
//https://minitoolz.com/tools/online-deflate-inflate-decompressor/
//https://minitoolz.com/tools/online-deflate-compressor/
//https://github.com/madler/zlib

// https://www.cs.ucdavis.edu/~martel/122a/deflate.html

class deflate : deflate_compressor {
private:

     static uint32_t flipBits (uint32_t value, uint8_t max_bit) {
        uint32_t v = 0;
        for (uint32_t i = 0, j = max_bit; i <= max_bit; i++, j--) {
            v |= extract1Bit(value, i) << (j - 1);
        }
        return v;
    }
    // https://github.com/ebiggers/libdeflate/blob/master/lib/deflate_compress.c
    // https://pzs.dstu.dp.ua/ComputerGraphics/ic/bibl/huffman.pdf
    class CodeMap {
    private:
        uint32_t codes[300];
    public:
        CodeMap () {
            std::memset(codes, 0, sizeof(uint32_t) * 300);
        }
        CodeMap (const CodeMap& c) {
            std::memcpy(codes, c.codes, sizeof(uint32_t) * 300);
        }
        void addOccur (uint32_t code) {
            if (code < 300) {
                codes[code] += 1;
            }
        }
        uint32_t getOccur (uint32_t code) {
            if (code < 300) {
                return codes[code];
            }
            return 0;
        }
        std::vector<Code> generateCodes (uint32_t max_bit_length) {   
            std::vector <PreCode> temp_codes;
            for (uint32_t i = 0; i < 300; i++) {
                uint32_t occurs = getOccur(i);
                if (occurs) {
                    temp_codes.push_back({(int32_t)i, occurs});
                }
            }
            std::vector<Code> out_codes = FlatHuffmanTree::generateCodeLengths(temp_codes, max_bit_length);
            return out_codes;
        }
        FlatHuffmanTree generateTree (uint32_t max_bit_length) {
            std::vector <PreCode> temp_codes;
            for (uint32_t i = 0; i < 300; i++) {
                uint32_t occurs = getOccur(i);
                if (occurs) {
                    temp_codes.push_back({(int32_t)i, occurs});
                }
            }
            std::vector<Code> tree_codes = FlatHuffmanTree::generateCodeLengths(temp_codes, max_bit_length);
            FlatHuffmanTree tree = FlatHuffmanTree(tree_codes);
            return tree;
        }
    };
    class Bitstream {
        private:
            uint8_t bit_offset;
            uint32_t offset;
            std::vector<uint8_t> data;
        public:
            Bitstream () {
                bit_offset = 0;
                offset = 0;
                data.push_back(0);
            }
            // copy constructor
            Bitstream (const Bitstream& b) {
                data = b.data;
                bit_offset = b.bit_offset;
            }
            void addBits (uint32_t val, uint8_t count) {
                for (uint32_t i = 0; i < count; i++, bit_offset++) {
                    if (bit_offset > 7) {
                        data.push_back(0);
                        offset++;
                        bit_offset = 0;
                    }
                    uint8_t bit = extract1Bit(val, i);
                    data[offset] |= (bit << bit_offset);
                }
            }
            void addBitsMSB (uint32_t val, uint8_t count) {
                for (uint32_t i = 0; i < count; i++, bit_offset++) {
                    if (bit_offset > 7) {
                        data.push_back(0);
                        offset++;
                        bit_offset = 0;
                    }
                    uint8_t bit = extract1Bit(val, i);
                    data[offset] |= (bit << (7 - bit_offset));
                }
            }
            uint8_t curBit () {
                return bit_offset;
            }
            void nextByteBoundary () {
                
                data.push_back(0);
                bit_offset = 0;
                offset++;
            }
            void nextByteBoundaryConditional () {
                if (bit_offset != 0) {
                    nextByteBoundary();
                }
            }

            std::vector<uint8_t> getData () {
                return data;
            }
            void addRawBuffer (std::vector<uint8_t> buffer) {
                for (auto& i : buffer) {
                    addBits(i, 8);
                }
            }
            void addRawBuffer (uint8_t buffer[], size_t n) {
                for (size_t i = 0; i < n; i++) {
                    addBits(buffer[i], 8);
                }
            }
            size_t getSize () {
                if (bit_offset == 0) {
                    return data.size() - 1;
                }
                return data.size();
            }
            void copyBitstream(const Bitstream& bs) {
                for(size_t i = 0; i < bs.offset; i++) {
                    addBits(bs.data[i], 8);
                }
                if (bs.bit_offset > 0) {
                    addBits(bs.data[bs.offset], bs.bit_offset);
                }
            }
    };

    
    //https://cs.stanford.edu/people/eroberts/courses/soco/projects/data-compression/lossless/lz77/concept.htm
    //https://en.wikipedia.org/wiki/LZ77_and_LZ78
    // rolling hash time!
    class LZ77 {
        private:
        size_t window_index;
        std::vector<uint32_t> map; // 9 bits for length, 16 for start??
        uint32_t modulo_power(uint32_t base, uint32_t n, uint32_t modulo) {
            uint32_t total = 1;
            for (size_t i = 0; i < n; i++) {
                total = (n * base) % modulo;
            }
            return total;
        }
        uint32_t hashFunc (uint8_t a, uint8_t b, uint8_t c, uint32_t start) {
            uint32_t t = (a * modulo_power(256, start, 512)) + (b * modulo_power(256, start+1, 512))
            + (c * modulo_power(256, start + 2, 512));
            return t % 512;
        }
        public:

        LZ77 (size_t size) {
            window_index = 1;
            for (size_t i = 0; i < size; i++) {
                map.push_back(0);
            }
        }
        // modifies the read_buffer to contain matches lol
        //https://github.com/ebiggers/libdeflate/blob/master/lib/hc_matchfinder.h
        // https://www.youtube.com/watch?v=BfUejqd07yo
        void getMatches (uint32_t read_buffer[], uint8_t raw_buffer[], size_t read_buffer_index, RangeLookup& rl, RangeLookup& dl) {
            const size_t size = read_buffer_index;
            uint32_t* raw_32_buffer = (uint32_t*)raw_buffer;
            while (window_index < size) {
                size_t match_length = 1;
                uint32_t base = 256;
                uint32_t hash = hashFunc(raw_buffer[window_index - 1], raw_buffer[window_index], raw_buffer[window_index + 1], window_index-1);
                for (int32_t i = window_index - 1; i > 0; i--) {
                    uint32_t c = raw_buffer[i];
                    uint32_t w = raw_buffer[window_index];
                    if (c == w) {
                        size_t j = 1;
                        size_t k = i+1;
                        for (; k < size && window_index+j < size && (raw_buffer[k]) == (raw_buffer[window_index+j]) && j < 258; k++, j++);
                        if (j >= match_length && j > 2) {
                            match_length = j;
                            // need to write out char again, 9 bits for lit/length
                            // need to write length extra bits if needed, 5 bits for extra length data
                            // need to write distance value in remaining 18 bits
                            Range r = rl.lookup(j);
                            uint32_t o = j - r.start;
                            uint32_t off = window_index-i;
                            read_buffer[window_index] = (off << 14) | (o << 9) | (r.code & CHAR_BITS);
                            i -= match_length-1;
                        }
                    }
                }
                window_index += match_length;
            }
        }

    };
    
    static void makeUncompressedBlock (Bitstream& bs, uint8_t read_buffer[], size_t read_buffer_index, bool final) {
        uint8_t pre = 0b000;
        if (final) {
            pre |= 1;
        }
        bs.addBits(pre, 3);
        bs.nextByteBoundaryConditional();
        bs.addBits(read_buffer_index, 16);
        bs.addBits(~(read_buffer_index), 16);
        bs.addRawBuffer(read_buffer, read_buffer_index);
    }

    // this is segfaulting on multi chunk files
    static std::pair<FlatHuffmanTree, FlatHuffmanTree> constructDynamicHuffmanTree (uint32_t read_buffer[], size_t read_buffer_index, RangeLookup& rl, RangeLookup& dl) {
        CodeMap c_map;
        c_map.addOccur(256);
        CodeMap dist_codes;
        for (uint32_t i = 0; i < read_buffer_index; i++) {
            uint32_t car = (read_buffer[i] & CHAR_BITS);
            uint32_t dist = (read_buffer[i] & DISTANCE_BITS) >> 14;
            c_map.addOccur(car);
            if (dist > 0) {
                Range dran = dl.lookup(dist);
                dist_codes.addOccur(dran.code);
            }
        }
        FlatHuffmanTree tree = c_map.generateTree(15);
        FlatHuffmanTree dist_tree = dist_codes.generateTree(15);
        return std::pair<FlatHuffmanTree, FlatHuffmanTree> (tree, dist_tree);
    }
    static uint32_t countRepeats (std::vector<uint8_t>& bytes, uint32_t index) {
        uint32_t count = 0;
        for (uint32_t i = index + 1; i < bytes.size(); i++) {
            if (bytes[i] == bytes[index]) {
                count++;
            } else {
                return count;
            }
        }
        return count;
    }
    static FlatHuffmanTree constructDynamicHuffmanCodeLengthTree (std::vector<uint8_t>& bytes, std::vector<Code>& codes) {
        CodeMap cm;
        for (uint32_t i = 0; i < bytes.size();) {
            uint32_t reps = countRepeats(bytes, i);
            if (reps > 2) {
                if (bytes[i] == 0) {
                    if (reps <= 10) {
                        cm.addOccur(17);
                        i += reps + 1;
                    } else {
                        cm.addOccur(18);
                        i += (reps <= 138) ? reps + 1 : 138;
                    }
                } else {
                    cm.addOccur(bytes[i]);
                    cm.addOccur(16);
                    i += (reps <= 6) ? reps + 1 : 6;
                }
            } else {
                cm.addOccur(bytes[i]);
                i++;
            }
        }
        std::vector<Code> t_codes = cm.generateCodes(MAX_PRE_CODE_LEN);
        std::vector <Code> test_codes;
        test_codes.push_back({16, 7, 0, 16});
        test_codes.push_back({17, 3, 0, 17});
        test_codes.push_back({18, 6, 0, 18});
        test_codes.push_back({0, 3, 0, 0});
        test_codes.push_back({8, 4, 0, 8});
        test_codes.push_back({7, 4, 0, 7});
        test_codes.push_back({9, 3, 0, 9});
        test_codes.push_back({6, 3, 0, 6});
        test_codes.push_back({10, 3, 0, 10});
        test_codes.push_back({5, 4, 0, 5});
        test_codes.push_back({11, 3, 0, 11});
        test_codes.push_back({4, 5, 0, 4});
        test_codes.push_back({2, 7, 0, 2});
        for (auto& i : codes) {
            for (auto& j : t_codes) {
                if (i.value == j.value) {
                    i.code = j.code;
                    i.len = j.len;
                    j.extra_bits = i.extra_bits;
                }
            }
        }
        return FlatHuffmanTree(codes);
    }
    static void writeDynamicHuffmanBytes (std::vector<Code> t_codes, std::vector<uint8_t>& bytes, size_t max) {
        struct {
            inline bool operator() (const Code& code1, const Code& code2) {
                return code1.value < code2.value;
            }
        } compare_code_value;
        std::sort(t_codes.begin(), t_codes.end(), compare_code_value);
        for (uint32_t i = 0; i < max; i++) {
            bool write = false;
            for (auto& j : t_codes) {
                // have to check contains the value
                if (((uint32_t)j.value) == i) {
                    bytes.push_back(j.len);
                    write = true;
                    break;
                }
            } 
            if (!write) {
                bytes.push_back(0);
            }
        }
    }
    // need to be able to repeat two in a row, so no break ups (look at infgen output for more detail)
    static void compressDynamicHuffmanTreeCodes (std::vector<uint8_t>& bytes, Bitstream& bs, FlatHuffmanTree& code_tree) {
        for (uint32_t i = 0; i < bytes.size();) {
            uint32_t reps = countRepeats(bytes, i);
            Code c;
            uint32_t inc = 0;
            if (bytes[i] != 0 || (reps <= 2 && bytes[i] == 0)) {
                c = code_tree.getCodeValue(bytes[i]);
                bs.addBits(flipBits(c.code, c.len), c.len);
                inc = 1;
            }
            if (reps > 2) {
                uint32_t reps_code;
                uint32_t real_reps;
                if (bytes[i] == 0) {
                    if (reps <= 10) {
                        reps_code = 17;
                        inc += (reps < 10) ? reps + 1 : 10;
                        real_reps = (reps < 10) ? reps + 1 : 10;
                    } else {
                        reps_code = 18;
                        inc += (reps < 138) ? reps + 1 : 138;
                        real_reps = (reps < 138) ? reps + 1 : 138;
                    }
                } else {
                    reps_code = 16;
                    inc += (reps < 6) ? reps : 6;
                    real_reps = (reps < 6) ? reps : 6;
                }
                c = code_tree.getCodeValue(reps_code);
                bs.addBits(flipBits(c.code, c.len), c.len);
                if (c.extra_bits > 0) {
                    uint32_t start = 3;
                    if (c.value == 18) {
                        start = 11;
                    }
                    uint32_t val = (real_reps) - start;
                    bs.addBits(val, c.extra_bits);
                }
            }
            i += inc;
        }
    }
    static void writeDynamicHuffmanTree (Bitstream& bs, FlatHuffmanTree tree, FlatHuffmanTree dist_tree, RangeLookup rl, RangeLookup dl) {
        // calculating how much above 257 to go for matches
        std::vector<Code> huff_codes = tree.decode();
        uint32_t matches_size = 257;
        for (size_t i = 0; i < huff_codes.size(); i++) {
            if (huff_codes[i].value > matches_size) {
                matches_size = huff_codes[i].value;
            }
        }
        matches_size = matches_size - 257 + 1;
        // HLIT
        bs.addBits(matches_size, 5);

        // calculating how big the distance needs to be
        std::vector<Code> dist_codes = dist_tree.decode();
        uint32_t dist_codes_size = 0;
        if (dist_codes.size() > 0) {
            dist_codes_size = dist_codes[0].value;
            for (auto& i : dist_codes) {
                if (i.value > dist_codes_size) {
                    dist_codes_size = i.value;
                }
            }
        }
        // HDIST
        bs.addBits(dist_codes_size, 5);
        // compress the code lengths and use that to generate the HCLEN
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
        std::vector<uint8_t> bytes;
        std::vector<uint8_t> huff_bytes;
        // constructing raw bytes of the table
        writeDynamicHuffmanBytes(huff_codes, huff_bytes, 257 + matches_size);
        std::vector<uint8_t> dist_bytes;
        writeDynamicHuffmanBytes(dist_codes, dist_bytes, 1 + dist_codes_size);
        for (auto& i : huff_bytes) {
            bytes.push_back(i);
        }
        for (auto& i : dist_bytes) {
            bytes.push_back(i);
        }
        // constructing the code length huffman tree
        // 3 bits for each code length
        // start by computing how often each code length character will be used
        FlatHuffmanTree code_tree = constructDynamicHuffmanCodeLengthTree(bytes, codes);
        // write the code length tree
        uint32_t max = 0;
        for (int32_t i = codes.size() - 1; i >= 0; i--) {
            if (codes[i].len > 0) {
                max = i;
                break;
            }
        }
        // HCLEN
        bs.addBits(max - 3, 4);       
        for (uint32_t i = 0; i <= max; i++) {
            bs.addBits(codes[i].len, 3);
        }
        // bs.nextByteBoundaryConditional();
        // compressed separate, not treated as one run of bits
        // main huffman tree
        compressDynamicHuffmanTreeCodes(huff_bytes, bs, code_tree) ;
        //  dist tree
        compressDynamicHuffmanTreeCodes(dist_bytes, bs, code_tree);
    }

    // deflate

    static Bitstream compressBuffer (uint32_t read_buffer[], size_t read_buffer_index, FlatHuffmanTree tree, FlatHuffmanTree dist_tree, uint32_t preamble, bool final, RangeLookup& rl, RangeLookup& dl) {
        // compress into huffman code format
        Bitstream bs;
        // writing the block out to file
        uint32_t pre = (final) ? (preamble | 0b001) : preamble; 
        bs.addBits(pre, 3);
        if ((preamble & 0b100) == 4) {
            writeDynamicHuffmanTree(bs, tree, dist_tree, rl, dl);
        }
        for (uint32_t i = 0; i < read_buffer_index;) {
            uint32_t car = (read_buffer[i] & CHAR_BITS);
            if (car > 256) {
                // process length and distance pair
                Range r_len = rl.findCode(car);
                uint32_t len = r_len.start;
                Code c = tree.getCodeValue(car);
                bs.addBits(flipBits(c.code, c.len), c.len);
                // add extra bits from the read_buffer
                if (r_len.extra_bits > 0) {
                    uint32_t extra_bits = (read_buffer[i] & LENGTH_BITS) >> 9;
                    len += extra_bits;
                    bs.addBits(extra_bits, r_len.extra_bits);
                }
                // distance
                uint32_t dist = (read_buffer[i] & DISTANCE_BITS) >> 14;
                Range r_dist = dl.lookup(dist);
                Code d_code = dist_tree.getCodeValue(r_dist.code);
                bs.addBits(flipBits(d_code.code, d_code.len), d_code.len);
                if (r_dist.extra_bits > 0) {
                    uint32_t extra_bits = dist % r_dist.start;
                    bs.addBits(extra_bits, r_dist.extra_bits);
                }
                i += len;
            } else {
                Code c = tree.getCodeValue(car);
                bs.addBits(flipBits(c.code, c.len), c.len);
                i++; 
            }
        }
        Code endcode = tree.getCodeValue(256);
        bs.addBits(flipBits(endcode.code, endcode.len), endcode.len);
        return bs;
    }
public:
    // not done
    static size_t compress (std::string file_path, std::string new_file) {
       /* std::streampos sp = getFileSize(file_path);
        std::ifstream fi;
        fi.open(file_path, std::ios::binary);
        if (!fi) {
            return 0;
        }
        std::ofstream of;
        of.open(new_file, std::ios::binary);
        if (!of) {
            return 0;
        }
        FlatHuffmanTree fixed_dist_huffman(generateFixedDistanceCodes());
        FlatHuffmanTree fixed_huffman(generateFixedCodes());
        uint8_t c;
        bool q = false;
        std::vector<uint8_t> buffer;
        size_t out_index = 0;
        while (!q) {
            std::streampos p = sp - fi.tellg();
            if (p > 0 && buffer.size() < KB32) {
                c = fi.get();
                buffer.push_back(c);
            } else {
                // writing the block out to file
                if (p <= 0) {
                    q = true;
                }
                LZ77 lz(KB32);
                // finding the matches above length of 2
                lz.getMatches(buffer);
                std::sort(matches.begin(), matches.end(), match_index_comp);

                std::vector<uint8_t> output_buffer;
                std::pair<FlatHuffmanTree, FlatHuffmanTree> trees = constructDynamicHuffmanTree(buffer, matches);

                Bitstream bs_fixed = compressBuffer(buffer, matches, fixed_huffman, fixed_dist_huffman, 0b001, q);
                Bitstream bs_dynamic = compressBuffer(buffer, matches, trees.first, trees.second, 0b010, q);
                if (bs_fixed.getSize() < (buffer.size() + 5) && bs_fixed.getSize() < bs_dynamic.getSize()) {
                    output_buffer = bs_fixed.getData();
                } else if (bs_dynamic.getSize() < (buffer.size() + 5)) {
                    output_buffer = bs_dynamic.getData();
                } else {
                    // output_buffer = makeUncompressedBlock(buffer, q).getData();
                }
                // compare size of bs
                for (auto& i : output_buffer) {
                    of << i;
                    out_index++;
                }
                buffer.clear();
            }
        }

        fi.close();
        of.close();*/
        return 0;
    }

    static std::vector<uint8_t> compress (char* data, size_t data_size) {
        FlatHuffmanTree fixed_dist_huffman(generateFixedDistanceCodes());
        FlatHuffmanTree fixed_huffman(generateFixedCodes());
        RangeLookup rl = generateLengthLookup();
        RangeLookup dl = generateDistanceLookup();
        bool q = false;
        size_t index = 0;
        Bitstream out_stream;

        size_t read_buffer_index = 0;
        uint32_t read_buffer[131072];
        uint8_t raw_buffer[32768];
        std::memset(read_buffer, 0, 131072);
         while (!q) {
            for (; index < data_size && read_buffer_index < KB32; index++, read_buffer_index++) {
                read_buffer[read_buffer_index] = ((uint32_t)data[index]) & 0xff;
                raw_buffer[read_buffer_index] = data[index];
            }
            // writing the block out to file
            if (index >= data_size) {
                q = true;
            }
            LZ77 lz(KB32);
            // finding the matches above length of 2
            lz.getMatches(read_buffer, raw_buffer, read_buffer_index, rl, dl);

            std::pair<FlatHuffmanTree, FlatHuffmanTree> trees;
            bool set_fixed = false;
            try {
                trees = constructDynamicHuffmanTree(read_buffer, read_buffer_index, rl, dl);
            } catch (std::runtime_error e) {
                std::cout << "Dynamic tree oversubscribed!\n";
                set_fixed = true;
            }
            
            Bitstream bs_dynamic;
            Bitstream bs_fixed = compressBuffer(read_buffer, read_buffer_index, fixed_huffman, fixed_dist_huffman, 0b010, q, rl, dl);
            if (!set_fixed) {
                try {
                    bs_dynamic = compressBuffer(read_buffer, read_buffer_index, trees.first, trees.second, 0b100, q, rl, dl);
                } catch (std::runtime_error e) {
                    std::cout << "Dynamic tree oversubscribed!\n";
                    set_fixed = true;
                }
            }
            if (bs_fixed.getSize() < (read_buffer_index + 5) && (!set_fixed && bs_fixed.getSize() <= bs_dynamic.getSize())) {
                out_stream.copyBitstream(bs_fixed);
            } else if (!set_fixed && bs_dynamic.getSize() < (read_buffer_index + 5)) {
                out_stream.copyBitstream(bs_dynamic);
            } else {
                makeUncompressedBlock(out_stream, raw_buffer, read_buffer_index, q);
            }
            read_buffer_index = 0;
        }
        return out_stream.getData();
    }
};