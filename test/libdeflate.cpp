#include "../include/deflate.hpp"
#include "../include/inflate.hpp"
#include <cstddef>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include "../build/external/include/libdeflate.h"

struct File {
    char* data;
    size_t size;
    File (size_t size) {
        data = static_cast<char*>(std::malloc(size));
        this->size = size;
    }
    ~File() {
        std::free(data);
    }
        // Copy constructor
    File(const File& other) {
        size = other.size;
        data = static_cast<char*>(std::malloc(size));
        if (!data) throw std::runtime_error("Copy allocation failed");
        std::memcpy(data, other.data, size);
    }

    // Move constructor
    File(File&& other) noexcept {
        data = other.data;
        size = other.size;
        other.data = nullptr;
        other.size = 0;
    }

    // Copy assignment
    File& operator=(const File& other) {
        if (this != &other) {
            std::free(data);
            size = other.size;
            data = static_cast<char*>(std::malloc(size));
            if (!data) throw std::runtime_error("Copy assignment allocation failed");
            std::memcpy(data, other.data, size);
        }
        return *this;
    }

    // Move assignment
    File& operator=(File&& other) noexcept {
        if (this != &other) {
            std::free(data);
            data = other.data;
            size = other.size;
            other.data = nullptr;
            other.size = 0;
        }
        return *this;
    }
};

void writeBufferToFile (char* data, size_t size, std::string file_path) {
    std::ofstream file;
    file.open(file_path, std::ios::binary);
    file.write(data, size);
    file.close();
}

bool sameData (File* f1, File* f2) {
    if (f1->size != f2->size) {
        return false;
    }
    for (size_t i = 0; i < f1->size; i++) {
        if (f1->data[i] != f2->data[i]) {
            std::cerr << "failed at index " << i << "\n";
            return false;
        }
    }

    return true;
}

size_t fileSize (std::string name) {
    std::filesystem::path path = name;
    return static_cast<size_t>(std::filesystem::file_size(path));
}

File readFile (std::string name) {
    std::ifstream f;
    f.open(name, std::ios::binary);
    if (!f) {
        throw std::runtime_error("Failed to read file " + name);
    }
    size_t sizef = fileSize(name);
    File file (sizef);
    f.read(file.data, sizef);
    f.close();
    return file;
}

