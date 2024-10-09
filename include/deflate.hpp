#pragma once
#include "common.hpp"
#include <iostream>
#include <utility>
/*
     However, we describe the compressed block format
         in below, as a sequence of data elements of various bit
         lengths, not a sequence of bytes.  We must therefore specify
         how to pack these data elements into bytes to form the final
         compressed byte sequence:

             * Data elements are packed into bytes in order of
               increasing bit number within the byte, i.e., starting
               with the least-significant bit of the byte.
             * Data elements other than Huffman codes are packed
               starting with the least-significant bit of the data
               element.
             * Huffman codes are packed starting with the most-
               significant bit of the code.

         In other words, if one were to print out the compressed data as
         a sequence of bytes, starting with the first byte at the
         *right* margin and proceeding to the *left*, with the most-
         significant bit of each byte on the left as usual, one would be
         able to parse the result from right to left, with fixed-width
         elements in the correct MSB-to-LSB order and Huffman codes in
         bit-reversed order (i.e., with the first bit of the code in the
         relative LSB position).
*/
// so reading huffman codes we read left to right versus regular data which is the basic right to left bit read
// https://www.rfc-editor.org/rfc/rfc1951#page-6
//https://minitoolz.com/tools/online-deflate-inflate-decompressor/
//https://minitoolz.com/tools/online-deflate-compressor/
//https://github.com/madler/zlib

// https://www.cs.ucdavis.edu/~martel/122a/deflate.html
//implement deflate itself
    //-test implementation (use libdeflate or tinydeflate)
    //-need to fix compressed buffers being unreadable
    //-maybe order of bits is wrong?
//implement rest of inflate
//make it a command line utility
//add error checking and maybe test files lol
//optimize
class deflate : deflate_compressor {
private:
    class DynamicHuffLengthCompressor {
private:
    std::vector<Code> codes;

