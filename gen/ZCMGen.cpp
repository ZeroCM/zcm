#include "ZCMGen.hpp"
#include <cmath>
#include <unordered_map>

extern "C" {
#include "tokenize.h"
}

/** The ZCM grammar is implemented here with a recursive-descent parser.
    handle_file is the top-level function, which calls parse_struct/parse_enum,
    and so on.

    Every ZCM type has an associated "signature", which is a hash of
    various components of its delcaration. If the declaration of a
    signature changes, the hash changes with high probability.

    Note that this implementation is sloppy about memory allocation:
    we don't worry about freeing memory since the program will exit
    after parsing anyway.
**/


//zcm_struct_t* parse_struct(zcmgen_t* zcm, const char* zcmfile, tokenize_t* t);
//zcm_enum_t* parse_enum(zcmgen_t* zcm, const char* zcmfile, tokenize_t* t);

// zcm's built-in types. Note that unsigned types are not present
// because there is no safe java implementation. Really, you don't
// want to add unsigned types.
static vector<string> primitiveTypes {
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "byte",
    "float",
    "double",
    "string",
    "boolean"
};

// which types can be legally used as array dimensions?
static vector<string> arrayDimTypes {
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t"
};

// which types can be legally used as const values?
static vector<string> constTypes {
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "float",
    "double"
};

static vector<string> fixedPointTypes {
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t"
};

static unordered_map<string, size_t> primitiveTypeSize {
    {"int8_t",  1},
    {"int16_t", 2},
    {"int32_t", 4},
    {"int64_t", 8},
    {"byte",    1},
    {"float",   4},
    {"double",  8},
    {"boolean", 1}
};

// Given NULL-terminated array of strings "ts", does "t" appear in it?
static bool inArray(const vector<string>& ts, const string& t)
{
    return std::find(begin(ts), end(ts), t) != end(ts);
}

bool   ZCMGen::isPrimitiveType(const string& t)       { return inArray(primitiveTypes, t); }
bool   ZCMGen::isArrayDimType(const string& t)        { return inArray(arrayDimTypes, t);  }
bool   ZCMGen::isLegalConstType(const string& t)      { return inArray(constTypes, t);     }
size_t ZCMGen::getPrimitiveTypeSize(const string& tn)
{
    auto iter = primitiveTypeSize.find(tn);
    if (iter == primitiveTypeSize.end()) {
        fprintf(stderr, "Unable to find size of primitive type: %s\n", tn.c_str());
        exit(1);
    }
    return iter->second;
}

static bool isLegalMemberName(const string& t)
{
    return isalpha(t[0]) || t[0] == '_';
}

static u64 signExtendedRightShift(u64 val, size_t nShift)
{
    return !(val >> 63) ?  val >> nShift :
                          (val >> nShift) | ~((1 << (64 - nShift)) - 1);
}

// Make the hash dependent on the value of the given character. The
// order that hash_update is called in IS important.
static u64 hashUpdate(u64 v, char c)
{
    v = ((v<<8) ^ signExtendedRightShift(v, 55)) + c;
    return v;
}

// Make the hash dependent on each character in a string.
static u64 hashUpdate(u64 v, const string& s)
{
    v = hashUpdate(v, s.size());
    for (auto& c : s)
        v = hashUpdate(v, c);
    return v;
}

// Create a parsing context
ZCMGen::ZCMGen()
{}

// Parse a type into package and class name.
ZCMTypename::ZCMTypename(ZCMGen& zcmgen, const string& name)
{
    ZCMTypename& t = *this;

    const string& packagePrefix = zcmgen.gopt->getString("package-prefix");

    t.fullname = name;

    // package name: everything before the last ".", or "" if there is no "."
    //
    // shortname: everything after the last ".", or everything if
    // there is no "."
    //
    size_t dot = name.rfind('.');
    if (dot == string::npos) {
        t.package = "";
        t.shortname = name;
    } else {
        t.package = name.substr(0, dot);
        t.shortname = name.substr(dot + 1);
    }

    if (packagePrefix.size() > 0 && !ZCMGen::isPrimitiveType(t.shortname)) {
        if (t.package.size() > 0) {
            t.package = packagePrefix + "." + t.package;
        } else {
            t.package = packagePrefix;
        }
        t.fullname = t.package + "." + t.shortname;
    }
}