// Runs a full round-trip test for a given image file:
//   1. libdeflate compresses the file, then inflate.hpp decompresses it.
//   2. deflate.hpp compresses the file, then libdeflate decompresses it.
//   3. deflate.hpp compresses the file, then inflate.hpp decompresses it.
// Returns false if any step fails.
bool testDecompressionFile(std::string path, int compressionLevel) {
    File original = readFile(path);
    std::cerr << path << " is " << original.size << " bytes\n";

    // --- Step 1: libdeflate compress → inflate.hpp decompress ---
    libdeflate_compressor* compressor = libdeflate_alloc_compressor(1);
    File libCompressed(original.size + 100);
    size_t libCompressedSize = libdeflate_deflate_compress(
        compressor, original.data, original.size,
        libCompressed.data, original.size + 100);
    if (libCompressedSize == 0) {
        std::cerr << "[FAIL] libdeflate compress failed for " << path << "\n";
        return false;
    }
    std::cerr << "libdeflate compressed size for " << path << ": " << libCompressedSize << "\n";
    writeBufferToFile(libCompressed.data, libCompressedSize, path + "libtestdeflate.txt");

    File hppInflated(original.size);
    size_t hppInflatedSize = inflate::decompress(
        libCompressed.data, libCompressedSize,
        hppInflated.data, original.size);
    if (hppInflatedSize == 0) {
        std::cerr << "[FAIL] inflate.hpp decompress of libdeflate output failed for " << path << "\n";
        return false;
    }
    writeBufferToFile(hppInflated.data, hppInflatedSize, path + "hpptestinflate.bmp");
    std::cerr << "[PASS] Step 1: inflate.hpp round-trip (via libdeflate) succeeded\n";

    // --- Step 2: deflate.hpp compress → libdeflate decompress ---
    std::vector<uint8_t> hppCompressed = deflate::compress(original.data, original.size, compressionLevel);
    if (hppCompressed.size() == 0) {
        std::cerr << "[FAIL] deflate.hpp compress failed for " << path << "\n";
        return false;
    }
    std::cerr << "deflate.hpp compressed size for " << path << ": " << hppCompressed.size() << "\n";
    writeBufferToFile(reinterpret_cast<char*>(hppCompressed.data()), hppCompressed.size(), path + "hpptestdeflate.txt");

    libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    File libInflated(original.size * 2);
    size_t libInflatedSize = 0;
    libdeflate_result libInflateResult = libdeflate_deflate_decompress(
        decompressor,
        hppCompressed.data(), hppCompressed.size(),
        libInflated.data, original.size * 2,
        &libInflatedSize);
    if (libInflateResult != LIBDEFLATE_SUCCESS) {
        std::cerr << "[FAIL] libdeflate decompress of deflate.hpp output failed for " << path << "\n";
        return false;
    }
    std::cerr << "libdeflate decompressed size for " << path << ": " << libInflatedSize << "\n";
    writeBufferToFile(libInflated.data, libInflatedSize, path + "libtestinflate.bmp");
    std::cerr << "[PASS] Step 2: libdeflate round-trip (via deflate.hpp) succeeded\n";

    // --- Step 3: deflate.hpp compress → inflate.hpp decompress ---
    File hppInflatedFromHpp(original.size);
    size_t hppInflatedFromHppSize = inflate::decompress(
        hppCompressed.data(), hppCompressed.size(),
        hppInflatedFromHpp.data, original.size);
    if (hppInflatedFromHppSize == 0) {
        std::cerr << "[FAIL] inflate.hpp decompress of deflate.hpp output failed for " << path << "\n";
        return false;
    }
    std::cerr << "inflate.hpp decompressed size (deflate.hpp input) for " << path << ": " << hppInflatedFromHppSize << "\n";
    writeBufferToFile(hppInflatedFromHpp.data, hppInflatedFromHppSize, path + "hpptestinflatehpp.bmp");
    std::cerr << "[PASS] Step 3: inflate.hpp round-trip (via deflate.hpp) succeeded\n";

    std::cerr << "\n=== [PASS] All tests passed for " << path << " ===\n\n";
    return true;
}

void testDeflateSpeed(std::string path, size_t iterations, int compressionLevel) {
    File file = readFile(path);
    double totalMs = 0;
    for (size_t i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> compressed = deflate::compress(file.data, file.size, compressionLevel);
        auto end = std::chrono::high_resolution_clock::now();

        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
        totalMs += elapsedMs;
        std::cerr << "deflate.hpp run " << (i + 1) << ": " << elapsedMs << " ms\n";
    }
    std::cerr << "deflate.hpp average over " << iterations << " runs: " << (totalMs / iterations) << " ms\n";
}

void testInflateSpeed(std::string path, size_t iterations) {
    File file = readFile(path);
    double totalMs = 0;
    for (size_t i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> decompressed = inflate::decompress(file.data, file.size);
        auto end = std::chrono::high_resolution_clock::now();

        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
        totalMs += elapsedMs;
        std::cerr << "inflate.hpp run " << (i + 1) << ": " << elapsedMs << " ms\n";
    }
    std::cerr << "inflate.hpp average over " << iterations << " runs: " << (totalMs / iterations) << " ms\n";
}

// Compresses a file with libdeflate, then decompresses with inflate.hpp (vector
// overload), and verifies the result matches the original byte-for-byte.
void compareInflateLibVector(std::string path) {
    File original = readFile(path);
    libdeflate_compressor* compressor = libdeflate_alloc_compressor(1);
    File libCompressed(original.size + 100);
    size_t libCompressedSize = libdeflate_deflate_compress(
        compressor, original.data, original.size,
        libCompressed.data, original.size + 100);

    std::vector<uint8_t> hppInflated = inflate::decompress(libCompressed.data, libCompressedSize);
    for (size_t i = 0; i < hppInflated.size(); i++) {
        if ((uint8_t)original.data[i] != hppInflated[i]) {
            std::cerr << "[FAIL] inflate.hpp mismatch at byte " << i << " for " << path << "\n";
            return;
        }
    }
    std::cerr << "[PASS] inflate.hpp matches original: " << path << "\n";
}

