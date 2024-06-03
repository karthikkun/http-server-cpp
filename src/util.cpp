#include "util.h"
#include <sstream>

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

void splitString(const std::string& str, char delimiter, DynamicArray& tokens) {
    std::string token;
    std::stringstream ss(str);
    
    while (std::getline(ss, token, delimiter)) {
        tokens.add(token);
    }
}
