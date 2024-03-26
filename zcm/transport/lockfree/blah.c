#include "lf_machine_assumptions.h"
int main()
{
    static_assert(1, "");
    static_assert(3 == 3, "");
}
