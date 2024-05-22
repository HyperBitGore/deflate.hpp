#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <string>
#include <fstream>

//https://minitoolz.com/tools/online-deflate-inflate-decompressor/
//https://minitoolz.com/tools/online-deflate-compressor/

struct CodeL {
    int8_t code;
    int32_t n;
};

//huffman tree class for deflate, put this in private for deflate later
class HuffmanTree{
    private:
    struct Member{
        int8_t code;
        int32_t freq;
        std::shared_ptr<Member> left = nullptr;
        std::shared_ptr<Member> right = nullptr;

    };
    std::shared_ptr<Member> head = nullptr;

    void insert (Member m) {
        if (head) {
           
        } else {
            
        }
    }

    uint32_t pick_lowest (std::vector<Member>& members) {
        uint32_t n = 0;
        for(uint32_t i = 0; i < members.size(); i++) {
            if (members[i].freq < members[n].freq) {
                n = i;
            }
        }
        return n;
    }
    
    void reinsert (std::vector<Member>& members, Member m) {
        for (uint32_t i = 0; i < members.size(); i++) {
            if (m.freq < members[i].freq) {
                members.insert(members.begin() + i, m);
                return;
            }
        }
        members.insert(members.end(), m);
    }

    public:
    HuffmanTree () {
        
    }
    ~HuffmanTree () {

    }
    //copy constructor
    HuffmanTree (const HuffmanTree& huff) {
        
    }
    
    void encode (std::vector<CodeL> codes) {

        struct {
            bool operator()(CodeL a, CodeL b) const { return a.n < b.n; }
        } compareCodeL;

        std::sort(codes.begin(), codes.end(), compareCodeL);
        std::vector<Member> membs;
        for (auto& i : codes) {
            membs.push_back({i.code, i.n});
        }

        //STEP 1: pick the two smallest freq in array
        //STEP 2: combine the two lowest freq leafs
        //STEP 3: reinsert the new leaf into array
        //REPEAT: until the array is empty


        while (membs.size() > 1) {
            //step 1
            uint32_t index = pick_lowest(membs);
            Member m = membs[index];
            membs.erase(membs.begin() + index);
            index = pick_lowest(membs);
            Member m2 = membs[index];
            membs.erase(membs.begin() + index);
            //step 2
            Member m3 = {-1, m.freq + m2.freq};
            std::shared_ptr<Member> mp = std::make_shared<Member>(m);
            std::shared_ptr<Member> mp2 = std::make_shared<Member>(m2);
            m3.left = (m.code < m2.code) ? mp : mp2;
            m3.right = (m.code < m2.code) ? mp2 : mp;
            //step 3
            reinsert(membs, m3);
        }
        head = std::make_shared<Member>(membs[0]);
    }

    std::string flatten () {
        
    }

    uint32_t decode_bits (uint32_t input) {

        return 300; //never possible in a deflate compressed block
    }

};

/*
    * Data elements are packed into bytes in order of
    increasing bit number within the byte, i.e., starting
    with the least-significant bit of the byte.
    * Data elements other than Huffman codes are packed
    starting with the least-significant bit of the data
    element.
    * Huffman codes are packed starting with the most-
    significant bit of the code.
*/

class Deflate{
private:
    //from right to left
    static uint8_t extract1Bit(uint8_t c, uint8_t n) {
        return (c >> n) & 1;
    }
public:
    Deflate () {

    }
    ~Deflate () {

    }
    //copy constructor
    Deflate (const Deflate& def) {

    }
    //move constructor
    Deflate (const Deflate&& def) {

    }
    static void decode (std::string file_path, std::string new_file) {
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