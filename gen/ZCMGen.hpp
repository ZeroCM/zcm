#pragma once
#include "Common.hpp"
#include "GetOpt.hpp"

extern "C" {
#include "getopt.h"
}

struct ZCMGen;

struct ZCMTypename
{
    string fullname;  // fully-qualified name, e.g., "com.foo.prj.laser"

    string package;   // package name, e.g., "com.foo.prj"
    string shortname; // e.g., "laser"

    ZCMTypename(ZCMGen& zcmgen, const string& lctypename);
    void dump();

    static bool isSame(const ZCMTypename& a, const ZCMTypename& b)
    { return a.fullname == b.fullname; }
};

enum ZCMDimensionMode {
    ZCM_CONST,
    ZCM_VAR,
};

struct ZCMDimension
{
	ZCMDimensionMode mode;
	string size;      // a string containing either a member variable name or a constant
};

struct ZCMMember
{
    ZCMTypename    type;
    string         membername;

	// A scalar is a 1-dimensional array of length 1.
    vector<ZCMDimension> dimensions;

    // Comments in the ZCM type definition immediately before a member
    // declaration are attached to that member
    string comment;

    // Are all of the dimensions of this array constant? (scalars return true)
    bool isConstantSizeArray();

    void dump();
    ZCMMember(const ZCMTypename& type, const string& membername) :
        type(type), membername(membername)
    {}
};

struct ZCMConstant
{
    string type;    // int8_t / int16_t / int32_t / int64_t / float / double
    string membername;
    union {
        int8_t  i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;
        float   f;
        double  d;
    } val;
    string valstr;   // value as a string, as specified in the .zcm file

    // Comments in the ZCM type definition immediately before a constant are
    // attached to the constant.
    string comment;

    ZCMConstant(const string& type, const string& name, const string& valstr);
};

struct ZCMStruct
{
    ZCMTypename          structname; // name of the data type
    vector<ZCMMember>    members;

    // recursive declaration of structs and enums
    vector<ZCMStruct>    structs;
    vector<ZCMConstant>  constants;

    string               zcmfile; // file/path of function that declared it
    i64                  hash;

    // Comments in the ZCM type defition immediately before a struct is declared
    // are attached to that struct.
    string               comment;


    //////////////////////////////////////////////////////////////////////////////
    static inline string dotsToUnderscores(const string& s)
    {
        string ret = s;
        for (uint i = 0; i < ret.size(); i++)
            if (ret[i] == '.')
                ret[i] = '_';
        return ret;
    }
    string underscore;
    bool hasUnderscore = false;
    const string& nameUnderscore()
    {
        if (!hasUnderscore) {
            hasUnderscore = true;
            underscore = dotsToUnderscores(structname.fullname);
        }
        return underscore;
    }
    const char *nameUnderscoreCStr()
    {
        return nameUnderscore().c_str();
    }
    //////////////////////////////////////////////////////////////////////////////

    // Returns the member of a struct by name. Returns NULL on error.
    ZCMMember *findMember(const string& name);

    // Returns the constant of a struct by name. Returns NULL on error.
    ZCMConstant *findConst(const string& name);

    i64 computeHash();

    void dump();
    ZCMStruct(ZCMGen& zcmgen, const string& zcmfile, const string& structname);
};

struct ZCMGen
{
    string             package; // remembers the last-specified package name, which is prepended to other types.
    GetOpt             *gopt = nullptr;
    vector<ZCMStruct>  structs;

    string             comment;

    // create a new parsing context.
    ZCMGen();

    // Returns true if the "lazy" option is enabled AND the file "outfile" is
    // older than the file "declaringfile"
    bool needsGeneration(const string& declaringfile, const string& outfile);

    // for debugging, emit the contents to stdout
    void dump();

    // parse the provided file
    int handleFile(const string& path);

    // Returns true if the argument is a built-in type (e.g., "int64_t", "float").
    static bool isPrimitiveType(const string& t);

    // Returns true if the argument is a built-in type usable as and array dim
    static bool isArrayDimType(const string& t);

    // Returns true if the argument is a legal constant type (e.g., "int64_t", "float").
    static bool isLegalConstType(const string& t);
};
