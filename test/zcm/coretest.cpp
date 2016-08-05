#include <iostream>
#include <type_traits>
#include <vector>
#include <cassert>
#include <cstdbool>

#include "zcm/zcm_coretypes.h"

using namespace std;

#define assert_equals(A,B) \
    __assert_equals(A, B, #A, #B, __LINE__)

void __assert_equals(const uint64_t& A, const uint64_t& B, const char* a, const char* b, int linenum)
{
    if (A != B) {
        cerr << "Assertion on line " << linenum << ": "
             << a << " != " << b << "   :   " << A << " != " << B << endl;
        //exit(1);
    }
}

static bool machineIsLittleEndian()
{
    int endianess_check = 1;
    return (*(char *)&endianess_check == 1);
}

#define makeTest(V, TYPE, UNIQUE) \
do { \
    const decltype(V) vec = V; \
    typedef decltype(vec)::value_type primitive; \
    int nbytes = sizeof(primitive) * vec.size(); \
 \
    assert_equals(__ ## TYPE ## _encoded_array_size(NULL, 10), nbytes); \
 \
    primitive* arr0 = (primitive*) malloc(nbytes); \
    primitive* arr1 = (primitive*) malloc(nbytes + (1 * sizeof(primitive))); \
    primitive* arr2 = (primitive*) malloc(nbytes); \
    primitive* arr3 = (primitive*) malloc(nbytes); \
    primitive* arr4 = (primitive*) malloc(nbytes + (1 * sizeof(primitive))); \
 \
    for (size_t i = 0; i < vec.size(); ++i) arr0[i] = vec[i]; \
    arr1[0] = UNIQUE; \
 \
    assert_equals(__ ## TYPE ## _encode_array(arr1, 1 * sizeof(primitive), \
                                              nbytes, arr0, vec.size()), \
                  nbytes); \
 \
    assert_equals(__ ## TYPE ## _encode_little_endian_array(arr4, 1 * sizeof(primitive), \
                                                            nbytes, arr0, vec.size()), \
                  nbytes); \
 \
    assert_equals((int) arr1[0], UNIQUE); \
    for (int i = 0; i < (int) vec.size(); ++i) { \
        if (machineIsLittleEndian()) { \
            primitive tmp = 0; \
            for (size_t j = 0; j < sizeof(tmp); ++j) \
                tmp |= ((uint64_t)(((uint8_t*)(&arr1[i + 1]))[j]) << ((sizeof(tmp) - j - 1) * 8)); \
            assert_equals((int) tmp, vec[i]); \
            assert_equals((int) arr4[i + 1], vec[i]); \
        } else { \
            primitive tmp = 0; \
            for (size_t j = 0; j < sizeof(tmp); ++j) \
                tmp |= ((uint64_t)(((uint8_t*)(&arr4[i + 1]))[j]) << ((sizeof(tmp) - j - 1) * 8)); \
            assert_equals((int) arr1[i + 1], vec[i]); \
            assert_equals((int) tmp, vec[i]); \
        } \
    } \
 \
    assert_equals(__ ## TYPE ## _decode_array(arr1, 1 * sizeof(primitive), \
                                              nbytes, arr2, vec.size()), \
                  nbytes); \
    for (int i = 0; i < (int) vec.size(); ++i) assert_equals((int) arr2[i], vec[i]); \
 \
    assert_equals(__ ## TYPE ## _clone_array(arr2, arr3, vec.size()), nbytes); \
    for (int i = 0; i < (int) vec.size(); ++i) assert_equals((int) arr3[i], vec[i]); \
 \
    free(arr0); \
    free(arr1); \
    free(arr2); \
    free(arr3); \
    free(arr4); \
} while(0)

int main(int argc, char* argv[])
{
    makeTest(((vector<uint8_t>){1, 2, 3, 4, 5, 6, 7, 8, 9,    10}),    byte, 11);
    makeTest(((vector<int8_t >){1, 2, 3, 4, 5, 6, 7, 8, 9,    10}),  int8_t, 11);
    makeTest(((vector<int16_t>){1, 2, 3, 4, 5, 6, 7, 8, 9, 31894}), int16_t, 10);

    return 0;
}
