#include "zcm/util/lockfile.h"
#include "zcm/util/debug.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <cstring>
#include <string>
using namespace std;

#define LOCK_PATH_PREFIX "/var/lock/LCK.."

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
    string ret = LOCK_PATH_PREFIX;
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
    if(fd < 0)
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
    if(kill((pid_t)pid, 0) < 0 && errno == ESRCH) {
        unlink(path.c_str());
        return false;
    }

    return true;
}


bool lockfile_trylock(const char *name)
{
    auto path = makeLockfilePath(name);
    if (isLocked(path)) {
        ZCM_DEBUG("Cannot lock '%s', it's already locked!", path.c_str());
        return false;
    }

    // Create lockfile compatible with UUCP-1.2
    int mask = umask(022);
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(fd < 0)
        return false; // Cannot create lock file (possibly a race)
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
    unlink(path.c_str());
}
