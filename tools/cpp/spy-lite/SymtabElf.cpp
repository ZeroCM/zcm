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
    return 0;
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
const std::string& SymtabElf::getNext()
{
    static std::string emptystr;
    s.clear();

    while(1) {
        while(index < numread) {
            uint8_t c = buffer[index++];

            // null byte, end of a string?
            if (c == '\0') {
                if (s.size() > 0)
                    return s;
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

        // we've exhausted the buffer, let's try to get more...
        if (!refillBuffer()) {
            // end-of-file or error
            return emptystr;
        }
    }
}
