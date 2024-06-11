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
