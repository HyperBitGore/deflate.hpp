#pragma once
#include "common.hpp"
#include <cstdint>
#include <iostream>
#include <utility>
#define MAX_LITLEN_CODE_LEN 15
#define MAX_DIST_CODE_LEN 15
#define MAX_PRE_CODE_LEN 7
#define KB32 32768

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
        std::vector<Code> generateCodes (uint32_t max_length) {   
            std::vector <PreCode> temp_codes;
            for (uint32_t i = 0; i < 300; i++) {
                uint32_t occurs = getOccur(i);
                if (occurs) {
                    temp_codes.push_back({(int32_t)i, occurs});
                }
            }
            FlatHuffmanTree temp_f(temp_codes, max_length);
            std::vector<Code> out_codes = temp_f.decode();
            return out_codes;
        }
        FlatHuffmanTree generateTree () {
            std::vector <PreCode> temp_codes;
            for (uint32_t i = 0; i < 300; i++) {
                uint32_t occurs = getOccur(i);
                if (occurs) {
                    temp_codes.push_back({(int32_t)i, occurs});
                }
            }
            return  FlatHuffmanTree(temp_codes);
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
            size_t getSize () {
                if (bit_offset == 0) {
                    return data.size() - 1;
                }
                return data.size();
            }
    };

    struct Match {
            uint16_t offset; //offset backwards from match
            uint16_t length; //length of match
            uint16_t start;
            Match () {
                offset = 0;
                length = 0;
                start = 0;
            }
            Match (uint16_t offset, uint16_t length, uint16_t start) {
                this->offset = offset;
                this->length = length;
                this->start = start;
            }
    };

    
    //https://cs.stanford.edu/people/eroberts/courses/soco/projects/data-compression/lossless/lz77/concept.htm
    //https://en.wikipedia.org/wiki/LZ77_and_LZ78
    class LZ77 {
        private:
        class MatchHashmap {
            private: 
            std::vector<Match> matches;
            public:
            MatchHashmap () {
            }
            MatchHashmap (size_t size) {
                matches.reserve(size);
                for (size_t i = 0; i < size; i++) {
                    matches.push_back(Match());
                }
            }

            bool addMatch (Match m) {
                size_t index = m.start;
                if (matches[index].length < m.length) {
                    matches[index] = m;
                    return true;
                }
                return false;
            }
        };

        std::vector <uint8_t> buffer;
        std::vector <Match> matches;
        MatchHashmap mhash;
        size_t window_index;

        // match with zero length is an error
        Match findLongestMatch () {
            size_t match_length = 3;
            Match m;
            for (int32_t i = window_index - 1; i < (int32_t)window_index && i > 0; i--) {
                if (buffer[i] == buffer[window_index]) {
                    size_t j = 1;
                    size_t k = i+1;
                    for (; k < buffer.size() && window_index+j < buffer.size() && buffer[k] == buffer[window_index+j]; k++, j++);
                    if (j >= match_length) {
                        match_length = j;
                        m = Match((uint16_t)window_index-i, (uint16_t)j, (uint16_t)window_index);
                    }
                }
            }
            return m;
        }

        public:

        LZ77 (size_t size) {
            window_index = 0;
            mhash = MatchHashmap(size);
            buffer.reserve(size);
        }

        std::vector<Match> getMatches (std::vector<uint8_t>& data) {
            for (size_t i = 0; i < data.size(); i++) {
                buffer.push_back(data[i]);
            }
            std::vector<Match> matches;
            while (window_index < buffer.size()) {
                Match m = findLongestMatch();
                if (m.length > 0 && mhash.addMatch(m)) {
                    matches.push_back(m);
                    window_index += m.length;
                } else {
                    window_index++;
                }
            }
            return matches;
        }

    };
    
    static Bitstream makeUncompressedBlock (std::vector<uint8_t>& buffer, bool final) {
        Bitstream bs;
        uint8_t pre = 0b000;
        if (final) {
            pre |= 1;
        }
        bs.addBits(pre, 3);
        bs.nextByteBoundary();
        bs.addBits(buffer.size(), 16);
        bs.addBits(~(buffer.size()), 16);
        bs.addRawBuffer(buffer);
        return bs;
    }

    struct match_index_comp {
        inline bool operator() (const Match& first, const Match& second) {
            return first.start < second.start;
        }
    };

    static std::pair<FlatHuffmanTree, FlatHuffmanTree> constructDynamicHuffmanTree (std::vector<uint8_t>& buffer, std::vector<Match> matches) {
        CodeMap c_map;
        c_map.addOccur(256);
        CodeMap dist_codes;
        RangeLookup rl = generateLengthLookup();
        RangeLookup dl = generateDistanceLookup();
        for (uint32_t i = 0; i < buffer.size(); i++) {
            if (matches.size() > 0 && i == matches[0].start) {
                size_t c = 0;
                for (size_t j = 0; j < matches.size() && matches[j].start == matches[0].start; j++, c++) {
                    Range lookup = rl.lookup(matches[j].length);
                    Range dist = dl.lookup(matches[j].offset);
                    c_map.addOccur(lookup.code);
                    dist_codes.addOccur(dist.code);
                }
                for (size_t j = 0; j < c; j++) {
                    matches.erase(matches.begin());   
                }
            } else {
                c_map.addOccur(buffer[i]);
            }
        }
        FlatHuffmanTree tree = c_map.generateTree();
        FlatHuffmanTree dist_tree = dist_codes.generateTree();
        return std::pair<FlatHuffmanTree, FlatHuffmanTree> (tree, dist_tree);
    }
    struct compare_code_value {
        inline bool operator() (const Code& code1, const Code& code2) {
            return code1.value < code2.value;
        }
    };
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
        std::sort(t_codes.begin(), t_codes.end(), compare_code_value());
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
                // std::cout << "writing " << (uint32_t)bytes[i] << "\n";
                inc = 1;
            }
            if (reps > 2) {
                uint32_t reps_code;
                uint32_t real_reps;
                if (bytes[i] == 0) {
                    if (reps <= 10) {
                        reps_code = 17;
                        inc += (reps < 10) ? reps : 10;
                        real_reps = (reps < 10) ? reps : 10;
                    } else {
                        reps_code = 18;
                        inc += (reps < 138) ? reps : 138;
                        real_reps = (reps < 138) ? reps : 138;
                    }
                } else {
                    reps_code = 16;
                    inc += (reps < 6) ? reps : 6;
                    real_reps = (reps < 6) ? reps : 6;
                }
                // std::cout << "Repeating " << (uint32_t)bytes[i] << "," << real_reps << " times\n";
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
            // std::cout << "current i: " << i << "\n";
        }
    }
    static void writeDynamicHuffmanTree (Bitstream& bs, std::vector<Match>& matches, FlatHuffmanTree tree, FlatHuffmanTree dist_tree, RangeLookup rl, RangeLookup dl) {
        // calculating how much above 257 to go for matches
        uint32_t matches_size = 0;
        if (matches.size() > 0) {
            matches_size = matches[0].length;
            for (auto& i : matches) {
                if (i.length > matches_size) {
                    matches_size = i.length;
                }
            }
            Range range = rl.lookup(matches_size);
            matches_size = range.code - 257 + 1;
        }
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
        std::vector<Code> huff_codes = tree.decode();
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

    static Bitstream compressBuffer (std::vector<uint8_t>& buffer, std::vector<Match> matches, FlatHuffmanTree tree, FlatHuffmanTree dist_tree, uint32_t preamble, bool final) {
        // compress into huffman code format
        Bitstream bs;
        RangeLookup rl = generateLengthLookup();
        RangeLookup dl = generateDistanceLookup();
        // writing the block out to file
        uint32_t pre = (final) ? (preamble | 0b001) : preamble; 
        bs.addBits(pre, 3);
        if ((preamble & 0b100) == 4) {
            writeDynamicHuffmanTree(bs, matches, tree, dist_tree, rl, dl);
        }
        for (uint32_t i = 0; i < buffer.size();) {
            if (matches.size() > 0 && i == matches[0].start) {
                // output the code for the thing
                Range lookup = rl.lookup(matches[0].length);

                Code c = tree.getCodeValue(lookup.code);
                bs.addBits(flipBits(c.code, c.len), c.len);
                // add extra bits to bitstream
                if (c.extra_bits > 0) {
                    uint32_t extra_bits = matches[0].length % lookup.start;
                    bs.addBits(extra_bits, c.extra_bits);
                }
                Range dist = dl.lookup(matches[0].offset);
                Code dic = dist_tree.getCodeValue(dist.code);
                bs.addBits(flipBits(dic.code, dic.len), dic.len);
                if (dic.extra_bits > 0) {
                    uint32_t extra_bits = matches[0].offset % dist.start;
                    bs.addBits(extra_bits, dic.extra_bits);
                }
                i += matches[0].length;
                matches.erase(matches.begin());
            } else {
                Code c = tree.getCodeValue((uint32_t)buffer[i]);
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
        std::streampos sp = getFileSize(file_path);
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
                std::vector<Match> matches = lz.getMatches(buffer);
                std::sort(matches.begin(), matches.end(), match_index_comp());

                std::vector<uint8_t> output_buffer;
                std::pair<FlatHuffmanTree, FlatHuffmanTree> trees = constructDynamicHuffmanTree(buffer, matches);

                Bitstream bs_fixed = compressBuffer(buffer, matches, fixed_huffman, fixed_dist_huffman, 0b001, q);
                Bitstream bs_dynamic = compressBuffer(buffer, matches, trees.first, trees.second, 0b010, q);
                if (bs_fixed.getSize() < (buffer.size() + 5) && bs_fixed.getSize() < bs_dynamic.getSize()) {
                    output_buffer = bs_fixed.getData();
                } else if (bs_dynamic.getSize() < (buffer.size() + 5)) {
                    output_buffer = bs_dynamic.getData();
                } else {
                    output_buffer = makeUncompressedBlock(buffer, q).getData();
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
        of.close();
        return out_index;
    }

    static std::vector<uint8_t> compress (char* data, size_t data_size) {
        // std::vector<PreCode> pre_codes = {{'A', 45}, {'B', 13}, {'C', 12}, {'D', 16}, 
        // {'E', 9}, {'F', 5}};
        // FlatHuffmanTree pre_codes_test(pre_codes);
        FlatHuffmanTree fixed_dist_huffman(generateFixedDistanceCodes());
        FlatHuffmanTree fixed_huffman(generateFixedCodes());
        bool q = false;
        std::vector<uint8_t> buffer;
        size_t index = 0;
        size_t out_index = 0;
        std::vector<uint8_t> out_data;
         while (!q) {
            for (; index < data_size && buffer.size() < KB32; index++) {
                buffer.push_back(data[index]);
            }
            // writing the block out to file
            if (index >= data_size) {
                q = true;
            }
            LZ77 lz(KB32);
            // finding the matches above length of 2
            std::vector<Match> matches = lz.getMatches(buffer);
            std::sort(matches.begin(), matches.end(), match_index_comp());

            std::vector<uint8_t> output_buffer;
            std::pair<FlatHuffmanTree, FlatHuffmanTree> trees = constructDynamicHuffmanTree(buffer, matches);

            Bitstream bs_fixed = compressBuffer(buffer, matches, fixed_huffman, fixed_dist_huffman, 0b010, q);
            Bitstream bs_dynamic = compressBuffer(buffer, matches, trees.first, trees.second, 0b100, q);
            Bitstream bs_out;
            if (bs_fixed.getSize() < (buffer.size() + 5) && bs_fixed.getSize() < bs_dynamic.getSize()) {
                bs_out = bs_dynamic;
            } else if (bs_dynamic.getSize() < (buffer.size() + 5)) {
                bs_out = bs_dynamic;
            } else {
                bs_out = makeUncompressedBlock(buffer, q);
            }
            // compare size of bs
            output_buffer = bs_out.getData();
            for (int32_t i = 0; i < (int32_t)bs_out.getSize(); i++) {
                out_data.push_back(output_buffer[i]);
                out_index++;
            }
            buffer.clear();
        }
        return out_data;
    }
};