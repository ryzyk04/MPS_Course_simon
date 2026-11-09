#pragma once

#include <stdint.h>

#include <stdexcept>
#include <string>
#include <iostream>

inline uint64_t checks = 0;
inline unsigned cases = 0;

inline void check(bool condition, const char *text, int line)
{
    ++checks;
    if (!condition) {
        throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + text);
    }
}

#define CHECK(condition) check(bool(condition), #condition, __LINE__)

template<class Function>
void run(const char *name, Function function)
{
    function();
    ++cases;
    std::cout << "PASS " << name << "\n";
}
