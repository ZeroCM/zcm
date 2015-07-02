#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef WIN32
#define __STDC_FORMAT_MACROS			// Enable integer types
#endif
#include <inttypes.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#else					// These should be coming from stdint.h - but not working?
#include <zcm/windows/WinPorting.h>
#define INT8_MIN     ((int8_t)_I8_MIN)
#define INT8_MAX     _I8_MAX
#define INT16_MIN    ((int16_t)_I16_MIN)
#define INT16_MAX    _I16_MAX
#define INT32_MIN    ((int32_t)_I32_MIN)
#define INT32_MAX    _I32_MAX
#define INT64_MIN    ((int64_t)_I64_MIN)
#define INT64_MAX    _I64_MAX
#define UINT8_MAX    _UI8_MAX
#define UINT16_MAX   _UI16_MAX
#define UINT32_MAX   _UI32_MAX
#endif

#include "zcmgen.h"
#include "tokenize.h"


#ifdef WIN32
#define	strtoll	_strtoui64
#endif

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

zcm_struct_t *parse_struct(zcmgen_t *zcm, const char *zcmfile, tokenize_t *t);
zcm_enum_t *parse_enum(zcmgen_t *zcm, const char *zcmfile, tokenize_t *t);

// zcm's built-in types. Note that unsigned types are not present
// because there is no safe java implementation. Really, you don't
// want to add unsigned types.
static const char *primitive_types[] = { "int8_t",
                                         "int16_t",
                                         "int32_t",
                                         "int64_t",
                                         "byte",
                                         "float",
                                         "double",
                                         "string",
                                         "boolean",
                                         NULL};

// which types can be legally used as array dimensions?
static const char *array_dimension_types[] = { "int8_t",
                                               "int16_t",
                                               "int32_t",
                                               "int64_t",
                                               NULL};

// which types can be legally used as const values?
static const char *const_types[] = { "int8_t",
                                     "int16_t",
                                     "int32_t",
                                     "int64_t",
                                     "float",
                                     "double",
                                     NULL };

// Given NULL-terminated array of strings "ts", does "t" appear in it?
static int string_in_array(const char *t, const char **ts)
{
    for (int i = 0; ts[i] != NULL; i++) {
        if (!strcmp(ts[i], t))
            return 1;
    }

    return 0;
}

int zcm_is_primitive_type(const char *t)
{
    return string_in_array(t, primitive_types);
}

int zcm_is_array_dimension_type(const char *t)
{
    return string_in_array(t, array_dimension_types);
}

int zcm_is_legal_member_name(const char *t)
{
    return isalpha(t[0]) || t[0]=='_';
}

int zcm_is_legal_const_type(const char *t)
{
    return string_in_array(t, const_types);
}

// Make the hash dependent on the value of the given character. The
// order that hash_update is called in IS important.
static int64_t hash_update(int64_t v, char c)
{
    v = ((v<<8) ^ (v>>55)) + c;

    return v;
}

// Make the hash dependent on each character in a string.
static int64_t hash_string_update(int64_t v, const char *s)
{
    v = hash_update(v, strlen(s));

    for (; *s != 0; s++)
        v = hash_update(v, *s);

    return v;
}

// Create a parsing context
zcmgen_t *zcmgen_create()
{
    zcmgen_t *zcmgen = (zcmgen_t*) calloc(1, sizeof(zcmgen_t));
    zcmgen->structs = g_ptr_array_new();
    zcmgen->enums = g_ptr_array_new();
    zcmgen->package = strdup("");
    zcmgen->comment_doc = NULL;

    return zcmgen;
}