ZCMStruct::ZCMStruct(ZCMGen& zcmgen, const string& zcmfile, const string& structname) :
    structname(zcmgen, structname),
    zcmfile(zcmfile)
{}

ZCMConstant::ZCMConstant(const string& type, const string& name, const string& valstr) :
    type(type), membername(name), valstr(valstr)
{}

bool ZCMConstant::isFixedPoint() { return inArray(fixedPointTypes, type); }

u64 ZCMStruct::computeHash()
{
    u64 v = 0x12345678;

    #ifdef ENABLE_TYPENAME_HASHING
    v = hashUpdate(v, structname.shortname);
    #endif

    for (auto& m : members) {

        #ifdef ENABLE_MEMBERNAME_HASHING
        // hash the member name
        v = hashUpdate(v, m.membername);
        #endif

        // if the member is a primitive type, include the type
        // signature in the hash. Do not include them for compound
        // members, because their contents will be included, and we
        // don't want a struct's name change to break the hash.
        if (ZCMGen::isPrimitiveType(m.type.fullname))
            v = hashUpdate(v, m.type.fullname);

        // hash the dimensionality information
        v = hashUpdate(v, m.dimensions.size());
        for (auto& dim : m.dimensions) {
            v = hashUpdate(v, dim.mode);
            v = hashUpdate(v, dim.size);
        }
    }

    return v;
}

// semantic error: it parsed fine, but it's illegal. (we don't try to
// identify the offending token). This function does not return.
static void semantic_error(tokenize_t* t, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    printf("\n");
    vprintf(fmt, ap);
    printf("\n");

    printf("%s : %i\n", t->path, t->token_line);
    printf("%s", t->buffer);

    va_end(ap);
    fflush(stdout);
    exit(1);
}

// semantic warning: it parsed fine, but it's dangerous.
static void semantic_warning(tokenize_t* t, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    printf("\n");
    vprintf(fmt, ap);
    printf("\n");

    printf("%s : %i\n", t->path, t->token_line);
    printf("%s", t->buffer);

    va_end(ap);
}

// parsing error: we cannot continue. This function does not return.
static void parse_error(tokenize_t* t, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    printf("\n");
    vprintf(fmt, ap);
    printf("\n");

    printf("%s : %i\n", t->path, t->token_line);
    printf("%s", t->buffer);
    for (int i = 0; i < t->token_column; i++) {
        if (isspace(t->buffer[i]))
            printf("%c", t->buffer[i]);
        else
            printf(" ");
    }
    printf("^\n");

    va_end(ap);
    fflush(stdout);
    exit(1);
}

// Consume any available comments and store them in zcmgen->comment_doc
static void parseTryConsumeComment(tokenize_t* t, ZCMGen* zcmgen = NULL)
{
    if (zcmgen)
        zcmgen->comment = "";

    while (tokenize_peek(t) != EOF && t->token_type == ZCM_TOK_COMMENT) {
        tokenize_next(t);

        if (zcmgen) {
            if (zcmgen->comment != "")
                zcmgen->comment += "\n";
            zcmgen->comment += t->token;
        }
    }
}

// If the next non-comment token is "tok", consume it and return 1. Else, return 0
static int parseTryConsume(tokenize_t* t, const char* tok)
{
    parseTryConsumeComment(t);
    int res = tokenize_peek(t);
    if (res == EOF)
        parse_error(t, "End of file while looking for %s.", tok);

    res = (t->token_type != ZCM_TOK_COMMENT && !strcmp(t->token, tok));

    // consume if the token matched
    if (res)
        tokenize_next(t);

    return res;
}

// Consume the next token. If it's not "tok", an error is emitted and
// the program exits.
static void parseRequire(tokenize_t* t, const char* tok)
{
    parseTryConsumeComment(t);
    int res;
    do {
        res = tokenize_next(t);
    } while (t->token_type == ZCM_TOK_COMMENT);

    if (res == EOF || strcmp(t->token, tok))
        parse_error(t, "expected token %s", tok);
}

