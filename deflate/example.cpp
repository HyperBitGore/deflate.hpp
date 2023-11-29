#include "deflate.h"


int main() {
	HuffmanTree tree;
	tree.addTable({ {'A', 20}, {'D', 100}, {'K', 30}
	});
	std::cout << tree.flatten() << "\n";
	return 0;
}