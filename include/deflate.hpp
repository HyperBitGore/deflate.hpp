#pragma once
#include "common.hpp"
#include <cstdint>
#include <utility>
#include <vector>
#ifdef DEBUG
#include <iostream>
#include <chrono>
#endif
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
                // see how much of current byte we can shove from val
                for (int32_t i = count; i > 0;) {
                    uint32_t remaining = 8 - bit_offset;
                    uint32_t change = ((i) < remaining) ? i : remaining;
                    // get bit mask for remaining bits to be written
                    uint32_t mask = (1 << change) - 1;
                    // mask bits from val
                    data[offset] |= ((val & mask) << bit_offset);
                    // move value down corresponding to bits removed
                    val >>= change;
                    i -= change;
                    bit_offset += change;
                    if (bit_offset > 7) {
                        data.push_back(0);
                        offset++;
                        bit_offset = 0;
                    }
                }
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
            void clear () {
                bit_offset = 0;
                offset = 0;
                data.clear();
            }
            uint8_t getBits () {
                return bit_offset;
            }
    };
    class BitFile {
    private:
        Bitstream bs;
        std::ofstream out_file;
    public:
        BitFile (std::string file) {
            out_file.open(file.c_str(), std::ios::binary);
        }
        ~BitFile() {
            out_file.close();
        }
        void writeFile () {
            std::vector<uint8_t> c = bs.getData();
            out_file.write((char*)c.data(), c.size());
            bs.clear();
        }
        void addBitStream (const Bitstream& b) {
            bs.copyBitstream(b);
            if (bs.getBits() == 0) {
                writeFile();
            } 
        }
    };
    
    //https://cs.stanford.edu/people/eroberts/courses/soco/projects/data-compression/lossless/lz77/concept.htm
    //https://en.wikipedia.org/wiki/LZ77_and_LZ78
    // hash every four bytes
    // insert hashes into big ass table
    // when look in hash discover if pattern already seen
    // if seen compare length, then match
    class LZ77 {
        private:
        size_t window_index;
        struct member {
            uint32_t data;  // 9 bits for length, 16 for start
            int32_t next; // index of next member in bucket
        };
        std::vector<member> map; // 9 bits for length, 16 for start
        inline uint32_t hashFunc (uint32_t n) {
            // constant stolen from libdeflate :)
            uint32_t t = (n * 0x1E35A7BD) >> (32 - 15);
            return t;
        }
        inline std::pair<uint32_t, uint32_t> grabThreeBytes (uint8_t buffer[], size_t size, size_t offset) {
            int32_t n = ((int32_t)size - (int32_t)offset);
            if (n <= 0) {
                return {0, 0};
            }
            else if (n >= 3) {
                uint32_t out = (uint32_t)(buffer[offset]) | (buffer[offset + 1] << 8) | (buffer[offset + 2] << 16);
                offset += 3;
                return {out, 3};
            } else {
                uint32_t c = 0;
                for (size_t i = 0; i < n; i++, offset++) {
                    c |= (buffer[offset] << (i * 8));
                }
                return {c, n};
            }
        }
        inline std::pair<uint32_t, int32_t> grabFourBytes (uint8_t buffer[], size_t size, size_t offset) {
            int32_t n = ((int32_t)size - (int32_t)offset);
            if (n <= 0) {
                return {0, 0};
            }
            else if (n >= 4) {
                uint32_t out = (uint32_t)(buffer[offset]) | (buffer[offset + 1] << 8) | (buffer[offset + 2] << 16) | (buffer[offset + 3] << 24);
                offset += 4;
                return {out, 4};
            } else {
                uint32_t c = 0;
                for (size_t i = 0; i < n; i++, offset++) {
                    c |= (buffer[offset] << (i * 8));
                }
                return {c, n};
            }
        }
        void addHash (uint32_t length, uint32_t start, uint32_t hash) {
            member m;
            m.next = -1;
            m.data = length & 0x1ff;
            m.data |= (start << 9);
            if (map[hash].data == 0) {
                map[hash] = m;
            } else {
                size_t s = hash;
                while(map[s].next != -1) {
                    s = map[s].next;
                }
                map.push_back(m);
                map[s].next = map.size() - 1;
            }
        }
        inline bool hashMatch (uint32_t hash) {
            if (map[hash].data != 0) {
                return true;
            }
            return false;
        }
        public:

        LZ77 (size_t size) {
            window_index = 0;
            map.reserve(size);
            for (size_t i = 0; i < size; i++) {
                map.push_back({0, -1});
            }
        }
        void getMatchesSlow (uint32_t read_buffer[], uint8_t raw_buffer[], size_t read_buffer_index, RangeLookup& rl, RangeLookup& dl) {
            const size_t size = read_buffer_index;
            #ifdef DEBUG
            auto start = std::chrono::high_resolution_clock::now();
            #endif
            while (window_index < size) {
                std::pair<uint32_t, uint32_t> windowBytes = grabThreeBytes(raw_buffer, size, window_index);
                if (windowBytes.second < 3) {
                    return;
                }
                uint32_t match_length = 0;
                uint32_t windowHash = hashFunc(windowBytes.first);
                for (int32_t i = window_index-1; i >= 0; i--) {
                    std::pair<uint32_t, uint32_t> lookbackBytes = grabThreeBytes(raw_buffer, size, i);
                    if (lookbackBytes.first == windowBytes.first) {
                        // loop forward seeing how long bytes are equal
                        uint32_t tempLength = 3;
                        size_t j = i + 3;
                        size_t k = window_index + 3;
                        for (; j < size && k < size && raw_buffer[j] == raw_buffer[k] && match_length < 258; j++, k++, tempLength++);
                        if (tempLength > match_length) {
                            match_length = tempLength;
                            Range r = rl.lookup(match_length);
                            uint32_t o = (match_length) - r.start;
                            uint32_t of = window_index-i;
                            read_buffer[window_index] = (of << 14) | (o << 9) | (r.code & CHAR_BITS);
                        }
                    }
                }
                window_index++;
            }
            #ifdef DEBUG
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            std::cout << "slow match execution time: " << elapsed.count() << " ms\n";
            #endif
        }
        // modifies the read_buffer to contain matches lol
        //https://github.com/ebiggers/libdeflate/blob/master/lib/hc_matchfinder.h
        // https://www.youtube.com/watch?v=BfUejqd07yo
        // hash each three byte run
        // check if hash in map, if in map loop through each hash comparing the bytes to current hash bytes
        void getMatches (uint32_t read_buffer[], uint8_t raw_buffer[], size_t read_buffer_index, RangeLookup& rl, RangeLookup& dl) {
            const size_t size = read_buffer_index;
            #ifdef DEBUG
            auto start = std::chrono::high_resolution_clock::now();
            #endif
            while (window_index < size) {
                std::pair<uint32_t, uint32_t> bytes = grabFourBytes(raw_buffer, size, window_index);
                if (bytes.second != 4) {
                    break;
                }
                uint32_t w = hashFunc(bytes.first);
                if (hashMatch(w)) {
                    int32_t h = w;
                    while(map[h].next != -1 && grabFourBytes(raw_buffer, size, (map[h].data & 0xfffffe00) >> 9).first != bytes.first) {
                        h = map[h].next;
                    }
                    uint32_t start = (map[h].data & 0xfffffe00) >> 9;
                    std::pair<uint32_t, uint32_t> grab = grabFourBytes(raw_buffer, size, start);
                    if (grab.first == bytes.first && grab.second == 4) {
                        // read four bytes until unequal
                        // if unequal read drop a byte from last four bytes until same or out
                        uint32_t off = window_index+4;
                        uint32_t off2 = start+4;
                        uint32_t j = 4;
                        while(true) {
                            std::pair<uint32_t, uint32_t> w2 = grabFourBytes(raw_buffer, size, off);
                            std::pair<uint32_t, uint32_t> f = grabFourBytes(raw_buffer, size, off2);
                            if (w2 != f || j >= 258) {
                                // drop bytes from each till equal or not :)))
                                int32_t i = 1;
                                for (; i < 4 && w2 != f && j + i < size; i++) {
                                    w2.first = (w2.first & (0xffffffff >> (i * 8)));
                                    f.first = f.first & (0xffffffff >> (i * 8));
                                }
                                if (j >= 258) {
                                    j = 258;
                                    i = 4;
                                }
                                if (w2 == f && i != 4 && window_index + j + i < size) {
                                    // need to write out char again, 9 bits for lit/length
                                    // need to write length extra bits if needed, 5 bits for extra length data
                                    // need to write distance value in remaining 18 bits
                                    Range r = rl.lookup(j + i); // this will be length btw
                                    uint32_t o = j + i - r.start;
                                    uint32_t of = window_index-start;
                                    uint32_t of_m = (of << 14);
                                    read_buffer[window_index] = (of << 14) | (o << 9) | (r.code & CHAR_BITS);
                                    break;
                                } else {
                                    Range r = rl.lookup(j); // this will be length btw
                                    uint32_t o = (j) - r.start;
                                    uint32_t of = window_index-start;
                                    uint32_t of_m = (of << 14);
                                    read_buffer[window_index] = (of << 14) | (o << 9) | (r.code & CHAR_BITS);
                                    break;
                                }
                            } else {
                                off += 4;
                                off2 += 4;
                                j += 4;
                            }
                        }
                    }
                } else {
                    addHash(4, window_index, w);
                }
                window_index+=4;
            }
            #ifdef DEBUG
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            std::cout << "match execution time: " << elapsed.count() << " ms\n";
            #endif
        }

    };
    
    static Bitstream makeUncompressedBlock (uint8_t read_buffer[], size_t read_buffer_index, bool final) {
        Bitstream bs;
        uint8_t pre = 0b000;
        if (final) {
            pre |= 1;
        }
        bs.addBits(pre, 3);
        bs.nextByteBoundaryConditional();
        bs.addBits(read_buffer_index, 16);
        bs.addBits(~(read_buffer_index), 16);
        bs.addRawBuffer(read_buffer, read_buffer_index);
        return bs;
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
        // compressed separate, not treated as one run of bits
        // main huffman tree
        compressDynamicHuffmanTreeCodes(huff_bytes, bs, code_tree) ;
        //  dist tree
        compressDynamicHuffmanTreeCodes(dist_bytes, bs, code_tree);
    }

    // deflate

    static Bitstream compressBuffer (uint32_t read_buffer[], size_t read_buffer_index, FlatHuffmanTree tree, FlatHuffmanTree dist_tree, uint32_t preamble, bool final, RangeLookup& rl, RangeLookup& dl, bool skip_pre = false) {
        // compress into huffman code format
        Bitstream bs;
        // writing the block out to file
        if (!skip_pre) {
            uint32_t pre = (final) ? (preamble | 0b001) : preamble; 
            bs.addBits(pre, 3);
        }
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
    // done
    static size_t compress (std::string file_path, std::string new_file, bool better_compression) {
        FlatHuffmanTree fixed_dist_huffman(generateFixedDistanceCodes());
        FlatHuffmanTree fixed_huffman(generateFixedCodes());
        RangeLookup rl = generateLengthLookup();
        RangeLookup dl = generateDistanceLookup();
        bool q = false;
        size_t index = 0;

        size_t read_buffer_index = 0;
        uint32_t read_buffer[KB32];
        uint8_t raw_buffer[KB32];
        std::memset(read_buffer, 0, KB32);

        std::ifstream f;
        f.open(file_path.c_str(), std::ios::binary);

        BitFile out_file(new_file);
        size_t out_size = 0;
        while (!q) {
            f.read((char*)(raw_buffer), KB32);
            std::streamsize read = f.gcount();
            for(size_t i = 0; i < read; i++, read_buffer_index++) {
                read_buffer[read_buffer_index] = ((uint32_t)raw_buffer[i] & 0xff);
            }
            // writing the block out to file
            if (read < KB32) {
                q = true;
            }
            LZ77 lz(KB32);
            // finding the matches above length of 2
            if (better_compression) {
                lz.getMatchesSlow(read_buffer, raw_buffer, read_buffer_index, rl, dl);
            } else {
                lz.getMatches(read_buffer, raw_buffer, read_buffer_index, rl, dl);
            }
            std::pair<FlatHuffmanTree, FlatHuffmanTree> trees;
            bool set_fixed = false;
            try {
                trees = constructDynamicHuffmanTree(read_buffer, read_buffer_index, rl, dl);
            } catch (std::runtime_error e) {
                #ifdef DEBUG
                std::cout << "Dynamic tree oversubscribed!\n";
                #endif
                set_fixed = true;
            }
            Bitstream bs_fixed = compressBuffer(read_buffer, read_buffer_index, fixed_huffman, fixed_dist_huffman, 0b010, q, rl, dl);
            Bitstream bs_dynamic;
            try {
                bs_dynamic = compressBuffer(read_buffer, read_buffer_index, trees.first, trees.second, 0b100, q, rl, dl);
            } catch (std::runtime_error e) {
                #ifdef DEBUG
                std::cout << "Dynamic tree oversubscribed!\n";
                #endif
                set_fixed = true;
            }

            Bitstream picked;
            if (!set_fixed && bs_dynamic.getSize() < bs_fixed.getSize() && bs_dynamic.getSize() < read_buffer_index + 5) {
                picked = bs_dynamic;
            } else if(bs_fixed.getSize() < read_buffer_index + 5){
                picked = bs_fixed;
            } else {
                picked = makeUncompressedBlock(raw_buffer, read_buffer_index, q);
            }
            out_file.addBitStream(picked);
            read_buffer_index = 0;
        }
        out_file.writeFile();
        f.close();
        return out_size;
    }

    static std::vector<uint8_t> compress (char* data, size_t data_size, bool better_compression) {
        FlatHuffmanTree fixed_dist_huffman(generateFixedDistanceCodes());
        FlatHuffmanTree fixed_huffman(generateFixedCodes());
        RangeLookup rl = generateLengthLookup();
        RangeLookup dl = generateDistanceLookup();
        bool q = false;
        size_t index = 0;
        Bitstream out_stream;

        size_t read_buffer_index = 0;
        uint32_t read_buffer[KB32];
        uint8_t raw_buffer[KB32];
        std::memset(read_buffer, 0, KB32);
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
            if (better_compression) {
                lz.getMatchesSlow(read_buffer, raw_buffer, read_buffer_index, rl, dl);
            } else {
                lz.getMatches(read_buffer, raw_buffer, read_buffer_index, rl, dl);
            }

            std::pair<FlatHuffmanTree, FlatHuffmanTree> trees;
            bool set_fixed = false;
            try {
                trees = constructDynamicHuffmanTree(read_buffer, read_buffer_index, rl, dl);
            } catch (std::runtime_error e) {
                #ifdef DEBUG
                std::cout << "Dynamic tree oversubscribed!\n";
                #endif
                set_fixed = true;
            }
            Bitstream bs_fixed = compressBuffer(read_buffer, read_buffer_index, fixed_huffman, fixed_dist_huffman, 0b010, q, rl, dl);
            Bitstream bs_dynamic;
            try {
                bs_dynamic = compressBuffer(read_buffer, read_buffer_index, trees.first, trees.second, 0b100, q, rl, dl);
            } catch (std::runtime_error e) {
                #ifdef DEBUG
                std::cout << "Dynamic tree oversubscribed!\n";
                #endif
                set_fixed = true;
            }

            Bitstream picked;
            if (!set_fixed && bs_dynamic.getSize() < bs_fixed.getSize() && bs_dynamic.getSize() < read_buffer_index + 5) {
                picked = bs_dynamic;
            } else if(bs_fixed.getSize() < read_buffer_index + 5){
                picked = bs_fixed;
            } else {
                picked = makeUncompressedBlock(raw_buffer, read_buffer_index, q);
            }
            out_stream.copyBitstream(picked);
            read_buffer_index = 0;
        }
        return out_stream.getData();
    }
};