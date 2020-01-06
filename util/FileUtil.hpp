#pragma once
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "StringUtil.hpp"

namespace FileUtil
{
    static inline bool exists(const string& path)
    {
        struct stat st;
        return 0 == stat(path.c_str(), &st);
    }

    static inline bool dirExists(const string& path)
    {
        struct stat st;
        int ret = stat(path.c_str(), &st);
        if (ret != 0)
            return false;
        return S_ISDIR(st.st_mode);
    }

    static inline int remove(const string& path)
    {
        return unlink(path.c_str());
    }

    static inline int rename(const string& old, const string& new_)
    {
        return ::rename(old.c_str(), new_.c_str());
    }

    static inline int mkdirWithParents(const string& path, mode_t mode)
    {
        auto parts = StringUtil::split(path, '/');
        string dirPath = "";
        for (size_t i = 0; i < parts.size(); ++i) {
            const auto& p = parts[i];
            if (p == "") {
                dirPath += "/";
                continue;
            }
            dirPath += p;
            int err = mkdir(dirPath.c_str(), mode);
            if (err < 0 && errno != EEXIST) {
                return err;
            }
            dirPath += "/";
        }
        return 0;
    }

    static inline string dirname(const string& path)
    {
        // TODO: Do this without a malloc
        char *pathCopy = strdup(path.c_str());
        string ret = ::dirname(pathCopy);
        free(pathCopy);
        return ret;
    }

    static int makeDirsForFile(const string& path)
    {
        return mkdirWithParents(dirname(path), 0755);
    }
}