// require that the next token exist (not EOF). Description is a
// human-readable description of what was expected to be read.
static void tokenizeNextOrFail(tokenize_t* t, const char* description)
{
    int res = tokenize_next(t);
    if (res == EOF)
        parse_error(t, "End of file reached, expected %s.", description);
}

static int parseConst(ZCMGen& zcmgen, ZCMStruct& lr, tokenize_t* t)
{
    parseTryConsumeComment(t);
    tokenizeNextOrFail(t, "type identifier");

    // get the constant type
    if (!ZCMGen::isLegalConstType(t->token))
        parse_error(t, "invalid type for const");

    string lctypename = t->token;

    do {
        // get the member name
        parseTryConsumeComment(t);
        tokenizeNextOrFail(t, "name identifier");
        if (!isLegalMemberName(t->token))
            parse_error(t, "Invalid member name: must start with [a-zA-Z_].");

        string membername = t->token;

        // make sure this name isn't already taken.
        if (lr.findMember(t->token))
            semantic_error(t, "Duplicate member name '%s'.", t->token);
        if (lr.findConst(t->token))
            semantic_error(t, "Duplicate member name '%s'.", t->token);

        // get the value
        parseRequire(t, "=");
        parseTryConsumeComment(t);
        tokenizeNextOrFail(t, "constant value");

        // create a new const member
        ZCMConstant lc {lctypename, membername, t->token};

        // Attach the last comment if one was defined.
        if (zcmgen.comment != "")
            std::swap(lc.comment, zcmgen.comment);

        // TODO: Rewrite all of this in a much cleaner C++ style approach
        // TODO: This should migrate to either the ctor or a helper function called just
        //       before the ctor
        char* endptr = NULL;
        #define INT_CASE(TYPE, STORE) \
            } else if (lctypename == #TYPE) { \
                long long v = strtoll(t->token, &endptr, 0); \
                if (endptr == t->token || *endptr != '\0') \
                    parse_error(t, "Expected integer value"); \
                if (strlen(t->token) > 2 && \
                        t->token[0] == '0' && (t->token[1] == 'x' || t->token[1] == 'X')) { \
                    if (strlen(t->token) > sizeof(TYPE) * 2 + 2) \
                        semantic_error(t, "Too many hex digits specified" \
                                          #TYPE ": %lld", v); \
                } else if (v < std::numeric_limits<TYPE>::min() || \
                           v > std::numeric_limits<TYPE>::max()) { \
                    semantic_error(t, "Integer value out of bounds for " \
                                      #TYPE ": %lld", v); \
                } \
                STORE = (TYPE) v;

        #define FLT_CASE(TYPE, STORE) \
            } else if (lctypename == #TYPE) { \
                double v = strtod(t->token, &endptr); \
                if (endptr == t->token || *endptr != '\0') \
                    parse_error(t, "Expected floating point value"); \
                if (fabs(v) > std::numeric_limits<TYPE>::max() || \
                        fabs(v) < std::numeric_limits<TYPE>::min()) \
                    semantic_error(t, "Cannot represent precision"); \
                STORE = (TYPE) v;

        if (false) {
        INT_CASE(int8_t,  lc.val.i8)
        INT_CASE(int16_t, lc.val.i16)
        INT_CASE(int32_t, lc.val.i32)
        INT_CASE(int64_t, lc.val.i64)
        FLT_CASE(float,   lc.val.f)
        FLT_CASE(double,  lc.val.d)
        } else {
            fprintf(stderr, "[%s]\n", t->token);
            assert(0);
        }

        lr.constants.push_back(std::move(lc));

    } while (parseTryConsume(t, ","));

    parseRequire(t, ";");
    return 0;
}

// parse a member declaration. This looks long and scary, but most of
// the code is for semantic analysis (error checking)
static int parseMember(ZCMGen& zcmgen, ZCMStruct& lr, tokenize_t* t)
{
    // Read a type specification. Then read members (multiple
    // members can be defined per-line.) Each member can have
    // different array dimensionalities.

    // inline type declaration?
    if (parseTryConsume(t, "struct")) {
        parse_error(t, "recursive structs not implemented.");
    } else if (parseTryConsume(t, "const")) {
        return parseConst(zcmgen, lr, t);
    }

    // standard declaration
    parseTryConsumeComment(t);
    tokenizeNextOrFail(t, "type identifier");

    if (!isLegalMemberName(t->token))
        parse_error(t, "invalid type name");

    // A common mistake is use 'int' as a type instead of 'intN_t'
    if (string(t->token) == "int")
        semantic_warning(t, "int type should probably be int8_t, int16_t, int32_t, or int64_t");

    ZCMTypename lt {zcmgen, t->token};

    do {
        // get the zcm type name
        parseTryConsumeComment(t);
        tokenizeNextOrFail(t, "name identifier");

        if (!isLegalMemberName(t->token))
            parse_error(t, "Invalid member name: must start with [a-zA-Z_].");

        // make sure this name isn't already taken.
        if (lr.findMember(t->token))
            semantic_error(t, "Duplicate member name '%s'.", t->token);
        if (lr.findConst(t->token))
            semantic_error(t, "Duplicate member name '%s'.", t->token);

        // create a new member
        ZCMMember lm { lt, t->token };
        if (zcmgen.comment != "")
            std::swap(lm.comment, zcmgen.comment);

        // (multi-dimensional) array declaration?
        while (parseTryConsume(t, "[")) {

            // pull out the size of the dimension, either a number or a variable name.
            parseTryConsumeComment(t);
            tokenizeNextOrFail(t, "array size");

            ZCMDimension dim;

            if (ZCMConstant* c = lr.findConst(t->token)) {
                if (!ZCMGen::isArrayDimType(c->type))
                    semantic_error(t, "Array dimension '%s' must be an integer type.", t->token);

                dim.mode = ZCM_CONST;
                dim.size = c->valstr;
            } else {
                if (isdigit(t->token[0])) {
                    // we have a constant size array declaration.
                    int sz = strtol(t->token, NULL, 0);
                    if (sz <= 0)
                        semantic_error(t, "Constant array size must be > 0");
                    dim.mode = ZCM_CONST;
                    dim.size = t->token;
                } else {
                    // we have a variable sized declaration.
                    if (t->token[0]==']')
                        semantic_error(t, "Array sizes must be declared either as a constant or variable.");
                    if (!isLegalMemberName(t->token))
                        semantic_error(t, "Invalid array size variable name: must start with [a-zA-Z_].");

                    // make sure the named variable is
                    // 1) previously declared and
                    // 2) an integer type
                    int okay = 0;

                    for (auto& thislm : lr.members) {
                        if (thislm.membername == t->token) {
                            if (thislm.dimensions.size() != 0)
                                semantic_error(t, "Array dimension '%s' must be not be an array type.", t->token);
                            if (!ZCMGen::isArrayDimType(thislm.type.fullname))
                                semantic_error(t, "Array dimension '%s' must be an integer type.", t->token);
                            okay = 1;
                            break;
                        }
                    }

                    if (!okay)
                        semantic_error(t, "Unknown variable array index '%s'. Index variables must be declared before the array.", t->token);

                    dim.mode = ZCM_VAR;
                    dim.size = t->token;
                }
            }
            parseRequire(t, "]");

            // increase the dimensionality of the array by one dimension.
            lm.dimensions.push_back(std::move(dim));
        }

        lr.members.push_back(std::move(lm));

    } while(parseTryConsume(t, ","));

    parseRequire(t, ";");

    return 0;
}

/** assume the "struct" token is already consumed **/
static ZCMStruct parseStruct(ZCMGen& zcmgen, const string& zcmfile,
                             const string& package, tokenize_t* t)
{
    parseTryConsumeComment(t);
    tokenizeNextOrFail(t, "struct name");

    string name = t->token;

    ZCMStruct lr {zcmgen, zcmfile, (package == "") ? name : package + "." + name};
    if (zcmgen.comment != "")
        std::swap(lr.comment, zcmgen.comment);

    parseRequire(t, "{");

    while (!parseTryConsume(t, "}")) {
        // Check for leading comments that will be used to document the member.
        parseTryConsumeComment(t, &zcmgen);

        if (parseTryConsume(t, "}"))
            break;

        parseMember(zcmgen, lr, t);
    }

    lr.hash = lr.computeHash();
    return lr;
}

static const ZCMStruct* findStruct(ZCMGen& zcmgen, const string& package, const string& name)
{
    for (auto& lr : zcmgen.structs) {
        if (package == lr.structname.package &&
            name == lr.structname.shortname)
            return &lr;
    }
    return nullptr;
}

/** parse entity (top-level construct), return EOF if eof. **/
static int parseEntity(ZCMGen& zcmgen, const string& zcmfile, tokenize_t* t)
{
    parseTryConsumeComment(t, &zcmgen);

    int res = tokenize_next(t);
    if (res == EOF)
        return EOF;

    if (string(t->token) == "package") {
        parseTryConsumeComment(t, &zcmgen);
        tokenizeNextOrFail(t, "package name");
        zcmgen.package = t->token;
        parseRequire(t, ";");
        return 0;
    }

    if (string(t->token) ==  "struct") {
        ZCMStruct lr = parseStruct(zcmgen, zcmfile, zcmgen.package, t);

        // check for duplicate types
        auto* prior = findStruct(zcmgen,
                                 lr.structname.package,
                                 lr.structname.shortname);
        if (prior) {
            printf("ERROR:  duplicate type %s declared in %s\n",
                   lr.structname.fullname.c_str(), zcmfile.c_str());
            printf("        %s was previously declared in %s\n",
                   lr.structname.fullname.c_str(), prior->zcmfile.c_str());
            return 1;
        } else {
            zcmgen.structs.push_back(std::move(lr));
            zcmgen.package = "";
        }
        return 0;
    }

    parse_error(t, "Missing struct token.");
    return 1;
}

int ZCMGen::handleFile(const string& path)
{
    tokenize_t* t = tokenize_create(path.c_str());

    if (t==NULL) {
        perror(path.c_str());
        return -1;
    }

    if (gopt->getBool("tokenize")) {
        int ntok = 0;
        printf("%6s %6s %6s: %s\n", "tok#", "line", "col", "token");

        while (tokenize_next(t) != EOF)
            printf("%6i %6i %6i: %s\n", ntok++, t->token_line, t->token_column, t->token);
        return 0;
    }

    int res;
    do {
        res = parseEntity(*this, path, t);
    } while (res == 0);

    tokenize_destroy(t);
    if (res == 0 || res == EOF)
        return 0;
    else
        return res;
}

void ZCMTypename::dump()
{
    printf("\t%-20s", fullname.c_str());
}

void ZCMMember::dump()
{
    type.dump();

    printf("  ");

    printf("%s", membername.c_str());

    for (auto& dim : dimensions) {
        switch (dim.mode) {
            case ZCM_CONST:
                printf(" [ (const) %s ]", dim.size.c_str());
                break;
            case ZCM_VAR:
                printf(" [ (var) %s ]", dim.size.c_str());
                break;
            default:
                // oops! unhandled case
                assert(0);
        }
    }

    printf("\n");
}

void ZCMStruct::dump()
{
    printf("struct %s [hash=0x%16" PRId64 "]\n", structname.fullname.c_str(), hash);
    for (auto& lm : members)
        lm.dump();
}

void ZCMGen::dump()
{
    for (auto& lr : structs)
        lr.dump();
}

/** Find and return the member whose name is name. **/
ZCMMember* ZCMStruct::findMember(const string& name)
{
    for (auto& lm : members)
        if (lm.membername == name)
            return &lm;

    return nullptr;
}

ZCMConstant* ZCMStruct::findConst(const string& name)
{
    for (auto& lc : constants)
        if (lc.membername == name)
            return &lc;

    return nullptr;
}

bool ZCMGen::needsGeneration(const string& declaringfile, const string& outfile)
{
    struct stat instat, outstat;
    int res;

    if (!gopt->getBool("lazy"))
        return true;

    res = stat(declaringfile.c_str(), &instat);
    if (res) {
        printf("Funny error: can't stat the .zcm file");
        perror(declaringfile.c_str());
        return true;
    }

    res = stat(outfile.c_str(), &outstat);
    if (res)
        return true;

    return instat.st_mtime > outstat.st_mtime;
}

/** Is the member an array of constant size? If it is not an array, it returns zero. **/
bool ZCMMember::isConstantSizeArray()
{
    if (dimensions.size() == 0)
        return true;

    for (auto& dim : dimensions)
        if (dim.mode == ZCM_VAR)
            return false;

    return true;
}
