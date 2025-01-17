#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <string>
#include <fstream>

// deflate
//  -update implementation
//      -improve the matches
//      -make sure bitstream writing in correct order for huffman codes
//      -https://brandougherty.github.io/blog/posts/implementing_deflate:_incomplete_and_oversubscribed_codes.html
//      -rewrite buffer compression, maybe need to write bits in different order
//  -add error checking and maybe test files lol
//  -optimize

// inflate
//  -add file version
//  -optimize code reading, make one function in bitwrapper???
//  -reduce cycles by reading larger blocks of bits?? min length of bits for tree???

class deflate_compressor {
    protected:

    //from right to left
    static uint8_t extract1Bit(uint32_t c, uint16_t n) {
        return (c >> n) & 1;
    }
    static uint8_t extract1BitLeft (uint32_t c, uint16_t n) {
        return ((c << n) & 0b10000000000000000000000000000000) >> 31;
    }
    static uint8_t extract1BitLeft (uint8_t c, uint8_t n) {
        return ((c << n) & 0b10000000) >> 7;
    }

    struct Code {
        uint16_t code; //actual code
        int32_t len; //code length
        uint8_t extra_bits = 0; //extra bits for code
        uint16_t value; //original value
    };

    class FlatHuffmanTree {
        private:
            struct Member {
                uint16_t value;
                uint16_t code;
                int32_t len;
                uint8_t extra_bits = 0;
                int left;
                int right;
            };
            std::vector<Member> members;
            int head = -1;

            void insert (Code c) {
                if (head == -1) {
                    members.push_back({0, 0, -1, 0, -1, -1});
                    head = 0;
                }
                // insert down the line
                size_t index = 0;
                uint16_t bit = 0;
                size_t memb = head;
                while(bit < c.len) {
                    uint32_t direction = extract1Bit(c.code, bit);
                    // left
                    if (!direction) {
                        if (members[memb].left == -1) {
                            members.push_back({0, 0, -1, 0, -1, -1});
                            members[memb].left = members.size() - 1;
                        }
                        memb = members[memb].left;
                    } else {
                        //right
                        if (members[memb].right == -1) {
                            members.push_back({0, 0, -1, 0, -1, -1});
                            members[memb].right = members.size() - 1;
                        }
                        memb = members[memb].right;
                    }
                    bit++;
                    if (bit == c.len) {
                        members[memb].code = c.code;
                        members[memb].extra_bits = c.extra_bits;
                        members[memb].len = c.len;
                        members[memb].value = c.value;
                    }
                }
            }

            void construct (std::vector<Code>& codes) {
                 // sort the codes based on len, then sort by smallest value upward
                struct {
                    bool operator()(Code a, Code b) const { 
                        if (a.len < b.len) return true;
                        if (b.len < a.len) return false;

                        if (a.value < b.value) return true;
                        if (b.value < a.value) return false;
                        return false;
                    }
                } compareCode;
                std::sort(codes.begin(), codes.end(), compareCode);
                int16_t code = 0;
                int16_t next_code[16] = {0};
                int16_t bl_count[16] = {0};
                for (size_t i = 0; i < codes.size(); i++) {
                    int32_t len = codes[i].len;
                    if (len > 0) {
                        bl_count[len]++;
                    }
                }

                for (int32_t bits = 1; bits <= 15; bits++) {
                    code = (code + bl_count[bits - 1]) << 1;
                    next_code[bits] = code;
                }

                for (uint32_t i = 0; i < codes.size(); i++) {
                    int32_t len = codes[i].len;
                     if (len != 0) {
                        codes[i].code = next_code[len];
                        next_code[len]++;
                    }
                }

                for (size_t i = 0; i < codes.size(); i++) {
                    if (codes[i].len > 0) {
                        insert(codes[i]);
                    }
                }
            }

            Member findMemberCode (uint32_t code, uint32_t len) {
                int index = head;
                uint32_t bit = 0;
                while (bit < len && index != -1) {
                    uint32_t direction = extract1Bit(code, bit);
                    // left
                    if (!direction) {
                        index = members[index].left;
                    } 
                    // right
                    else {
                        index = members[index].right;
                    }
                    bit++;
                }
                if (index == -1) {
                    return {0, 0, -1};
                }
                return members[index];
            }

