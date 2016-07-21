#pragma once
#include "Common.hpp"

struct SymtabElf
{
    SymtabElf(const std::string& libname);
    ~SymtabElf();

    bool good() { return f != NULL; }
    bool getNext(string& s);

private:
    bool getNextChar(char& c);
    bool refillBuffer();

private:
    FILE *f = NULL;
    static const size_t MaxBuf = 4096;
    char buffer[MaxBuf];
    size_t numread = 0;
    size_t index = 0;
};
