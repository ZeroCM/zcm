#pragma once

#include "types/example_t.hpp"

struct MyStruct {
    static void greet(bool printStuff=false, const example_t* ex = nullptr);
};
