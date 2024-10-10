#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <algorithm>
#include <string>
#include <fstream>


class deflate_compressor {
    protected:
    struct Code {
    uint16_t code; //actual code
    int32_t len; //code length
    uint8_t extra_bits = 0; //extra bits for code
    uint16_t value; //original value
};

//huffman tree class for deflate, put this in private for deflate later
class HuffmanTree{
    private:
    struct Member{
        uint16_t value;
        int32_t len;
        uint16_t code;
        uint8_t extra_bits = 0;
        std::shared_ptr<Member> left = nullptr;
        std::shared_ptr<Member> right = nullptr;

    };
    std::shared_ptr<Member> head;
    //from right to left
    uint8_t extract1Bit(uint16_t c, uint16_t n) {
        return (c >> n) & 1;
    }

    uint32_t extractBits (uint32_t c, uint8_t offset, uint32_t count) {
        uint32_t out = 0;
        for (uint8_t i = 0; i < count; i++) {
            out |= (extract1Bit(c, offset) << (count));
        }
        return out;
    }

    void reinsert (std::vector<Member>& members, Member m) {
        for (uint32_t i = 0; i < members.size(); i++) {
            if (m.len < members[i].len) {
                members.insert(members.begin() + i, m);
                return;
            }
        }
        members.insert(members.end(), m);
    }

    int32_t findMaxCode (std::vector<Code>& codes) {
        int32_t o = codes[0].code;
        for (int32_t i = 0; i < codes.size(); i++) {
            if (codes[i].code > o) {
                o = codes[i].code;
            }
        }
        return o;
    }

    int16_t countCodeLength (std::vector<Member>& membs, int32_t code_length) {
        int16_t count = 0;
        for (auto& i : membs) {
            if (i.len == code_length) {
                count++;
            }
        }
        return count;
    }

    void insert (Member m) {
        if (!head) {
            Member mm = {300, -1, 0, 0};
            head = std::make_shared<Member>(mm);
        }
        std::shared_ptr<Member> ptr = head;
        std::vector<Member> members;
        uint16_t bit_offset = m.len;
        while (true) {
            bit_offset--;
            // check if go left or right
            uint8_t val = extract1Bit(m.code, bit_offset);

            // left = 0 right = 1
            if (val) {
                //check if right is open
                if (!ptr->right) {
                    Member mm = {300, -1, 0, 0};
                    ptr->right = std::make_shared<Member>(mm);
                }
                ptr = ptr->right;
            } else {
                //check if left is open
                if (!ptr->left) {
                    Member mm = {300, -1, 0, 0};
                    ptr->left = std::make_shared<Member>(mm);
                }
                ptr = ptr->left;
            }

            // ptr->len += m.len;
            //check if on last ptr
            if (bit_offset == 0) {
                ptr->value = m.value;
                ptr->extra_bits = m.extra_bits;
                ptr->code = m.code;
                ptr->len = m.len;
                break;
            }
        }
    }
    Code getCodeFromCompressed (uint32_t compressed_code, std::shared_ptr<Member> ptr) {
        if (ptr->code == compressed_code && ptr->len > 0) {
            return {ptr->code, ptr->len, ptr->extra_bits, ptr->value};
        }
        Code left = {0, -1};
        if (ptr->left != nullptr) {
            left = getCodeFromCompressed(compressed_code, ptr->left);
        }
        if (left.len > 0) {
            return left;
        }
        Code right = {0, -1};
        if (ptr->right != nullptr) {
            right = getCodeFromCompressed(compressed_code, ptr->right);
        }
        if (right.len > 0) {
            return right;
        }
        return {0, -1, 0, 300};
    }

    Code getCodeFromUncompressed (uint32_t uncompressed_code, std::shared_ptr<Member> ptr) {
        if (ptr->value == uncompressed_code && ptr->len > 0) {
            return {ptr->code, ptr->len, ptr->extra_bits, ptr->value};
        }
        Code left = {0, -1};
        if (ptr->left != nullptr) {
            left = getCodeFromUncompressed(uncompressed_code, ptr->left);
        }
        if (left.len > 0) {
            return left;
        }
        Code right = {0, -1};
        if (ptr->right != nullptr) {
            right = getCodeFromUncompressed(uncompressed_code, ptr->right);
        }
        if (right.len > 0) {
            return right;
        }
        return {0, -1, 0, 300};
    }