            Member findMemberCode (uint32_t code) {
                int index = head;
                uint32_t bit = 0;
                uint32_t concat = 0;
                while (index != -1 && concat != code) {
                    uint32_t direction = extract1Bit(code, bit);
                    concat |= (direction << bit);
                    // left
                    if (!direction) {
                        index = members[index].left;
                    } 
                    // right
                    else {
                        index = members[index].right;
                    }
                    bit++;
                }
                if (index == -1) {
                    return {0, 0, -1};
                }
                return members[index];
            }

            Member findMemberValue (uint32_t value) {
                for (size_t i = 0; i < members.size(); i++) {
                    if (members[i].len != -1 && members[i].value == value) {
                        return members[i];
                    }
                }
                return {0, 0, -1};
            }
        public:
            FlatHuffmanTree() {

            }
            FlatHuffmanTree (std::vector<Code> codes){
                construct(codes);
            }

            //copy constructor, not really, ptr is the same lol
            FlatHuffmanTree (const FlatHuffmanTree& huff) {
                members = huff.members;
                head = huff.head;
            }
            FlatHuffmanTree (FlatHuffmanTree& huff) {
                members = huff.members;
                head = huff.head;
            }

            Code getCodeEncoded (uint32_t code, int32_t len) {
                if (len == -1) {
                    return {0, -1, 0, 0};
                }
                Member m = findMemberCode(code, len);
                if (m.len == -1) {
                    return {0, -1, 0, 0};
                }
                return {m.code, m.len, m.extra_bits, m.value};
            }

            Code getCodeEncoded (uint32_t code) {
                Member m = findMemberCode(code);
                if (m.len == -1) {
                    return {0, -1, 0, 0};
                }
                return {m.code, m.len, m.extra_bits, m.value};
            }

            Code getCodeValue (uint32_t value) {
                Member m = findMemberValue(value);
                if (m.len == -1) {
                    return {0, -1, 0, 0};
                }
                return {m.code, m.len, m.extra_bits, m.value};
            }
            std::vector<Code> decode () {
                std::vector<Code> codes;
                for (auto& i : members) {
                    if (i.len > -1) {
                        codes.push_back({i.code, i.len, i.extra_bits, i.value});
                    }
                }
                struct {
                    bool operator()(Code a, Code b) const { return a.value < b.value; }
                } compareCodeV;
                std::sort(codes.begin(), codes.end(), compareCodeV);
                return codes;
            }
    };


    struct Range {
        uint32_t start;
        uint32_t end;
        uint32_t code;
        int32_t extra_bits;
    };

    class RangeLookup {
    private:
        std::vector <Range> ranges;
    public:
        RangeLookup () {
        }
        void addRange (Range r) {
            ranges.push_back(r);
        }
        Range lookup (uint32_t length) {
            for (Range i : ranges) {
                if (length >= i.start && length <= i.end) {
                    return i;
                }
            }
            return {0, 0, 0, -1};
        }
        Range findCode (uint32_t code) {
            for (Range i : ranges) {
                if (i.code == code) {
                    return i;
                }
            }
            return {0, 0, 0, -1};
        }
    };

    static std::vector<Code> generateFixedCodes () {
        std::vector <Code> fixed_codes;
        uint16_t i = 0;
        //regular alphabet
        for (uint16_t code = 48; i < 144; i++, code++) {
            fixed_codes.push_back({code, 8, 0, i});
        }
        for (uint16_t code = 400; i < 256; i++, code++) {
            fixed_codes.push_back({code, 9, 0, i});
        }
        uint8_t extra_bits = 0;
        for(uint16_t code = 0; i < 280; i++, code++) {
            switch (i) {
                case 265:
                    extra_bits = 1;
                break;
                case 269:
                    extra_bits = 2;
                break;
                case 273:
                    extra_bits = 3;
                break;
                case 277:
                    extra_bits = 4;
                break;
            }
            fixed_codes.push_back({code, 7, extra_bits, i});
        }
        for(uint16_t code = 192; i < 288; i++, code++) {
            switch (i) {
                case 281:
                    extra_bits = 5;
                break;
                case 285:
                    extra_bits = 0;
                break;
            }
            fixed_codes.push_back({code, 8, extra_bits, i});
        }
        return fixed_codes;
    }

