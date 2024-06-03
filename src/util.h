#ifndef UTIL_H
#define UTIL_H

#include <cstddef>
#include <string>

class DynamicArray {
public:
    DynamicArray(size_t initialSize = 10);
    ~DynamicArray();
    void add(const std::string& token);
    std::string* getData() const;
    size_t getSize() const;

private:
    void resize();
    size_t size;
    size_t capacity;
    std::string* data;
};

void splitString(const std::string& str, char delimiter, DynamicArray& tokens);

#endif