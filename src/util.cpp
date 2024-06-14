#include "util.h"
#include <sstream>
#include <iostream>
#include <cstring>
#include <zlib.h>

DynamicArray::DynamicArray(size_t initialSize)
    : size(0), capacity(initialSize), data(new std::string[initialSize]) {}

DynamicArray::~DynamicArray() {
    delete[] data;
}

void DynamicArray::add(const std::string& token) {
    if (size >= capacity) {
        resize();
    }
    data[size++] = token;
}

std::string* DynamicArray::getData() const {
    return data;
}

size_t DynamicArray::getSize() const {
    return size;
}

void DynamicArray::resize() {
    capacity *= 2;
    std::string* newData = new std::string[capacity];
    for (size_t i = 0; i < size; ++i) {
        newData[i] = std::move(data[i]);
    }
    delete[] data;
    data = newData;
}

bool DynamicArray::contains(const std::string& token) const {
    for (size_t i = 0; i < size; ++i) {
        if (data[i] == token) {
            return true;
        }
    }
    return false;
}

void splitString(const std::string& str, char delimiter, DynamicArray& tokens) {
    std::string token;
    std::stringstream ss(str);
    
    while (std::getline(ss, token, delimiter)) {
        tokens.add(lstrip(token));
    }
}

std::string lstrip(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(str[start])) {
        ++start;
    }
    return str.substr(start);
}

bool gzipCompress(const char* inputData, size_t inputSize, char* &outputData, size_t& outputSize) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        std::cerr << "deflateInit failed while compressing." << std::endl;
        return false;
    }

    zs.next_in = (Bytef*) inputData;
    zs.avail_in = inputSize;
    
    size_t bufferSize = inputSize * 3 + 12; // Initial buffer size
    char* bufferData = new char[bufferSize];
    
    zs.next_out = (Bytef*) bufferData;
    zs.avail_out = bufferSize;

    int ret = deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        std::cerr << "Exception during zlib compression: (" << ret << ") " << zs.msg << std::endl;
        return false;
    }

    outputSize = zs.total_out;

    // Resize the buffer to the actual output size
    outputData = new char[outputSize];
    memcpy(outputData, bufferData, outputSize);
    
    delete[] bufferData;
    return true;
}
