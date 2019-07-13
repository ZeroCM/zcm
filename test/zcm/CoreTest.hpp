#ifndef CORETEST_HPP
#define CORETEST_HPP

#include <iostream>
#include <type_traits>
#include <vector>
#include <cassert>
#include <cstdbool>


#include "cxxtest/TestSuite.h"
#include "zcm/zcm_coretypes.h"

using namespace std;

static bool machineIsLittleEndian()
{
    int endianess_check = 1;
    return (*(char *)&endianess_check == 1);
}


#define makeTest(V, UNIQUE, CORETYPE) \
do { \
    const decltype(V) vec = V; \
    typedef decltype(vec)::value_type primitive; \
    int nbytes = sizeof(primitive) * vec.size(); \
 \
    decltype(V) vec0 = vec; \
    decltype(V) vec1 = vec; \
    vec1.push_back(UNIQUE); \
    decltype(V) vec2 = vec; \
    decltype(V) vec3 = vec; \
    decltype(V) vec4 = vec; \
    vec4.push_back(UNIQUE); \
 \
    primitive* arr0 = (primitive*) vec0.data(); \
    primitive* arr1 = (primitive*) vec1.data(); \
    primitive* arr2 = (primitive*) vec2.data(); \
    primitive* arr3 = (primitive*) vec3.data(); \
    primitive* arr4 = (primitive*) vec4.data(); \
 \
    for (size_t i = 0; i < vec.size(); ++i) arr0[i] = (primitive) vec[i]; \
 \
    TS_ASSERT_EQUALS(__ ## CORETYPE ## _encoded_array_size(arr0, vec.size()), nbytes); \
 \
    arr1[0] = UNIQUE; \
    TS_ASSERT_EQUALS(__ ## CORETYPE ## _encode_array(arr1, 1 * sizeof(primitive), \
                                                  nbytes, arr0, vec.size()), \
                  nbytes); \
    TS_ASSERT_EQUALS(arr1[0], UNIQUE); \
 \
 \
    TS_ASSERT_EQUALS(__ ## CORETYPE ## _encode_little_endian_array(arr4, 1 * sizeof(primitive), \
                                                                nbytes, arr0, vec.size()), \
                  nbytes); \
    for (size_t i = 0; i < vec.size(); ++i) { \
        if (machineIsLittleEndian()) { \
            primitive tmp = 0; \
            for (size_t j = 0; j < sizeof(tmp); ++j) \
                ((uint8_t*)&tmp)[j] |= ((uint8_t*)(&arr1[i + 1]))[sizeof(tmp) - j - 1]; \
            TS_ASSERT_EQUALS(tmp, vec[i]); \
            TS_ASSERT_EQUALS(arr4[i + 1], vec[i]); \
        } else { \
            primitive tmp = 0; \
            for (size_t j = 0; j < sizeof(tmp); ++j) \
                ((uint8_t*)&tmp)[j] |= ((uint8_t*)(&arr4[i + 1]))[sizeof(tmp) - j - 1]; \
            TS_ASSERT_EQUALS(arr1[i + 1], vec[i]); \
            TS_ASSERT_EQUALS(tmp, vec[i]); \
        } \
    } \
 \
    TS_ASSERT_EQUALS(__ ## CORETYPE ## _decode_array(arr1, 1 * sizeof(primitive), \
                                                  nbytes, arr2, vec.size()), \
                  nbytes); \
    for (size_t i = 0; i < vec.size(); ++i) TS_ASSERT_EQUALS(arr2[i], vec[i]); \
 \
    TS_ASSERT_EQUALS(__ ## CORETYPE ## _clone_array(arr2, arr3, vec.size()), nbytes); \
    for (size_t i = 0; i < vec.size(); ++i) TS_ASSERT_EQUALS(arr3[i], vec[i]); \
 \
} while(0)

class CoreTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testCore() {
        char *a = (char*) calloc(1, 2); a[0] = 'A';
        char *b = (char*) calloc(1, 2); b[0] = 'B';
        char *c = (char*) calloc(1, 2); c[0] = 'C';
        char *d = (char*) calloc(1, 2); d[0] = 'D';
        char *e = (char*) calloc(1, 2); e[0] = 'E';

        // TODO: disabled because I could not easily fix this one
        makeTest(((vector<uint8_t>){1, 2, 3, 4, 5, 6, 7, 8, 9,         10}),      11,     byte);
        makeTest(((vector<int8_t >){1, 2, 3, 4, 5, 6, 7, 8, 9,         10}),      11,   int8_t);
        makeTest(((vector<int16_t>){1, 2, 3, 4, 5, 6, 7, 8, 9,      31894}),      10,  int16_t);
        makeTest(((vector<int32_t>){1, 2, 3, 4, 5, 6, 7, 8, 9,     301894}),      10,  int32_t);
        makeTest(((vector<int64_t>){1, 2, 3, 4, 5, 6, 7, 8, 9, 3012351894}),      10,  int64_t);
        makeTest(((vector<float  >){1, 2, 3, 4, 5, 6, 7, 8, 9,         10}),      11,    float);
        makeTest(((vector<double >){1, 2, 3, 4, 5, 6, 7, 8, 9,         10}),      11,   double);

        free(a);
        free(b);
        free(c);
        free(d);
        free(e);
    }
};

#endif // CORETEST_HPP
