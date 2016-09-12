#include "multi_file.hpp"

// This file exists for the sole purpose of ensuring the zcm cpp header can be safely included
// in multiple files for a binary compilation. Previously, we saw compiler issue resulting
// from multiple definitions of zcm cpp functions. This was because some functions in zcm-cpp.hpp
// were not declared as "inline", so the were re-defined in both files. If you edit the
// zcm-cpp.hpp (or the zcm-cpp-impl.hpp) files, it is important that all functions are declared
// as inline.
#include <zcm/zcm-cpp.hpp>

#include <iostream>
using namespace std;

void MyStruct::greet(bool printStuff, const example_t* ex)
{
    if (printStuff) cout << "Hello " << example_t::test_const_float << endl;
    if (printStuff && ex) cout << "utime = " << ex->utime << endl;
}