    void decodeCodes (std::vector<Code>& codes, std::shared_ptr<Member> ptr) {
        if (ptr->len != -1) {
            codes.push_back({ptr->code, ptr->len, ptr->extra_bits, ptr->value});
        }
        if (ptr->left != nullptr) {
            decodeCodes(codes, ptr->left);
        }
        if (ptr->right != nullptr) {
            decodeCodes(codes, ptr->right);
        }

    }
    public:
    HuffmanTree () {
        head = nullptr;
    }

    HuffmanTree (std::vector<Code> codes) {
        encode(codes);
    }
    //copy constructor, not really, ptr is the same lol
    HuffmanTree (const HuffmanTree& huff) {
        head = huff.head;
    }
    HuffmanTree (HuffmanTree& huff) {
        head = huff.head;
    }

    bool encode (std::vector<Code> codes) {
        if (codes.size() < 1) {
            return false;
        }
        struct {
            bool operator()(Code a, Code b) const { return a.len < b.len; }
        } compareCodeL;

        std::sort(codes.begin(), codes.end(), compareCodeL);
        std::vector<Member> membs;
        for (auto& i : codes) {
            membs.push_back({i.value, i.len, i.code, i.extra_bits});
        }
        std::vector<Member> saved_memb = membs;
        int16_t code = 0;
        int16_t next_code[16];
        int16_t bl_count[16];
        for (uint32_t i = 0; i < 16; i++) {
            bl_count[i] = countCodeLength(membs, i);
        }
        bl_count[0] = 0;
        for (int32_t bits = 1; bits <= 15; bits++) {
            code = (code + bl_count[bits - 1]) << 1;
            next_code[bits] = code;
        }

        for (int32_t i = 0; i < membs.size(); i++) {
            int32_t len = membs[i].len;
            membs[i].code = next_code[len];
            next_code[len]++;
        }

        //LOOP THROUGH AND DETERMINE WHERE TO PLACE MEMBS BASED ON THEIR CODE

        while (membs.size() > 0) {
            insert(membs[0]);
            membs.erase(membs.begin());
        }





        return true;
    }
    // returns the codes that makeup the tree
    std::vector<Code> decode () {
        std::vector<Code> codes;
        decodeCodes(codes, head);

        return codes;
    }

    std::string flatten () {
        return "";
    }

    // already have the input decoded to proper format
    Code getUncompressedCode (uint32_t compressed_code) {
        return getCodeFromCompressed(compressed_code, head); 
    }
    // Input is uncompressed code
    Code getCompressedCode (uint32_t uncompressed_code) {
        return getCodeFromUncompressed(uncompressed_code, head);
    }

};


struct Match {
        uint16_t offset; //offset from start to match
        uint16_t length; //length of match
        uint8_t follow_code; //code after match
        int32_t dif; // difference from match to original
        Match () {
            offset = 0;
            length = 0;
            follow_code = 0;
            dif = 0;
        }
        Match (uint16_t offset, uint16_t length, uint8_t follow_code, int32_t dif) {
            this->offset = offset;
            this->length = length;
            this->follow_code = follow_code;
            this->dif = dif;
        }
        //do this
        bool overlaps (Match m) {
            return (!(this->offset + this->length < m.offset || m.offset + m.length < this->offset)) && 
            (this->offset + this->length <= m.offset + m.length || m.offset + m.length <= this->offset + this->length);
        }
};

//https://cs.stanford.edu/people/eroberts/courses/soco/projects/data-compression/lossless/lz77/concept.htm
//https://en.wikipedia.org/wiki/LZ77_and_LZ78
class LZ77 {
    private:
    std::vector <uint8_t> buffer;
    std::vector <Match> prev_matches;
    uint32_t window_index;
    uint32_t size;