// Parse a type into package and class name.  If no package is
// specified, we will try to use the package from the last specified
// "package" directive, like in Java.
zcm_typename_t *zcm_typename_create(zcmgen_t *zcmgen, const char *lctypename)
{
    zcm_typename_t *lt = (zcm_typename_t*) calloc(1, sizeof(zcm_typename_t));

    lt->lctypename = strdup(lctypename);

    // package name: everything before the last ".", or "" if there is no "."
    //
    // shortname: everything after the last ".", or everything if
    // there is no "."
    //
    char *tmp = strdup(lctypename);
    char *rtmp = strrchr(tmp, '.');
    if (rtmp == NULL) {
        lt->shortname = tmp;
        if (zcm_is_primitive_type(lt->shortname)) {
            lt->package = strdup("");
        } else {
            // we're overriding the package name using the last directive.
            lt->package = strdup(zcmgen->package);
            lt->lctypename = g_strdup_printf("%s%s%s", lt->package,
                                          strlen(zcmgen->package)>0 ? "." : "",
                                          lt->shortname);
        }
    } else {
        lt->package = tmp;
        *rtmp = 0;
        lt->shortname = &rtmp[1];
    }

    const char* package_prefix = getopt_get_string(zcmgen->gopt, "package-prefix");
    if (strlen(package_prefix)>0 && !zcm_is_primitive_type(lt->shortname)){
        lt->package = g_strdup_printf("%s%s%s",
                                      package_prefix,
                                      strlen(lt->package) > 0 ? "." : "",
                                      lt->package);
        lt->lctypename = g_strdup_printf("%s.%s", lt->package, lt->shortname);
    }

    return lt;
}

zcm_struct_t *zcm_struct_create(zcmgen_t *zcmgen, const char *zcmfile, const char *structname)
{
    zcm_struct_t *lr = (zcm_struct_t*) calloc(1, sizeof(zcm_struct_t));
    lr->zcmfile    = strdup(zcmfile);
    lr->structname = zcm_typename_create(zcmgen, structname);
    lr->members    = g_ptr_array_new();
    lr->constants  = g_ptr_array_new();
    lr->enums      = g_ptr_array_new();
    lr->structs    = g_ptr_array_new();
    return lr;
}

zcm_constant_t *zcm_constant_create(const char *type, const char *name, const char *val_str)
{
    zcm_constant_t *lc = (zcm_constant_t*) calloc(1, sizeof(zcm_constant_t));
    lc->lctypename = strdup(type);
    lc->membername = strdup(name);
    lc->val_str = strdup(val_str);
    // don't fill in the value
    return lc;
}

void zcm_constant_destroy(zcm_constant_t *lc)
{
    free(lc->lctypename);
    free(lc->membername);
    free(lc);
}

zcm_enum_t *zcm_enum_create(zcmgen_t *zcmgen, const char *zcmfile, const char *name)
{
    zcm_enum_t *le = (zcm_enum_t*) calloc(1, sizeof(zcm_enum_t));
    le->zcmfile  = strdup(zcmfile);
    le->enumname = zcm_typename_create(zcmgen, name);
    le->values   = g_ptr_array_new();

    return le;
}

zcm_enum_value_t *zcm_enum_value_create(const char *name)
{
    zcm_enum_value_t *lev = (zcm_enum_value_t*) calloc(1, sizeof(zcm_enum_t));

    lev->valuename = strdup(name);

    return lev;
}

zcm_member_t *zcm_member_create()
{
    zcm_member_t *lm = (zcm_member_t*) calloc(1, sizeof(zcm_member_t));
    lm->dimensions = g_ptr_array_new();
    return lm;
}

//zcm_constant_t *zcm_constant_create(zcmgen_t *zcmgen, const char *zcmfile

int64_t zcm_struct_hash(zcm_struct_t *lr)
{
    int64_t v = 0x12345678;

    // NO: Purposefully, we do NOT include the structname in the hash.
    // this allows people to rename data types and still have them work.
    //
    // In contrast, we DO hash the types of a structs members (and their names).
    //  v = hash_string_update(v, lr->structname);

    for (unsigned int i = 0; i < g_ptr_array_size(lr->members); i++) {
        zcm_member_t *lm = (zcm_member_t *) g_ptr_array_index(lr->members, i);

        // hash the member name
        v = hash_string_update(v, lm->membername);

        // if the member is a primitive type, include the type
        // signature in the hash. Do not include them for compound
        // members, because their contents will be included, and we
        // don't want a struct's name change to break the hash.
        if (zcm_is_primitive_type(lm->type->lctypename))
            v = hash_string_update(v, lm->type->lctypename);

        // hash the dimensionality information
        int ndim = g_ptr_array_size(lm->dimensions);
        v = hash_update(v, ndim);
        for (int j = 0; j < ndim; j++) {
            zcm_dimension_t *dim = (zcm_dimension_t*) g_ptr_array_index(lm->dimensions, j);
            v = hash_update(v, dim->mode);
            v = hash_string_update(v, dim->size);
        }
    }

    return v;
}

