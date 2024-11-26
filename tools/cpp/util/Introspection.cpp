#include <iostream>
#include <cassert>

#include "Introspection.hpp"

#include "util/StringUtil.hpp"

using namespace std;

namespace zcm {
namespace Introspection {

bool processEncodedType(const string& name,
                        const uint8_t *buf, size_t sz,
                        const string& separator,
                        const TypeDb& typeDb,
                        ProcessFn cb, void* usr)
{
    int64_t hash = 0;
    if (__int64_t_decode_array(buf, 0, sz, &hash, 1) < 0) return false;

    auto *zcmtype = typeDb.getByHash(hash);
    if (!zcmtype) return false;

    // get actual data points
    uint8_t* msg = new uint8_t[(size_t)zcmtype->info->struct_size()];
    if (zcmtype->info->decode(buf, 0, sz, msg) < 0) {
        delete[] msg;
        cerr << "Cannot decode zcmtype for: " << name << endl;
        return false;
    }

    bool ret = processType(name, *zcmtype->info, msg, separator, typeDb, cb, usr);
    if (!ret) cerr << "Cannot process type for: " << name << endl;

    delete[] msg;
    return ret;
}

bool processType(const string& name,
                 const zcm_type_info_t& info,
                 const void *data, const string& separator,
                 const TypeDb& typeDb,
                 ProcessFn cb, void* usr)
{
    bool ret = true;

    for (size_t i = 0; i < info.num_fields(); ++i) {
        zcm_field_t f;
        if (info.get_field(data, i, &f) < 0) {
            cerr << "Cannot get field: " << name << "[" << i << "]" << endl;
            ret = false;
            continue;
        }

        string nextName = name + separator + f.name;

        if (f.num_dim == 0) {
            if (!processScalar(nextName, f.type, f.typestr, f.data, separator, typeDb, cb, usr)) {
                cerr << "Cannot process scalar: " << nextName << endl;
                ret = false;
            }
        } else {
            const void *p = !f.dim_is_variable[0] ? f.data : *(const void **) f.data;
            if (!processArray(nextName, f.type, f.typestr,
                              f.num_dim, f.dim_size, f.dim_is_variable,
                              p, separator, typeDb, cb, usr)) {
                cerr << "Cannot process type's array: " << nextName << endl;
                ret = false;
            }
        }
    }

    return ret;
}

bool processArray(string name,
                  zcm_field_type_t type, const char* typestr,
                  size_t num_dims, int32_t *dim_size, int8_t *dim_is_variable,
                  const void* data, const string& separator,
                  const TypeDb& typeDb,
                  ProcessFn cb, void* usr)
{
    assert(num_dims > 0 && "Cant process an array of 0 dimensions");

    if (type == ZCM_FIELD_BYTE && num_dims == 1) {
        if (processEncodedType(name, (uint8_t*)data, dim_size[0], separator, typeDb, cb, usr))
            return true;
    }

    name += separator;

    bool ret = true;

    for (ssize_t i = 0; i < dim_size[0]; ++i) {
        string nextName = name + to_string(i);

        if (num_dims - 1 > 0) {
            const void *p = !dim_is_variable[0] ? data : *(const void **) data;
            if (!processArray(nextName, type, typestr,
                              num_dims - 1, dim_size + 1, dim_is_variable + 1,
                              p, separator, typeDb, cb, usr)) {
                cerr << "Cannot process array's array: " << nextName << endl;
                ret = false;
            }
            continue;
        }

        switch (type) {
            case ZCM_FIELD_INT8_T:
                if (!processScalar(nextName, type, typestr, &((int8_t*)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process int8_t array element: " << nextName << endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_INT16_T:
                if (!processScalar(nextName, type, typestr, &((int16_t*)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process int16_t array element: " << nextName << endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_INT32_T:
                if (!processScalar(nextName, type, typestr, &((int32_t*)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process int32_t array element: " << nextName << endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_INT64_T:
                if (!processScalar(nextName, type, typestr, &((int64_t*)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process int64_t array element: " << nextName << endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_BYTE:
                // Not introspecting into raw byte fields if it doesn't encode a zcmtype
                /*
                if (!processScalar(nextName, type, typestr, &((uint8_t*)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process byte array element: " << nextName << endl;
                    ret = false;
                }
                */
                break;
            case ZCM_FIELD_FLOAT:
                if (!processScalar(nextName, type, typestr, &((float*)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process float array element: " << nextName << endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_DOUBLE:
                if (!processScalar(nextName, type, typestr, &((double*)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process double array element: " << nextName << endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_BOOLEAN:
                if (!processScalar(nextName, type, typestr, &((bool*)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process boolean array element: " << nextName << endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_STRING:
                if (!processScalar(nextName, type, typestr, &((const char**)data)[i], separator, typeDb, cb, usr)) {
                    cerr << "Cannot process string array element: " << nextName << endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_USER_TYPE: {
                auto *zcmtype = typeDb.getByName(StringUtil::dotsToUnderscores(typestr));
                if (!zcmtype) {
                    cerr << "Cannot find array's zcmtype for " << name
                              << ": " << typestr << endl;
                    ret = false;
                    continue;
                }
                if (!processType(nextName, *zcmtype->info,
                                 ((uint8_t*)data) + i * zcmtype->info->struct_size(),
                                 separator, typeDb, cb, usr)) {
                    cerr << "Cannot process " << typestr
                              << " array element: " << nextName << endl;
                    ret = false;
                }
                break;
            }
        }
    }

    return ret;
}

bool processScalar(const string& name,
                   zcm_field_type_t type, const char* typestr,
                   const void* data, const string& separator,
                   const TypeDb& typeDb,
                   ProcessFn cb, void* usr)
{
    switch (type) {
        case ZCM_FIELD_INT8_T:
        case ZCM_FIELD_INT16_T:
        case ZCM_FIELD_INT32_T:
        case ZCM_FIELD_INT64_T:
        case ZCM_FIELD_BYTE:
        case ZCM_FIELD_FLOAT:
        case ZCM_FIELD_DOUBLE:
        case ZCM_FIELD_BOOLEAN:
        case ZCM_FIELD_STRING:
            cb(name, type, data, usr);
            break;
        case ZCM_FIELD_USER_TYPE: {
            auto *zcmtype = typeDb.getByName(StringUtil::dotsToUnderscores(typestr));
            if (!zcmtype) {
                cerr << "Cannot find scalar's zcmtype for " << name
                          << ": " << typestr << endl;
                return false;
            }
            return processType(name, *zcmtype->info, data, separator, typeDb, cb, usr);
        }
    }
    return true;
}

} // Introspection
} // zcm
