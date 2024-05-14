#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <string>


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

    public:
    HuffmanTree(){
        
    }
    ~HuffmanTree(){

    }
    //copy constructor
    HuffmanTree(const HuffmanTree& huff){
        
    }
    
    void encode(std::vector<CodeL> codes) {

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
            m3.left = std::make_shared<Member>(m);
            m3.right = std::make_shared<Member>(m2);
            //step 3
            membs.push_back(m3);
        }
        head = std::make_shared<Member>(membs[0]);

        
    }
};


class Deflate{
private:

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
    void decode (std::string file_path) {

    }
};