// The hash for ZCM enums is defined only by the name of the enum;
// this allows bit declarations to be added over time.
int64_t zcm_enum_hash(zcm_enum_t *le)
{
    int64_t v = 0x87654321;

    v = hash_string_update(v, le->enumname->shortname);
    return v;
}

// semantic error: it parsed fine, but it's illegal. (we don't try to
// identify the offending token). This function does not return.
void semantic_error(tokenize_t *t, const char *fmt, ...)
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
    _exit(1);
}

// semantic warning: it parsed fine, but it's dangerous.
void semantic_warning(tokenize_t *t, const char *fmt, ...)
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
void parse_error(tokenize_t *t, const char *fmt, ...)
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
    _exit(1);
}

// Consume any available comments and store them in zcmgen->comment_doc
void parse_try_consume_comment(zcmgen_t* zcmgen, tokenize_t* t,
    int store_comment_doc)
{
    if (store_comment_doc) {
        g_free(zcmgen->comment_doc);
        zcmgen->comment_doc = NULL;
    }

    while (tokenize_peek(t) != EOF && t->token_type == ZCM_TOK_COMMENT) {
        tokenize_next(t);

        if (store_comment_doc) {
            if (!zcmgen->comment_doc) {
                zcmgen->comment_doc = g_strdup(t->token);
            } else {
                gchar* orig = zcmgen->comment_doc;
                zcmgen->comment_doc = g_strdup_printf("%s\n%s", orig, t->token);
                g_free(orig);
            }
        }
    }
}

