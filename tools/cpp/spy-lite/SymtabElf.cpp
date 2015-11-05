#include "SymtabElf.hpp"

SymtabElf::SymtabElf(const std::string& libname)
{
    f = fopen(libname.c_str(), "rb");
}

SymtabElf::~SymtabElf()
{
    fclose(f);
}

bool SymtabElf::refillBuffer()
{
    assert(numread == index);
    int n = fread(buffer, 1, sizeof(buffer), f);
    if(n == 0)
        return false;

    numread = n;
    index = 0;
    return true;
}

static inline int identChar(uint8_t ch)
{
    return isalpha(ch) || isdigit(ch) || ch == '_';
}

static inline int firstIdentChar(uint8_t ch)
{
    return isalpha(ch) || ch == '_';
}

// a very crude symbol extraction method
// we simply search for byte sequences that look like valid
// C language identifiers
bool SymtabElf::getNext(string& s)
{
    s.clear();

    char c;
    while (getNextChar(c)) {
        // null byte, end of a string?
        if (c == '\0') {
            if (s.size() > 0)
                return true;
        }

        // is it an identifier char?
        else if (s.size() > 0 && identChar(c)) {
            s.append(1, c);
        }

        else if(s.size() == 0 && firstIdentChar(c)) {
            s.append(1, c);
        }

        // otherwise its garbage, reset the string
        else {
            s.clear();
        }
    }

    return false;
}

bool SymtabElf::getNextChar(char& c)
{
    if (index == numread) {
        if (!refillBuffer()) {
            return false;
        }
    }
    c = buffer[index++];
    return true;
}
