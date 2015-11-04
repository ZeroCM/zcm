#pragma once
#include "Common.hpp"

struct SymtabElf
{
    SymtabElf(const std::string& libname);
    ~SymtabElf();

    bool good() { return f != NULL; }
    const std::string& getNext();

private:
    bool refillBuffer();

private:
    FILE *f = NULL;
    static const size_t MaxBuf = 256;
    char buffer[MaxBuf];
    size_t numread = 0;
    size_t index = 0;
    std::string s;
};
