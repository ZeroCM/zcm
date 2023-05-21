#pragma once

#include <string>
#include <functional>

#include <zcm/zcm_coretypes.h>

#include "TypeDb.hpp"

namespace zcm {
namespace Introspection {

typedef std::function<void(const std::string& name,
                           zcm_field_type_t type,
                           const void* data)> ProcessFn;

bool processEncodedType(const std::string& name,
                        const uint8_t *buf, size_t sz,
                        const std::string& separator,
                        const TypeDb& typeDb, ProcessFn cb);

bool processType(const std::string& name,
                 const zcm_type_info_t& info,
                 const void *data, const std::string& separator,
                 const TypeDb& typeDb, ProcessFn cb);

bool processArray(std::string name,
                  zcm_field_type_t type, const char* typestr,
                  size_t num_dims, int32_t *dim_size, int8_t *dim_is_variable,
                  const void* data, const std::string& separator,
                  const TypeDb& typeDb, ProcessFn cb);

bool processScalar(const std::string& name,
                   zcm_field_type_t type, const char* typestr,
                   const void* data, const std::string& separator,
                   const TypeDb& typeDb, ProcessFn cb);

} // Introspection
} // zcm