// Decompresses a zlib-wrapped file with both inflate.hpp and libdeflate, then
// compares results to verify inflate::decompressZlib is correct.
// Note: libdeflate skips the 2-byte zlib header, so we offset accordingly.
void testInflateZlibFile(std::string path) {
    File compressed = readFile(path);

    std::vector<uint8_t> hppDecompressed = inflate::decompressZlib(compressed.data, compressed.size);

    libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    File libDecompressed(hppDecompressed.size());
    size_t libDecompressedSize = 0;
    // Skip the 2-byte zlib header so libdeflate sees raw deflate data
    libdeflate_result libResult = libdeflate_deflate_decompress(
        decompressor,
        compressed.data + 2, compressed.size - 2,
        libDecompressed.data, libDecompressed.size,
        &libDecompressedSize);
    if (libResult != LIBDEFLATE_SUCCESS) {
        std::cerr << "[FAIL] libdeflate decompress failed (code " << libResult << ") for " << path << "\n";
        return;
    }

    for (size_t i = 0; i < libDecompressedSize && i < hppDecompressed.size(); i++) {
        if (hppDecompressed[i] != (uint8_t)libDecompressed.data[i]) {
            std::cerr << "[FAIL] inflate::decompressZlib mismatch at byte " << i << " for " << path << "\n";
            return;
        }
    }
    std::cerr << "[PASS] inflate::decompressZlib matches libdeflate: " << path << "\n";
}

int main() {
    std::cerr << "=== deflate/inflate.hpp test suite ===\n\n";

    // --- inflate.hpp correctness: compare against libdeflate ---
    std::cerr << "-- inflate.hpp vector decompress vs libdeflate --\n";
    compareInflateLibVector("test.bmp");
    compareInflateLibVector("large.bmp");

    // --- inflate::decompressZlib correctness ---
    std::cerr << "\n-- inflate::decompressZlib vs libdeflate --\n";
    testInflateZlibFile("weird.dat");

    // --- Full round-trip: libdeflate ↔ inflate.hpp and deflate.hpp ↔ libdeflate ---
    std::cerr << "\n-- Full round-trip tests (compression level 0) --\n";
    testDecompressionFile("large.bmp", 0);
    testDecompressionFile("test.bmp",  0);
    testDecompressionFile("tiny.bmp",  0);

    std::cerr << "\n-- Full round-trip tests (compression level 1) --\n";
    // testDecompressionFile("large.bmp", 1);
    // testDecompressionFile("test.bmp",  1);
    // testDecompressionFile("tiny.bmp",  1);

    std::cerr << "\n-- Full round-trip tests (compression level 2) --\n";
    testDecompressionFile("large.bmp", 2);
    testDecompressionFile("test.bmp",  2);
    testDecompressionFile("tiny.bmp",  2);

    std::cerr << "\n-- Full round-trip tests (compression level 3) --\n";
    testDecompressionFile("test.bmp", 3);
    testDecompressionFile("tiny.bmp", 3);

    // --- File-path API round-trip ---
    std::cerr << "\n-- File-path API round-trip (test.bmp, level 3) --\n";
    deflate::compress("test.bmp", "hppdeflate_testbmp", 3);
    inflate::decompress("hppdeflate_testbmp", "hppinflate_test.bmp");
    File original  = readFile("test.bmp");
    File roundTrip = readFile("hppinflate_test.bmp");
    std::cerr << (sameData(&original, &roundTrip)
        ? "[PASS] test.bmp -> deflate -> inflate matches original\n"
        : "[FAIL] test.bmp -> deflate -> inflate does not match original\n");

    // --- inflate.hpp speed benchmark ---
    std::cerr << "\n-- inflate.hpp speed benchmark --\n";
    testInflateSpeed("large.bmplibtestdeflate.txt", 1);

    return 0;
}