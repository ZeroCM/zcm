#include "zcm/util/lockfile.h"
#include "zcm/util/debug.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <cstring>
#include <string>
using namespace std;

#define DEFAULT_LOCK_DIR "/var/lock/zcm"
#define LOCK_PREFIX "LCK.."

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

static bool isLocked(const string& path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;

    char buf[128];
    int n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n == 0)
        return false;
    int pid = -1;
    buf[n] = 0;
    sscanf(buf, "%d", &pid);
    if (pid <= 0)
        return false;

    // Check to see if the process that created the file is still alive
    if (kill((pid_t)pid, 0) < 0 && errno == ESRCH) {
        unlink(path.c_str());
        return false;
    }

    return true;
}


bool lockfile_trylock(const char *name)
{
    auto path = makeLockfilePath(name);
    if (path == "") return false;
    if (isLocked(path)) {
        ZCM_DEBUG("Cannot lock '%s', it's already locked!", path.c_str());
        return false;
    }

    // Create lockfile compatible with UUCP-1.2
    int mask = umask(022);
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        ZCM_DEBUG("Unable to create lockfile: '%s'", path.c_str());
        return false; // Cannot create lock file (possibly a race)
    }
    umask(mask);
    uid_t uid = getuid();
    gid_t gid = getgid();
    int ret1 = chown(path.c_str(), uid, gid);
    (void) ret1;
    char buf[128];
    snprintf(buf, sizeof(buf), "%10ld zcm %.20s\n", (long)getpid(), (getpwuid(uid))->pw_name);
    ssize_t ret2 = write(fd, buf, strlen(buf));
    (void) ret2;
    close(fd);

    return true;
}

void lockfile_unlock(const char *name)
{
    auto path = makeLockfilePath(name);
    int err = unlink(path.c_str());
    if (err < 0)
        ZCM_DEBUG("Failed to unlink lockfile '%s'. Error: %s", name, strerror(errno));
}