// If the next non-comment token is "tok", consume it and return 1. Else,
// return 0
int parse_try_consume(tokenize_t *t, const char *tok)
{
    parse_try_consume_comment(NULL, t, 0);
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
void parse_require(tokenize_t *t, char *tok)
{
    parse_try_consume_comment(NULL, t, 0);
    int res;
    do {
        res = tokenize_next(t);
    } while (t->token_type == ZCM_TOK_COMMENT);

    if (res == EOF || strcmp(t->token, tok))
        parse_error(t, "expected token %s", tok);

}

// require that the next token exist (not EOF). Description is a
// human-readable description of what was expected to be read.
void tokenize_next_or_fail(tokenize_t *t, const char *description)
{
    int res = tokenize_next(t);
    if (res == EOF)
        parse_error(t, "End of file reached, expected %s.", description);
}

int parse_const(zcmgen_t *zcmgen, zcm_struct_t *lr, tokenize_t *t)
{
    parse_try_consume_comment(zcmgen, t, 0);
    tokenize_next_or_fail(t, "type identifier");

    // get the constant type
    if (!zcm_is_legal_const_type(t->token))
        parse_error(t, "invalid type for const");
    char *lctypename = strdup(t->token);

another_constant:
    // get the member name
    parse_try_consume_comment(zcmgen, t, 0);
    tokenize_next_or_fail(t, "name identifier");
    if (!zcm_is_legal_member_name(t->token))
        parse_error(t, "Invalid member name: must start with [a-zA-Z_].");
    char *membername = strdup(t->token);

    // make sure this name isn't already taken.
    if (zcm_find_member(lr, t->token) != NULL)
        semantic_error(t, "Duplicate member name '%s'.", t->token);
    if (zcm_find_const(lr, t->token) != NULL)
        semantic_error(t, "Duplicate member name '%s'.", t->token);

    // get the value
    parse_require(t, "=");
    parse_try_consume_comment(zcmgen, t, 0);
    tokenize_next_or_fail(t, "constant value");

    // create a new const member
    zcm_constant_t *lc = zcm_constant_create(lctypename, membername, t->token);

    // Attach the last comment if one was defined.
    if (zcmgen->comment_doc) {
      lc->comment = zcmgen->comment_doc;
      zcmgen->comment_doc = NULL;
    }

    char *endptr = NULL;
    if (!strcmp(lctypename, "int8_t")) {
        long long v = strtoll(t->token, &endptr, 0);
        if (endptr == t->token || *endptr != '\0')
            parse_error(t, "Expected integer value");
        if (v < INT8_MIN || v > INT8_MAX)
            semantic_error(t, "Integer value out of bounds for int8_t");
        lc->val.i8 = (int8_t)v;
    } else if (!strcmp(lctypename, "int16_t")) {
        long long v = strtoll(t->token, &endptr, 0);
        if (endptr == t->token || *endptr != '\0')
            parse_error(t, "Expected integer value");
        if (v < INT16_MIN || v > INT16_MAX)
            semantic_error(t, "Integer value out of range for int16_t");
        lc->val.i16 = (int16_t)v;
    } else if (!strcmp(lctypename, "int32_t")) {
        long long v = strtoll(t->token, &endptr, 0);
        if (endptr == t->token || *endptr != '\0')
            parse_error(t, "Expected integer value");
        if (v < INT32_MIN || v > INT32_MAX)
            semantic_error(t, "Integer value out of range for int32_t");
        lc->val.i32 = (int32_t)v;
    } else if (!strcmp(lctypename, "int64_t")) {
        long long v = strtoll(t->token, &endptr, 0);
        if (endptr == t->token || *endptr != '\0')
            parse_error(t, "Expected integer value");
        lc->val.i64 = (int64_t)v;
    } else if (!strcmp(lctypename, "float")) {
        double v = strtod(t->token, &endptr);
        if (endptr == t->token || *endptr != '\0')
            parse_error(t, "Expected floating point value");
        if (v > FLT_MAX || v < -FLT_MAX)
            semantic_error(t, "Floating point value out of range for float");
        lc->val.f = (float)v;
    } else if (!strcmp(lctypename, "double")) {
        double v = strtod(t->token, &endptr);
        if (endptr == t->token || *endptr != '\0')
            parse_error(t, "Expected floating point value");
        lc->val.d = v;
    } else {
        fprintf(stderr, "[%s]\n", t->token);
        assert(0);
    }

    g_ptr_array_add(lr->constants, lc);

    free(membername);

    if (parse_try_consume(t, ",")) {
        goto another_constant;
    }

    free(lctypename);

    parse_require(t, ";");
    return 0;
}

// parse a member declaration. This looks long and scary, but most of
// the code is for semantic analysis (error checking)
int parse_member(zcmgen_t *zcmgen, zcm_struct_t *lr, tokenize_t *t)
{
    zcm_typename_t *lt = NULL;

    // Read a type specification. Then read members (multiple
    // members can be defined per-line.) Each member can have
    // different array dimensionalities.

    // inline type declaration?
    if (parse_try_consume(t, "struct")) {
        parse_error(t, "recursive structs not implemented.");
    } else if (parse_try_consume(t, "enum")) {
        parse_error(t, "recursive enums not implemented.");
    } else if (parse_try_consume(t, "const")) {
        return parse_const(zcmgen, lr, t);
    }

    // standard declaration
    parse_try_consume_comment(zcmgen, t, 0);
    tokenize_next_or_fail(t, "type identifier");

    if (!isalpha(t->token[0]) && t->token[0]!='_')
        parse_error(t, "invalid type name");

    // A common mistake is use 'int' as a type instead of 'intN_t'
    if(!strcmp(t->token, "int"))
        semantic_warning(t, "int type should probably be int8_t, int16_t, int32_t, or int64_t");

    lt = zcm_typename_create(zcmgen, t->token);

    while (1) {

        // get the zcm type name
        parse_try_consume_comment(zcmgen, t, 0);
        tokenize_next_or_fail(t, "name identifier");

        if (!zcm_is_legal_member_name(t->token))
            parse_error(t, "Invalid member name: must start with [a-zA-Z_].");

        // make sure this name isn't already taken.
        if (zcm_find_member(lr, t->token) != NULL)
            semantic_error(t, "Duplicate member name '%s'.", t->token);
        if (zcm_find_const(lr, t->token) != NULL)
            semantic_error(t, "Duplicate member name '%s'.", t->token);

        // create a new member
        zcm_member_t *lm = zcm_member_create();
        lm->type = lt;
        lm->membername = strdup(t->token);
        if (zcmgen->comment_doc) {
            lm->comment = zcmgen->comment_doc;
            zcmgen->comment_doc = NULL;
        }
        g_ptr_array_add(lr->members, lm);

        // (multi-dimensional) array declaration?
        while (parse_try_consume(t, "[")) {

            // pull out the size of the dimension, either a number or a variable name.
            parse_try_consume_comment(zcmgen, t, 0);
            tokenize_next_or_fail(t, "array size");

            zcm_dimension_t *dim = (zcm_dimension_t*) calloc(1, sizeof(zcm_dimension_t));

            if (isdigit(t->token[0])) {
                // we have a constant size array declaration.
                int sz = strtol(t->token, NULL, 0);
                if (sz <= 0)
                    semantic_error(t, "Constant array size must be > 0");

                dim->mode = ZCM_CONST;
                dim->size = strdup(t->token);

            } else {
                // we have a variable sized declaration.
                if (t->token[0]==']')
                    semantic_error(t, "Array sizes must be declared either as a constant or variable.");
                if (!zcm_is_legal_member_name(t->token))
                    semantic_error(t, "Invalid array size variable name: must start with [a-zA-Z_].");

                // make sure the named variable is
                // 1) previously declared and
                // 2) an integer type
                int okay = 0;

                for (unsigned int i = 0; i < g_ptr_array_size(lr->members); i++) {
                    zcm_member_t *thislm = (zcm_member_t*) g_ptr_array_index(lr->members, i);
                    if (!strcmp(thislm->membername, t->token)) {
                        if (g_ptr_array_size(thislm->dimensions) != 0)
                            semantic_error(t, "Array dimension '%s' must be not be an array type.", t->token);
                        if (!zcm_is_array_dimension_type(thislm->type->lctypename))
                            semantic_error(t, "Array dimension '%s' must be an integer type.", t->token);
                        okay = 1;
                        break;
                    }
                }
                if (!okay)
                    semantic_error(t, "Unknown variable array index '%s'. Index variables must be declared before the array.", t->token);

                dim->mode = ZCM_VAR;
                dim->size = strdup(t->token);
            }
            parse_require(t, "]");

            // increase the dimensionality of the array by one dimension.
            g_ptr_array_add(lm->dimensions, dim);
        }

        if (!parse_try_consume(t, ","))
            break;
    }

    parse_require(t, ";");

    return 0;
}

int parse_enum_value(zcm_enum_t *le, tokenize_t *t)
{
    tokenize_next_or_fail(t, "enum name");

    zcm_enum_value_t *lev = zcm_enum_value_create(t->token);

    if (parse_try_consume(t, "=")) {
        tokenize_next_or_fail(t, "enum value literal");

        lev->value = strtol(t->token, NULL, 0);
    } else {
        // the didn't specify the value, compute the next largest value
        int32_t max = 0;

        for (unsigned int i = 0; i < g_ptr_array_size(le->values); i++) {
            zcm_enum_value_t *tmp = (zcm_enum_value_t *) g_ptr_array_index(le->values, i);
            if (tmp->value > max)
                max = tmp->value;
        }

        lev->value = max + 1;
    }

    // make sure there aren't any duplicate values
    for (unsigned int i = 0; i < g_ptr_array_size(le->values); i++) {
        zcm_enum_value_t *tmp = (zcm_enum_value_t *) g_ptr_array_index(le->values, i);
        if (tmp->value == lev->value)
            semantic_error(t, "Enum values %s and %s have the same value %d!", tmp->valuename, lev->valuename, lev->value);
        if (!strcmp(tmp->valuename, lev->valuename))
            semantic_error(t, "Enum value %s declared twice!", tmp->valuename);
    }

    g_ptr_array_add(le->values, lev);
    return 0;
}

/** assume the "struct" token is already consumed **/
zcm_struct_t *parse_struct(zcmgen_t *zcmgen, const char *zcmfile, tokenize_t *t)
{
    char     *name;

    parse_try_consume_comment(zcmgen, t, 0);
    tokenize_next_or_fail(t, "struct name");
    name = strdup(t->token);

    zcm_struct_t *lr = zcm_struct_create(zcmgen, zcmfile, name);

    if (zcmgen->comment_doc) {
        lr->comment = zcmgen->comment_doc;
        zcmgen->comment_doc = NULL;
    }

    parse_require(t, "{");

    while (!parse_try_consume(t, "}")) {
        // Check for leading comments that will be used to document the member.
        parse_try_consume_comment(zcmgen, t, 1);

        if (parse_try_consume(t, "}")) {
            break;
        }
        parse_member(zcmgen, lr, t);
    }

    lr->hash = zcm_struct_hash(lr);

    free(name);
    return lr;
}

/** assumes the "enum" token is already consumed **/
zcm_enum_t *parse_enum(zcmgen_t *zcmgen, const char *zcmfile, tokenize_t *t)
{
    char     *name;

    tokenize_next_or_fail(t, "enum name");
    name = strdup(t->token);

    zcm_enum_t *le = zcm_enum_create(zcmgen, zcmfile, name);
    parse_require(t, "{");

    while (!parse_try_consume(t, "}")) {
        parse_enum_value(le, t);

        parse_try_consume(t, ",");
        parse_try_consume(t, ";");
    }

    le->hash = zcm_enum_hash(le);
    free(name);
    return le;
}

const zcm_struct_t* find_struct(zcmgen_t* zcmgen, const char* package,
    const char* name) {
    for (int i=0; i < zcmgen->structs->len; i++) {
        zcm_struct_t* lr =
            (zcm_struct_t*) g_ptr_array_index(zcmgen->structs, i);
        if (!strcmp(package, lr->structname->package) &&
            !strcmp(name, lr->structname->shortname))
            return lr;
    }
    return NULL;
}

/** parse entity (top-level construct), return EOF if eof. **/
int parse_entity(zcmgen_t *zcmgen, const char *zcmfile, tokenize_t *t)
{
    int res;

    parse_try_consume_comment(zcmgen, t, 1);

    res = tokenize_next(t);
    if (res==EOF)
        return EOF;

    if (!strcmp(t->token, "package")) {
        parse_try_consume_comment(zcmgen, t, 0);
        tokenize_next_or_fail(t, "package name");
        zcmgen->package = strdup(t->token);
        parse_require(t, ";");
        return 0;
    }

    if (!strcmp(t->token, "struct")) {
        zcm_struct_t *lr = parse_struct(zcmgen, zcmfile, t);

        // check for duplicate types
        const zcm_struct_t* prior = find_struct(zcmgen,
                lr->structname->package, lr->structname->shortname);
        if (prior) {
            printf("ERROR:  duplicate type %s declared in %s\n",
                    lr->structname->lctypename, zcmfile);
            printf("        %s was previously declared in %s\n",
                    lr->structname->lctypename, prior->zcmfile);
            // TODO destroy lr.
            return 1;
        } else {
            g_ptr_array_add(zcmgen->structs, lr);
        }
        return 0;
    }

    if (!strcmp(t->token, "enum")) {
        zcm_enum_t *le = parse_enum(zcmgen, zcmfile, t);
        g_ptr_array_add(zcmgen->enums, le);
        return 0;
    }

    parse_error(t,"Missing struct token.");
    return 1;
}

int zcmgen_handle_file(zcmgen_t *zcmgen, const char *path)
{
    tokenize_t *t = tokenize_create(path);

    if (t==NULL) {
        perror(path);
        return -1;
    }

    if (getopt_get_bool(zcmgen->gopt, "tokenize")) {
        int ntok = 0;
        printf("%6s %6s %6s: %s\n", "tok#", "line", "col", "token");

        while (tokenize_next(t)!=EOF)
            printf("%6i %6i %6i: %s\n", ntok++, t->token_line, t->token_column, t->token);
        return 0;
    }

    int res;
    do {
        res = parse_entity(zcmgen, path, t);
    } while (res == 0);

    tokenize_destroy(t);
    if (res == 0 || res == EOF)
        return 0;
    else
        return res;
}

void zcm_typename_dump(zcm_typename_t *lt)
{
    char buf[1024];
    int pos = 0;

    pos += sprintf(&buf[pos], "%s", lt->lctypename);

    printf("\t%-20s", buf);
}

void zcm_member_dump(zcm_member_t *lm)
{
    zcm_typename_dump(lm->type);

    printf("  ");

    printf("%s", lm->membername);

    int ndim = g_ptr_array_size(lm->dimensions);
    for (int i = 0; i < ndim; i++) {
        zcm_dimension_t *dim = (zcm_dimension_t *) g_ptr_array_index(lm->dimensions, i);
        switch (dim->mode)
        {
        case ZCM_CONST:
            printf(" [ (const) %s ]", dim->size);
            break;
        case ZCM_VAR:
            printf(" [ (var) %s ]", dim->size);
            break;
        default:
            // oops! unhandled case
            assert(0);
        }
    }

    printf("\n");
}

void zcm_enum_dump(zcm_enum_t *le)
{
    printf("enum %s\n", le->enumname->lctypename);
    for (unsigned int i = 0; i < g_ptr_array_size(le->values); i++) {
        zcm_enum_value_t *lev = (zcm_enum_value_t *) g_ptr_array_index(le->values, i);
        printf("        %-20s  %i\n", lev->valuename, lev->value);
    }
}

void zcm_struct_dump(zcm_struct_t *lr)
{
    printf("struct %s [hash=0x%16"PRId64"]\n", lr->structname->lctypename, lr->hash);
    for (unsigned int i = 0; i < g_ptr_array_size(lr->members); i++) {
        zcm_member_t *lm = (zcm_member_t *) g_ptr_array_index(lr->members, i);
        zcm_member_dump(lm);
    }

    for (unsigned int i = 0; i < g_ptr_array_size(lr->enums); i++) {
        zcm_enum_t *le = (zcm_enum_t *) g_ptr_array_index(lr->enums, i);
        zcm_enum_dump(le);
    }
}

void zcmgen_dump(zcmgen_t *zcmgen)
{
    for (unsigned int i = 0; i < g_ptr_array_size(zcmgen->enums); i++) {
        zcm_enum_t *le = (zcm_enum_t *) g_ptr_array_index(zcmgen->enums, i);
        zcm_enum_dump(le);
    }

    for (unsigned int i = 0; i < g_ptr_array_size(zcmgen->structs); i++) {
        zcm_struct_t *lr = (zcm_struct_t *) g_ptr_array_index(zcmgen->structs, i);
        zcm_struct_dump(lr);
    }
}

/** Find and return the member whose name is name. **/
zcm_member_t *zcm_find_member(zcm_struct_t *lr, const char *name)
{
    for (unsigned int i = 0; i < g_ptr_array_size(lr->members); i++) {
        zcm_member_t *lm = (zcm_member_t*) g_ptr_array_index(lr->members, i);
        if (!strcmp(lm->membername, name))
            return lm;
    }

    return NULL;
}

/** Find and return the const whose name is name. **/
zcm_constant_t *zcm_find_const(zcm_struct_t *lr, const char *name)
{
    for (unsigned int i = 0; i < g_ptr_array_size(lr->constants); i++) {
        zcm_constant_t *lc = (zcm_constant_t*) g_ptr_array_index(lr->constants, i);
        if (!strcmp(lc->membername, name))
            return lc;
    }
    return NULL;
}

int zcm_needs_generation(zcmgen_t *zcmgen, const char *declaringfile, const char *outfile)
{
    struct stat instat, outstat;
    int res;

    if (!getopt_get_bool(zcmgen->gopt, "lazy"))
        return 1;

    res = stat(declaringfile, &instat);
    if (res) {
        printf("Funny error: can't stat the .zcm file");
        perror(declaringfile);
        return 1;
    }

    res = stat(outfile, &outstat);
    if (res)
        return 1;

    return instat.st_mtime > outstat.st_mtime;
}

/** Is the member an array of constant size? If it is not an array, it returns zero. **/
int zcm_is_constant_size_array(zcm_member_t *lm)
{
    int ndim = g_ptr_array_size(lm->dimensions);

    if (ndim == 0)
        return 1;

    for (int i = 0; i < ndim; i++) {
        zcm_dimension_t *dim = (zcm_dimension_t *) g_ptr_array_index(lm->dimensions, i);

        if (dim->mode == ZCM_VAR)
            return 0;
    }

    return 1;
}
