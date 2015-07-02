#ifndef _ZCM_H
#define _ZCM_H

#include <stdint.h>
#include <glib.h>

#include "getopt.h"

/////////////////////////////////////////////////

#ifndef g_ptr_array_size
#define g_ptr_array_size(x) ((x)->len)
#endif

/////////////////////////////////////////////////
// zcm_typename_t: represents the name of a type, including package
//
//	Originally, the first field in the zcm_typename was named typename - which is a C++
//	keyword and caused much grief. Renamed to lctypename.
typedef struct zcm_typename zcm_typename_t;

struct zcm_typename
{
  	char *lctypename; // fully-qualified name, e.g., "edu.mit.dgc.laser_t"
    char *package;    // package name, e.g., "edu.mit.dgc"
    char *shortname;  // e.g., "laser_t"
};

/////////////////////////////////////////////////
// zcm_dimension_t: represents the size of a dimension of an
//                  array. The size can be either dynamic (a variable)
//                  or a constant.
//
typedef enum { ZCM_CONST, ZCM_VAR } zcm_dimension_mode_t;

typedef struct zcm_dimension zcm_dimension_t;

struct zcm_dimension
{
	zcm_dimension_mode_t mode;
	char *size;                // a string containing either a member variable name or a constant
};

/////////////////////////////////////////////////
// zcm_member_t: represents one member of a struct, including (if its
//               an array), its dimensions.
//
typedef struct zcm_member zcm_member_t;

struct zcm_member
{
    zcm_typename_t *type;
	char           *membername;

	// an array of zcm_dimension_t. A scalar is a 1-dimensional array
	// of length 1.
	GPtrArray *dimensions;

  // Comments in the ZCM type definition immediately before a member
  // declaration are attached to that member
  char* comment;
};

/////////////////////////////////////////////////
// zcm_struct_t: a first-class ZCM object declaration
//
typedef struct zcm_struct zcm_struct_t;

struct zcm_struct
{
  zcm_typename_t *structname; // name of the data type

  GPtrArray *members;  // zcm_member_t

  // recursive declaration of structs and enums
  GPtrArray *structs;  // zcm_struct_t
  GPtrArray *enums;    // locally-declared enums  DEPRECATED
  GPtrArray *constants; // zcm_constant_t

  char *zcmfile;       // file/path of function that declared it
  int64_t hash;

  // Comments in the ZCM type defition immediately before a struct is declared
  // are attached to that struct.
  char* comment;
};

/////////////////////////////////////////////////
// zcm_constant_: the symbolic name of a constant and its value.
//
typedef struct zcm_constant zcm_constant_t;

struct zcm_constant
{
  char *lctypename;    // int8_t / int16_t / int32_t / int64_t / float / double
  char *membername;
  union {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    float f;
    double d;
  } val;
  char *val_str;   // value as a string, as specified in the .zcm file

  // Comments in the ZCM type definition immediately before a constant are
  // attached to the constant.
  char* comment;
};

/////////////////////////////////////////////////
// DEPRECATED
// zcm_enum_value_t: the symbolic name of an enum and its constant
//                   value.
//
typedef struct zcm_enum_value zcm_enum_value_t;

struct zcm_enum_value
{
	char    *valuename;
	int32_t value;
};

/////////////////////////////////////////////////
// DEPRECATED
// zcm_enum_t: an enumeration, also a first-class ZCM object.
//
typedef struct zcm_enum zcm_enum_t;
struct zcm_enum
{
    zcm_typename_t *enumname; // name of the enum

	GPtrArray *values;   // legal values for the enum
	char *zcmfile;      // file/path of function that declared it

    // hash values for enums are "weak". They only involve the name of the enum,
    // so that new enumerated values can be added without breaking the hash.
    int64_t hash;
};

/////////////////////////////////////////////////
// zcmgen_t: State used when parsing ZCM declarations. The gopt is
//           essentially a set of key-value pairs that configure
//           various options. structs and enums are populated
//           according to the parsed definitions.
//
typedef struct zcmgen zcmgen_t;

struct zcmgen
{
    char      *package; // remembers the last-specified package name, which is prepended to other types.
    getopt_t  *gopt;
    GPtrArray *structs; // zcm_struct_t
    GPtrArray *enums;   // zcm_enum_t (declared at top level)

    gchar* comment_doc;
};

/////////////////////////////////////////////////
// Helper functions
/////////////////////////////////////////////////

// Returns 1 if the argument is a built-in type (e.g., "int64_t", "float").
int zcm_is_primitive_type(const char *t);

// Returns 1 if the argument is a legal constant type (e.g., "int64_t", "float").
int zcm_is_legal_const_type(const char *t);

// Returns the member of a struct by name. Returns NULL on error.
zcm_member_t *zcm_find_member(zcm_struct_t *lr, const char *name);

// Returns the constant of a struct by name. Returns NULL on error.
zcm_constant_t *zcm_find_const(zcm_struct_t *lr, const char *name);

// Returns 1 if the "lazy" option is enabled AND the file "outfile" is
// older than the file "declaringfile"
int zcm_needs_generation(zcmgen_t *zcmgen, const char *declaringfile, const char *outfile);

// create a new parsing context.
zcmgen_t *zcmgen_create();

// for debugging, emit the contents to stdout
void zcmgen_dump(zcmgen_t *zcm);

// parse the provided file
int zcmgen_handle_file(zcmgen_t *zcm, const char *path);

// Are all of the dimensions of this array constant?
// (scalars return 1)
int zcm_is_constant_size_array(zcm_member_t *lm);

#endif
