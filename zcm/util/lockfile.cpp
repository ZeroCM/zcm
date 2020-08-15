#include "zcm/util/lockfile.h"
#include "zcm/util/debug.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <cstring>
#include <string>
using namespace std;

#define DEFAULT_LOCK_DIR "/var/lock/zcm"
#define LOCK_PREFIX "LCK.."

#include <unordered_map>
static unordered_map<string, int> fds;

static bool startsWith(const string& s, const string& pre)
{
    if (s.size() < pre.size())
        return false;
    for (size_t i = 0; i < pre.size(); i++)
        if (s[i] != pre[i])
            return false;
    return true;
}

static string makeLockfilePath(const string& name)
{
    string ret = DEFAULT_LOCK_DIR;

    const char* dir = std::getenv("ZCM_LOCK_DIR");
    if (dir) ret = dir;

    int err = mkdir(ret.c_str(), S_IRWXO | S_IRWXG | S_IRWXU);
    if (err < 0 && errno != EEXIST) {
        ZCM_DEBUG("Unable to create lockfile directory");
        return "";
    }

    if (ret == "") return ret;

    if (ret[ret.size() - 1] != '/') ret += "/";

    ret += LOCK_PREFIX;

    size_t idx = 0;
    if (startsWith(name, "/dev/"))
        idx = 5;
    for (; idx < name.size(); idx++) {
        auto c = name[idx];
        if (c == '/')
            ret += string(1, '_');
        else
            ret += string(1, c);
    }
    return ret;
}

static int openLockfile(const string& name)
{
    if (!fds.count(name)) {
        auto path = makeLockfilePath(name);
        int fd = open(path.c_str(), O_WRONLY | O_CREAT, 0666);
        if (fd < 0) return -1;
        fds[name] = fd;
    }
    return fds[name];
}

static bool isLocked(const string& name)
{
    int fd = openLockfile(name);
    if (fd < 0) return true;

    int ret = lockf(fd, F_TEST, 0);

    return ret == 0;
}


bool lockfile_trylock(const char *name)
{
    int fd = openLockfile(name);
    if (fd < 0) {
        ZCM_DEBUG("Unable to open lockfile: '%s'", name);
        return false;
    }

    int ret = lockf(fd, F_TLOCK, 0);
    if (ret < 0) {
        ZCM_DEBUG("Failed to lock lockfile '%s'. Error: %s", name, strerror(errno));
        return false;
    }

    return true;
}

void lockfile_unlock(const char *name)
{
    int fd = openLockfile(name);
    if (fd < 0) return;

    int ret = lockf(fd, F_ULOCK, 0);
    if (ret < 0)
        ZCM_DEBUG("Failed to unlock lockfile '%s'. Error: %s", name, strerror(errno));

    close(fd);
    fds.erase(name);

    int err = unlink(makeLockfilePath(name).c_str());
    if (err < 0)
        ZCM_DEBUG("Failed to unlink lockfile '%s'. Error: %s", name, strerror(errno));
}