    //tries to find longest match, and moves window forward by 1 byte, if no match found, just returns an empty Match
    void findLongestMatch () {
        std::vector<Match> matches;
        for (int i = window_index - 1; i >= 0; i--) {
            if (buffer[i] == buffer[window_index]) {
                uint32_t length = 0;
                int j;
                for (j = i; j < window_index && window_index + length < buffer.size() && buffer[j] == buffer[window_index + length]; j++, length++);
                if (length > 2) {
                    matches.push_back(Match(i, length, (window_index + length < buffer.size()) ? buffer[window_index + length] : 0, window_index - i));
                }
            }
        }
        for (int i = 0; i < matches.size();) {
            bool er = false;
            for (int j = 0; j < prev_matches.size() && i < matches.size(); j++) {
                // when a matches is erased creates issue somehow here
                if (prev_matches[j].overlaps(matches[i])) {
                    if (prev_matches[j].length < matches[i].length) {
                        prev_matches[j] = matches[i];
                    } else {
                        er = true;
                        matches.erase(matches.begin() + i);
                    }           
                }
            }
            if (!er) {
                i++;
            }
        }
        Match longest;
        if (matches.size() > 0) {
            longest = matches[0];
            for (auto& i : matches) {
                if (i.length > longest.length) {
                    longest = i;
                }
            }
            prev_matches.push_back(longest);
        } else {
            longest.follow_code = buffer[window_index];
        }
        window_index += (longest.length > 0) ? longest.length : 1;
    }

    uint32_t remainingWindow () {
        return (uint32_t)buffer.size() - window_index;
    }
    void copyBuffer (std::vector<uint8_t>& buf) {
        for (uint8_t i : buf) {
            buffer.push_back(i);
        }
    }
    public:
    
    LZ77 (uint32_t size) {
        this->size = size;
        this->window_index = 0;
    }
    //maybe just return the matches vector and the inflate function can use to compress
    std::vector<Match> findBufferRuns (std::vector<uint8_t>& buf) {
        copyBuffer(buf);
        std::vector<Match> matches;
        //loop through buffer and find the longest matches
        //figure out why this is going out of bounds
        while (remainingWindow() > 0) {
            findLongestMatch();
        }
        //loop through buffer and edit it to compress streaks
        for (int i = 0; i < buffer.size();) {
            bool match = false;
            int j = 0;
            for (; j < prev_matches.size(); j++) {
                if (prev_matches[j].offset == i) {
                    match = true;
                    matches.push_back(prev_matches[j]);
                    i += prev_matches[j].length;
                    break;
                }
            }
            if (!match) {
                i++;
            } else {
                prev_matches.erase(prev_matches.begin() + j);
            }
        }
        return matches;
    }
};

struct Range {
    uint32_t start;
    uint32_t end;
    uint32_t code;
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
        return {0, 0, 0};
    }
};

class Bitstream {
private:
    uint8_t bit_offset;
    uint32_t offset;
    std::vector<uint8_t> data;
    
    //from right to left
    uint8_t extract1Bit(uint32_t c, uint16_t n) {
        return (c >> n) & 1;
    }
    uint8_t extract1BitLeft (uint32_t c, uint16_t n) {
        return ((c << n) & 0b10000000000000000000000000000000) >> 31;
    }
    uint8_t extract1BitLeft (uint8_t c, uint8_t n) {
        return ((c << n) & 0b10000000) >> 7;
    }
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
    void nextByteBoundary () {
        
        data.push_back(0);
        bit_offset = 0;
        offset++;
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

class Bitwrapper {
    private:
    size_t size = 0;
    size_t offset = 0;
    uint8_t bit_offset = 0;
    uint8_t* data;

    public:
    Bitwrapper (void* data, size_t size) {
        this->size = size;
        this->data = (uint8_t*)data;
    }
    //copy constructor
    Bitwrapper (const Bitwrapper& wrap) {
        size = wrap.size;
        offset = wrap.size;
        bit_offset = wrap.bit_offset;
        data = wrap.data;
    }

    uint32_t readBits (uint8_t bits) {
        if (bits > 32) {
            return 0;
        }
        uint32_t val = 0;
        for (uint32_t i = 0; i < bits; i++) {
            val |= (extract1Bit(data[offset], bit_offset++) << i);
            if (bit_offset > 7) {
                offset++;
                if (offset >= size) {
                    return val;
                }
                bit_offset = 0;
            }
        }
        return val;
    }

    uint8_t readByte () {
        return data[offset++];
    }

    void moveByte (bool off = false) {
        if (off) {
            if (bit_offset != 0) {
                offset++;
                bit_offset = 0;
            }
            return;
        }
        offset++;
        bit_offset = 0;
    }

    size_t getOffset() {
        if (offset > 0 && bit_offset == 0) {
            return offset - 1;
        }
        return offset;
    }

    size_t getSize() {
        return size;
    }
};


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
    std::vector<Code> generateCodes () {   
        struct t_code {
            uint32_t occurs;
            uint32_t value;
        };
        std::vector <t_code> temp_codes;
        for (uint32_t i = 0; i < 300; i++) {
            uint32_t occurs = getOccur(i);
            if (occurs) {
                // std::cout << "hit: " << i << ", occurs: " << occurs << "\n";
                temp_codes.push_back({occurs, i});
            }
        }

        struct compare_tcode {
            inline bool operator() (const t_code& t1, const t_code& t2) {
                return t1.occurs > t2.occurs;
            }
        };
        std::sort(temp_codes.begin(), temp_codes.end(), compare_tcode());
        int32_t len = 2;
        uint32_t code = 0;
        std::vector <Code> out_codes;
        for (t_code i : temp_codes) {
             // check if len needs to be increased for this one, so if the amount has surpassed the allowed number of codes of the bits
            uint32_t allowed = std::pow(2, len);
            if (code + 1 > allowed) {
                len++;
            }
            out_codes.push_back({(uint16_t)code, len, 0, (uint16_t)i.value});
            code++;
        }
        return out_codes;
    }
};
    