    uint32_t countRepeats (std::vector<uint8_t>& bytes, uint32_t index) {
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
    HuffmanTree constructTree (std::vector<uint8_t>& bytes) {
        CodeMap cm;
        for (uint32_t i = 0; i < bytes.size();) {
            uint32_t reps = countRepeats(bytes, i);
            if (reps > 2) {
                if (bytes[i] == 0) {
                    if (reps <= 10) {
                        cm.addOccur(17);
                        i += reps;
                    } else {
                        cm.addOccur(18);
                        i += (reps <= 138) ? reps : 138;
                    }
                } else {
                    cm.addOccur(16);
                    i += (reps <= 6) ? reps : 6;
                }
            } else {
                cm.addOccur(bytes[i]);
                i++;
            }
        }
        std::vector<Code> t_codes = cm.generateCodes();
        for (auto& i : codes) {
            for (auto& j : t_codes) {
                if (i.value == j.value) {
                    i.code = j.code;
                    i.len = j.len;
                    j.extra_bits = i.extra_bits;
                }
            }
        }
        return HuffmanTree(t_codes);
    }

    void writeCodeLengthTree (Bitstream& bs) {
        uint32_t max = 4;
        for (uint32_t i = 4; i < codes.size(); i++) {
            if (codes[i].len > 0) {
                max = i;
            }
        }
        max = max - 4;
        bs.addBits(max, 4);
        for (uint32_t i = 0; i < max + 4; i++) {
            bs.addBits(codes[i].len, 3);
        }
        bs.addBits(0, 4);
    }
public:
    DynamicHuffLengthCompressor() {
        codes = {
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
    }

    Bitstream compress (uint32_t len_max, HuffmanTree huff, HuffmanTree dist) {
        std::vector<uint8_t> bytes;
        std::vector<Code> huff_codes = huff.decode();
        // constructing raw bytes of the table
        struct compare_code_value {
            inline bool operator() (const Code& code1, const Code& code2) {
                return code1.value < code2.value;
            }
        };
        std::sort(huff_codes.begin(), huff_codes.end(), compare_code_value());
        for (uint32_t i = 0; i < 257 + len_max; i++) {
            bool write = false;
            for (auto& j : huff_codes) {
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
        std::vector<Code> dist_codes = dist.decode();
        std::sort(dist_codes.begin(), dist_codes.end(), compare_code_value());
        uint32_t max = dist_codes[dist_codes.size() - 1].value;
        for (uint32_t i = 0; i < 1 + max; i++) {
            bool write = false;
            for (auto& j : dist_codes) {
                if (j.value == i) {
                    bytes.push_back(j.len);
                    write = true;
                    break;
                }
            }
            if (!write) {
                bytes.push_back(0);
            }
        }
        // constructing the code length huffman tree
        // 3 bits for each code length
        // start by computing how often each code length character will be used
        HuffmanTree code_tree = constructTree(bytes);
        Bitstream bs;
        writeCodeLengthTree(bs);
        // compressing the table
        // for each byte we loop forward and see how many times it's repeated
        // depending on many times repeated we determine the proper code to use for that length of bytes
        for (uint32_t i = 0; i < bytes.size();) {
            uint32_t reps = countRepeats(bytes, i);
            uint32_t code = 0;
            uint32_t inc = 1;
            if (reps > 2) {
                if (bytes[i] == 0) {
                    if (reps <= 10) {
                        code = 17;
                        inc = (reps < 10) ? reps : 10;
                    } else {
                        code = 18;
                        inc = (reps < 138) ? reps : 138; 
                    }
                } else {
                    code = 16;
                    inc = (reps <= 6) ? reps : 6;
                }
            } else {
                code = bytes[i];
            }
            Code c = code_tree.getCompressedCode(code);
            bs.addBits(c.code, c.len);
            if (c.extra_bits > 0) {
                uint32_t start = 3;
                if (c.value == 18) {
                    start = 11;
                }
                uint32_t val = reps - start;
                bs.addBits(val, c.extra_bits);
            }
            i += inc;
        }

        return bs;
    }
};

    // deflate

    static Bitstream compressBuffer (std::vector<uint8_t>& buffer, std::vector<Match> matches, HuffmanTree tree, HuffmanTree dist_tree, uint32_t preamble, bool final) {
        // compress into huffman code format
        Bitstream bs;
        RangeLookup rl = generateLengthLookup();
        RangeLookup dl = generateDistanceLookup();
        // writing the block out to file
        uint32_t pre = (final) ? (preamble | 0b001) : preamble; 
        bs.addBits(pre, 3);
        bs.nextByteBoundary();
        if ((preamble & 0b100) == 4) {
            writeDynamicHuffmanTree(bs, matches, tree, dist_tree);
        }
        for (uint32_t i = 0; i < buffer.size(); i++) {
            if (matches.size() > 0 && i == matches[0].offset) {
                // output the code for the thing
                Range lookup = rl.lookup(matches[0].length);

                Code c = tree.getCompressedCode(lookup.code); //this is temp way to lookup 
                bs.addBits(c.code, c.len);
                // add extra bits to bitstream
                if (c.extra_bits > 0) {
                    uint32_t extra_bits = matches[0].length % lookup.start;
                    bs.addBits(extra_bits, c.extra_bits);
                }
                Range dist = dl.lookup(matches[0].dif);
                Code dic = dist_tree.getCompressedCode(dist.code);
                bs.addBits(dic.code, dic.len);
                if (dic.extra_bits > 0) {
                    uint32_t extra_bits = matches[0].dif % dist.start;
                    bs.addBits(extra_bits, dic.extra_bits);
                }
                matches.erase(matches.begin());
            } else {
                Code c = tree.getCompressedCode((uint32_t)buffer[i]);
                bs.addBits(c.code, c.len);
            }
        }
        Code endcode = tree.getCompressedCode(256);
        bs.addBits(endcode.code, endcode.len);
        return bs;
    }

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

    static std::pair<HuffmanTree, HuffmanTree> constructDynamicHuffmanTree (std::vector<uint8_t>& buffer, std::vector<Match> matches) {
        CodeMap c_map;
        c_map.addOccur(256);
        CodeMap dist_codes;
        RangeLookup rl = generateLengthLookup();
        RangeLookup dl = generateDistanceLookup();
        for (uint32_t i = 0; i < buffer.size(); i++) {
            if (matches.size() > 0 && i == matches[0].offset) {
                Range lookup = rl.lookup(matches[0].length);
                Range dist = dl.lookup(matches[0].dif);
                c_map.addOccur(lookup.code);
                dist_codes.addOccur(dist.code);
                matches.erase(matches.begin());
            } else {
                c_map.addOccur(buffer[i]);
            }
        }
        HuffmanTree tree(c_map.generateCodes());
        HuffmanTree dist_tree(dist_codes.generateCodes());
        return std::pair<HuffmanTree, HuffmanTree> (tree, dist_tree);
    }

    static void writeDynamicHuffmanTree (Bitstream& bs, std::vector<Match>& matches, HuffmanTree tree, HuffmanTree dist_tree) {
        uint32_t matches_size = 0;
        if (matches.size() > 0) {
            matches_size = matches[0].length;
            for (auto& i : matches) {
                if (i.length > matches_size) {
                    matches_size = i.length;
                }
            }
        }
        // HLIT
        bs.addBits(matches_size, 5);
        std::vector<Code> tree_codes= tree.decode();
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
        DynamicHuffLengthCompressor compressor;
        bs.addRawBuffer(compressor.compress(matches_size, tree, dist_tree).getData());
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
        HuffmanTree fixed_dist_huffman = generateFixedDistanceHuffman();
        HuffmanTree fixed_huffman = generateFixedHuffman();
        uint8_t c;
        bool q = false;
        std::vector<uint8_t> buffer;
        size_t out_index = 0;
        while (!q) {
            std::streampos p = sp - fi.tellg();
            if (p > 0 && buffer.size() < 32768) {
                c = fi.get();
                buffer.push_back(c);
            } else {
                // writing the block out to file
                if (p <= 0) {
                    q = true;
                }
                LZ77 lz(32768);
                // finding the matches above length of 2
                std::vector<Match> matches = lz.findBufferRuns(buffer);
                std::sort(matches.begin(), matches.end(), match_index_comp());

                std::vector<uint8_t> output_buffer;
                std::pair<HuffmanTree, HuffmanTree> trees = constructDynamicHuffmanTree(buffer, matches);

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

    static size_t compress (char* data, size_t data_size, char* out_data, size_t out_data_size) {
        HuffmanTree fixed_dist_huffman = generateFixedDistanceHuffman();
        HuffmanTree fixed_huffman = generateFixedHuffman();
        uint8_t c;
        bool q = false;
        std::vector<uint8_t> buffer;
        size_t index = 0;
        size_t out_index = 0;
         while (!q) {
            for (; index < data_size && buffer.size() < 32768; index++) {
                buffer.push_back(data[index]);
            }
            // writing the block out to file
            if (index >= data_size) {
                q = true;
            }
            LZ77 lz(32768);
            // finding the matches above length of 2
            std::vector<Match> matches = lz.findBufferRuns(buffer);
            std::sort(matches.begin(), matches.end(), match_index_comp());

            std::vector<uint8_t> output_buffer;
            std::pair<HuffmanTree, HuffmanTree> trees = constructDynamicHuffmanTree(buffer, matches);

            Bitstream bs_fixed = compressBuffer(buffer, matches, fixed_huffman, fixed_dist_huffman, 0b010, q);
            Bitstream bs_dynamic = compressBuffer(buffer, matches, trees.first, trees.second, 0b100, q);
            Bitstream bs_out;
            if (bs_fixed.getSize() < (buffer.size() + 5) && bs_fixed.getSize() < bs_dynamic.getSize()) {
                bs_out = bs_fixed;
            } else if (bs_dynamic.getSize() < (buffer.size() + 5)) {
                bs_out = bs_fixed;
            } else {
                bs_out = makeUncompressedBlock(buffer, q);
            }
            // Bitstream bs_store = makeUncompressedBlock(buffer, q);
            // output_buffer = bs_store.getData();
            // compare size of bs
            output_buffer = bs_out.getData();
            for (int32_t i = 0; i < bs_out.getSize(); i++) {
                out_data[out_index] = output_buffer[i];
                out_index++;
            }
            buffer.clear();
        }
        return out_index;
    }
};