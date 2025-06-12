# deflate.hpp
A simple and efficient C++ implementation of the DEFLATE compression and INFLATE decompression algorithms.


### Features

* **Simple API:** Easy to integrate `deflate` and `inflate` functionality into your C++ projects.
* **Two Compression Levels:** Choose between faster compression or better compression ratios for your needs.
* **Pure C++:** No external dependencies, making it highly portable.

### Good to Know
* Just throw the include directory in your project as an include directory, no other dependencies
* Targets C++17
* Building this repo will just give the tests
* Want to know more details how to use functions? Look at libdeflate_test.cpp or example.cpp in tests folder

### To Use Deflate

    Include deflate.hpp.
    Call deflate::compress.
    Deflate offers two levels of compression; better compression requires significantly more time.
    Modify the level with the bool at end of compression call parameters, true will enable the slower compression

### To Use Inflate

    Include inflate.hpp.
    Call inflate::decompress.