    //from right to left
    static uint8_t extract1Bit (uint8_t c, uint8_t n) {
        return (c >> n) & 1;
    }
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

    static HuffmanTree generateFixedHuffman () {
        std::vector <Code> fixed_codes = generateFixedCodes();
        return HuffmanTree(fixed_codes);
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

    static HuffmanTree generateFixedDistanceHuffman () {
        std::vector <Code> fixed_dist_codes = generateFixedDistanceCodes();
        return HuffmanTree(fixed_dist_codes);
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

    struct match_index_comp {
        inline bool operator() (const Match& first, const Match& second) {
            return first.offset < second.offset;
        }
    };


    static RangeLookup generateLengthLookup () {
        RangeLookup rl;
        rl.addRange({3, 3, 257});
        rl.addRange({4, 4, 258});
        rl.addRange({5, 5, 259});
        rl.addRange({6, 6, 260});
        rl.addRange({7, 7, 261});
        rl.addRange({8, 8, 262});
        rl.addRange({9, 9, 263});
        rl.addRange({10, 10, 264});
        rl.addRange({11, 12, 265});
        rl.addRange({13, 14, 266});
        rl.addRange({15, 16, 267});
        rl.addRange({17, 18, 268});
        rl.addRange({19, 22, 269});
        rl.addRange({23, 26, 270});
        rl.addRange({27, 30, 271});
        rl.addRange({31, 34, 272});
        rl.addRange({35, 42, 273});
        rl.addRange({43, 50, 274});
        rl.addRange({51, 58, 275});
        rl.addRange({59, 66, 276});
        rl.addRange({67, 82, 277});
        rl.addRange({83, 98, 278});
        rl.addRange({99, 114, 279});
        rl.addRange({115, 130, 280});
        rl.addRange({131, 162, 281});
        rl.addRange({163, 194, 282});
        rl.addRange({195, 226, 283});
        rl.addRange({227, 257, 284});
        rl.addRange({258, 258, 285});
        return rl;
    }

    static RangeLookup generateDistanceLookup () {
        RangeLookup rl;
        rl.addRange({1, 1, 0});
        rl.addRange({2, 2, 1});
        rl.addRange({3, 3, 2});
        rl.addRange({4, 4, 3});
        rl.addRange({5, 6, 4});
        rl.addRange({7, 8, 5});
        rl.addRange({9, 12, 6});
        rl.addRange({13, 16, 7});
        rl.addRange({17, 24, 8});
        rl.addRange({25, 32, 9});
        rl.addRange({33, 48, 10});
        rl.addRange({49, 64, 11});
        rl.addRange({65, 96, 12});
        rl.addRange({97, 128, 13});
        rl.addRange({129, 192, 14});
        rl.addRange({193, 256, 15});
        rl.addRange({257, 384, 16});
        rl.addRange({385, 512, 17});
        rl.addRange({513, 768, 18});
        rl.addRange({769, 1024, 19});
        rl.addRange({1025, 1536, 20});
        rl.addRange({1537, 2048, 21});
        rl.addRange({2049, 3072, 22});
        rl.addRange({3073, 4096, 23});
        rl.addRange({4097, 6144, 24});
        rl.addRange({6145, 8192, 25});
        rl.addRange({8193, 12288, 26});
        rl.addRange({12289, 16384, 27});
        rl.addRange({16385, 24576, 28});
        rl.addRange({24577, 32768, 29});
        return rl;
    }
    public:

};