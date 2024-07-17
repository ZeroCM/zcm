#include "zcm/util/lockfile.h"

#include <string>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct Serial {
    Serial() {}
    ~Serial() { close(); }

    bool open(const std::string& port, int baud, bool hwFlowControl);
    bool isOpen() { return fd > 0; };
    void close();

    int write(const u8* buf, size_t sz);
    int read(u8* buf, size_t sz, u64 timeoutMs);
    // Returns 0 on invalid input baud otherwise returns termios constant baud
    // value
    static int convertBaud(int baud);

    Serial(const Serial&) = delete;
    Serial(Serial&&) = delete;
    Serial& operator=(const Serial&) = delete;
    Serial& operator=(Serial&&) = delete;

   private:
    std::string port;
    int fd = -1;
    lockfile_t* lf;
};
