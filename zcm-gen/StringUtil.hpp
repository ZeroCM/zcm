#pragma once
#include "Common.hpp"
#include <sstream>

namespace StringUtil
{
    // Splits string with delimiter into a vector
    // Example:
    //    split("fg/kjh/i", '/') -> vector<string>{"fg", "kjh", "i"}
// This function is very useful for extracting delimited fields
    static vector<string> split(const string& str, char delimiter)
    {
        // TODO: if this function is ever too slow, it can be rewritten to loop throw
        // a c-string and set '\0' at every delimiter, and then .emplace those char*
        // directly into the vector<>
        // Alternatively, a special object could be created to cut down on malloc()
        // Only a single malloc of the string should be needed and then simply slice
        // up the memory. It could even work as an iterator in a lazy style
        // Something like python generators... Ideas Ideas.
        // However, it's unlikely any of this will be needed, but a boy can dream right?

        vector<string> v;
        std::stringstream ss {str};
        string tok;

        while(getline(ss, tok, delimiter))
            v.push_back(std::move(tok));

        // NOTE: getline always discards the delimiter, so without the following code
        //      split("f/k", '/')  -> vector<string>{"f", "k"}
        //      split("f/k/", '/') -> vector<string>{"f", "k"}
        //   Our semantics require:
        //      split("f/k", '/')  -> vector<string>{"f", "k"}
        //      split("f/k/", '/') -> vector<string>{"f", "k", ""}
        // So, we check for the original string ending in the delimiter, and fix up the vector
        auto len = str.size();
        if (len > 0 && str[len-1] == delimiter)
            v.push_back("");

        return v;
    }

    string toUpper(const string& s)
    {
        string ret = s;
        for (auto& c : ret)
            c = toupper(c);
        return ret;
    }
}
