#include "zcm/util/debug.hpp"

#ifdef ZCM_DEBUG_ENV_ENABLE
bool ZCM_DEBUG_ENABLED;

// This code runs before the main() function to load check if
// the ZCM_DEBUG env-var is set
struct BeforeMainCode
{
    BeforeMainCode()
    { ZCM_DEBUG_ENABLED = (NULL != getenv("ZCM_DEBUG")); }
};
static BeforeMainCode run;

#endif  // ZCM_DEBUG_ENV_ENABLE