    static std::vector <Code> generateFixedDistanceCodes () {
        std::vector <Code> fixed_codes;
        uint8_t extra_bits = 0;
        //regular alphabet
        for (uint16_t i = 0; i < 32; i++) {
            if (i >= 4) {
                extra_bits = (i / 2) - 1;
            } 
            fixed_codes.push_back({i, 5, extra_bits, i});
        }
        return fixed_codes;
    }

    static std::streampos getFileSize (std::string file) {
        std::streampos fsize = 0;
        std::ifstream fi (file, std::ios::binary);
        fsize = fi.tellg();
        fi.seekg(0, std::ios::end);
        fsize = fi.tellg() - fsize;
        fi.close();
        return fsize;
    }


    static RangeLookup generateLengthLookup () {
        RangeLookup rl;
        rl.addRange({3, 3, 257, 0});
        rl.addRange({4, 4, 258, 0});
        rl.addRange({5, 5, 259, 0});
        rl.addRange({6, 6, 260, 0});
        rl.addRange({7, 7, 261, 0});
        rl.addRange({8, 8, 262, 0});
        rl.addRange({9, 9, 263, 0});
        rl.addRange({10, 10, 264, 0});
        rl.addRange({11, 12, 265, 1});
        rl.addRange({13, 14, 266, 1});
        rl.addRange({15, 16, 267, 1});
        rl.addRange({17, 18, 268, 1});
        rl.addRange({19, 22, 269, 2});
        rl.addRange({23, 26, 270, 2});
        rl.addRange({27, 30, 271, 2});
        rl.addRange({31, 34, 272, 2});
        rl.addRange({35, 42, 273, 3});
        rl.addRange({43, 50, 274, 3});
        rl.addRange({51, 58, 275, 3});
        rl.addRange({59, 66, 276, 3});
        rl.addRange({67, 82, 277, 4});
        rl.addRange({83, 98, 278, 4});
        rl.addRange({99, 114, 279, 4});
        rl.addRange({115, 130, 280, 4});
        rl.addRange({131, 162, 281, 5});
        rl.addRange({163, 194, 282, 5});
        rl.addRange({195, 226, 283, 5});
        rl.addRange({227, 257, 284, 5});
        rl.addRange({258, 258, 285, 0});
        return rl;
    }

    static RangeLookup generateDistanceLookup () {
        RangeLookup rl;
        rl.addRange({1, 1, 0, 0});
        rl.addRange({2, 2, 1, 0});
        rl.addRange({3, 3, 2, 0});
        rl.addRange({4, 4, 3, 0});
        rl.addRange({5, 6, 4, 1});
        rl.addRange({7, 8, 5, 1});
        rl.addRange({9, 12, 6, 2});
        rl.addRange({13, 16, 7, 2});
        rl.addRange({17, 24, 8, 3});
        rl.addRange({25, 32, 9, 3});
        rl.addRange({33, 48, 10, 4});
        rl.addRange({49, 64, 11, 4});
        rl.addRange({65, 96, 12, 5});
        rl.addRange({97, 128, 13, 5});
        rl.addRange({129, 192, 14, 6});
        rl.addRange({193, 256, 15, 6});
        rl.addRange({257, 384, 16, 7});
        rl.addRange({385, 512, 17, 7});
        rl.addRange({513, 768, 18, 8});
        rl.addRange({769, 1024, 19, 8});
        rl.addRange({1025, 1536, 20, 9});
        rl.addRange({1537, 2048, 21, 9});
        rl.addRange({2049, 3072, 22, 10});
        rl.addRange({3073, 4096, 23, 10});
        rl.addRange({4097, 6144, 24, 11});
        rl.addRange({6145, 8192, 25, 11});
        rl.addRange({8193, 12288, 26, 12});
        rl.addRange({12289, 16384, 27, 12});
        rl.addRange({16385, 24576, 28, 13});
        rl.addRange({24577, 32768, 29, 13});
        return rl;
    }
};