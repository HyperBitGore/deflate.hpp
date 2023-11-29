#pragma once
#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>

struct TableElement {
	int8_t letter;
	size_t freq;
};


class HuffmanTree {
private:

	struct Letter {
		int8_t letter;
		size_t freq;
		std::shared_ptr<Letter> left;
		std::shared_ptr<Letter> right;
		Letter() {
			left = nullptr;
			right = nullptr;
			freq = 0;
			letter = -1;
		}
		Letter(int8_t letter, size_t freq) {
			this->letter = letter;
			this->freq = freq;
			left = nullptr;
			right = nullptr;
		}
		std::string toString() {
			std::string str = this->letter + " ";
			if (left) {
				str = str + left->toString();
			}
			if (right) {
				str = str + right->toString();
			}
			return str;
		}
	};
	struct sort_by_freq {
		inline bool operator() (const TableElement& n1, const TableElement& n2) {
			return (n1.freq > n2.freq);
		}
	};
	std::shared_ptr<Letter> root;
public:
	HuffmanTree() {
		root = nullptr;
	}
	//move constructor
	HuffmanTree(HuffmanTree&& si) noexcept {
		root = si.root;
	}
	//copy constructor
	HuffmanTree(const HuffmanTree& si) noexcept{
		root = si.root;
	}
	//adds a freq table into tree
	void addTable(std::vector<TableElement> table) {
		std::sort(table.begin(), table.end(), sort_by_freq());
		std::queue<std::shared_ptr<Letter>> let;
		//insert into queue
		for (int i = table.size() - 1; i >= 0; i--) {
			let.push(std::make_shared<Letter>(table[i].letter, table[i].freq));
		}
		//get two nodes and construct a node
		while (true) {
			if (let.size() > 1) {
				std::shared_ptr<Letter> l = let.front();
				let.pop();
				std::shared_ptr<Letter> r = let.front();
				let.pop();
				std::shared_ptr<Letter> n = std::make_shared<Letter>();
				n->left = l;
				n->right = r;
				n->freq = l->freq + r->freq;
				let.push(n);
			}
			else {
				break;
			}
		}
		root = let.front();
	}

	//returns flattend version of tree for writing to file
	std::string flatten() {
		if (root) {
			return root->toString();
		}
		return "";
	}

};

class Deflate {
private:

public:
	Deflate() {

	}
};