#include "Python.h"

void PyEval_InitThreads_CUSTOM()
{
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 9
#else
    PyEval_InitThreads();
#endif
}
