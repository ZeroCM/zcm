#pragma once

#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <functional>

#include "zcm/zcm_coretypes.h"

static const std::string separator = ".";

typedef std::function<void(const string& name,
                           zcm_field_type_t type,
                           const void* data)> ProcessFn;

static bool processType(const string& name, const zcm_type_info_t& info,
                        const void *data, const TypeDb& typeDb, ProcessFn cb);
static bool processEncodedType(const string& name,
                               const uint8_t *buf, size_t sz,
                               const TypeDb& typeDb, ProcessFn cb);

static bool processScalar(const string& name, zcm_field_type_t type, const char* typestr,
                          const void* data, const TypeDb& typeDb, ProcessFn cb)
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
            cb(name, type, data);
            break;
        case ZCM_FIELD_USER_TYPE: {
            auto *zcmtype = typeDb.getByName(StringUtil::dotsToUnderscores(typestr));
            if (!zcmtype) {
                std::cerr << "Cannot find scalar's zcmtype for " << name
                          << ": " << typestr << std::endl;
                return false;
            }
            return processType(name, *zcmtype->info, data, typeDb, cb);
        }
    }
    return true;
}

static bool processArray(string name, zcm_field_type_t type, const char* typestr,
                         size_t num_dims, int32_t *dim_size, int8_t *dim_is_variable,
                         const void* data, const TypeDb& typeDb, ProcessFn cb)
{
    assert(num_dims > 0 && "Cant process an array of 0 dimensions");

    if (type == ZCM_FIELD_BYTE && num_dims == 1) {
        if (processEncodedType(name, (uint8_t*)data, dim_size[0], typeDb, cb))
            return true;
    }

    name += separator;

    bool ret = true;

    for (ssize_t i = 0; i < dim_size[0]; ++i) {
        string nextName = name + std::to_string(i);

        if (num_dims - 1 > 0) {
            const void *p = !dim_is_variable[0] ? data : *(const void **) data;
            if (!processArray(nextName, type, typestr,
                              num_dims - 1, dim_size + 1, dim_is_variable + 1,
                              p, typeDb, cb)) {
                std::cerr << "Cannot process array's array: " << nextName << std::endl;
                ret = false;
            }
            continue;
        }

        switch (type) {
            case ZCM_FIELD_INT8_T:
                if (!processScalar(nextName, type, typestr, &((int8_t*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process int8_t array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_INT16_T:
                if (!processScalar(nextName, type, typestr, &((int16_t*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process int16_t array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_INT32_T:
                if (!processScalar(nextName, type, typestr, &((int32_t*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process int32_t array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_INT64_T:
                if (!processScalar(nextName, type, typestr, &((int64_t*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process int64_t array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_BYTE:
                // Not introspecting into raw byte fields if it doesn't encode a zcmtype
                /*
                if (!processScalar(nextName, type, typestr, &((uint8_t*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process byte array element: " << nextName << std::endl;
                    ret = false;
                }
                */
                break;
            case ZCM_FIELD_FLOAT:
                if (!processScalar(nextName, type, typestr, &((float*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process float array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_DOUBLE:
                if (!processScalar(nextName, type, typestr, &((double*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process double array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_BOOLEAN:
                if (!processScalar(nextName, type, typestr, &((bool*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process boolean array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_STRING:
                if (!processScalar(nextName, type, typestr, &((const char*)data)[i], typeDb, cb)) {
                    std::cerr << "Cannot process string array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            case ZCM_FIELD_USER_TYPE: {
                auto *zcmtype = typeDb.getByName(StringUtil::dotsToUnderscores(typestr));
                if (!zcmtype) {
                    std::cerr << "Cannot find array's zcmtype for " << name
                              << ": " << typestr << std::endl;
                    ret = false;
                    continue;
                }
                if (!processType(nextName, *zcmtype->info,
                                 ((uint8_t*)data) + i * zcmtype->info->struct_size(),
                                 typeDb, cb)) {
                    std::cerr << "Cannot process " << typestr
                              << " array element: " << nextName << std::endl;
                    ret = false;
                }
                break;
            }
        }
    }

    return ret;
}

static bool processType(const string& name, const zcm_type_info_t& info,
                        const void *data, const TypeDb& typeDb, ProcessFn cb)
{
    bool ret = true;

    for (size_t i = 0; i < info.num_fields(); ++i) {
        zcm_field_t f;
        if (info.get_field(data, i, &f) < 0) {
            std::cerr << "Cannot get field: " << name << "[" << i << "]" << std::endl;
            ret = false;
            continue;
        }

        string nextName = name + separator + f.name;

        if (f.num_dim == 0) {
            if (!processScalar(nextName, f.type, f.typestr, f.data, typeDb, cb)) {
                std::cerr << "Cannot process scalar: " << nextName << std::endl;
                ret = false;
            }
        } else {
            const void *p = !f.dim_is_variable[0] ? f.data : *(const void **) f.data;
            if (!processArray(nextName, f.type, f.typestr,
                              f.num_dim, f.dim_size, f.dim_is_variable,
                              p, typeDb, cb)) {
                std::cerr << "Cannot process type's array: " << nextName << std::endl;
                ret = false;
            }
        }
    }

    return ret;
}

static bool processEncodedType(const string& name,
                               const uint8_t *buf, size_t sz,
                               const TypeDb& typeDb, ProcessFn cb)
{
    int64_t hash = 0;
    if (__int64_t_decode_array(buf, 0, sz, &hash, 1) < 0) return false;

    auto *zcmtype = typeDb.getByHash(hash);
    if (!zcmtype) return false;

    // get actual data points
    uint8_t* msg = new uint8_t[(size_t)zcmtype->info->struct_size()];
    if (zcmtype->info->decode(buf, 0, sz, msg) < 0) {
        delete[] msg;
        std::cerr << "Cannot decode zcmtype for: " << name << std::endl;
        return false;
    }

    bool ret = processType(name, *zcmtype->info, msg, typeDb, cb);
    if (!ret) std::cerr << "Cannot process type for: " << name << std::endl;

    delete[] msg;
    return ret;
}
