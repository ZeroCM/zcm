#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/transport_register.hpp"
#include "zcm/transport/lockfree/lf_bcast.h"
#include "zcm/transport/lockfree/lf_shm.h"
#include "zcm/transport/lockfree/lf_util.h"
#include "zcm/util/debug.h"
#include "util/TimeUtil.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <cassert>
#include <unistd.h>

// Define this the class name you want
#define ZCM_TRANS_NAME TransportIpcShm
#define DEFAULT_MSG_MAXSZ 1024
#define DEFAULT_DEPTH 16

typedef struct Msg Msg;
struct __attribute__((aligned(256))) Msg
{
  u64  size;
  char channel[ZCM_CHANNEL_MAXLEN+1];

  __attribute__((aligned(256))) u8 payload[];
};

static inline bool parse_u64(const char *s, uint64_t *_num)
{
  uint64_t num = 0;
  while (1) {
    char c = *s++;
    if (!c) break;
    if (!('0' <= c && c <= '9')) return false; // not a decimal digit

    uint64_t next_num = 10*num + (uint64_t)(c-'0');
    if (next_num < num) return false; // overflow!
    num = next_num;
  }

  *_num = num;
  return true;
}

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    lf_bcast_t *bcast = nullptr;
    lf_bcast_sub_t sub[1] = {{}};

    Msg *recv = nullptr;

    size_t msg_maxsz = DEFAULT_MSG_MAXSZ;
    size_t msg_align = alignof(Msg);
    size_t queue_depth = DEFAULT_DEPTH;

    void *mem = nullptr;
    size_t shm_size = 0;

    ZCM_TRANS_CLASSNAME(zcm_url_t *url)
    {
        trans_type = ZCM_BLOCKING;
        vtbl = &methods;

        ZCM_DEBUG("Init ipcshm:");
        ZCM_DEBUG("  address='%s'", zcm_url_address(url));
        zcm_url_opts_t *opts = zcm_url_opts(url);
        for (size_t i = 0; i < opts->numopts; i++) {
          ZCM_DEBUG("  %s='%s'", opts->name[i], opts->value[i]);
        }

        for (size_t i = 0; i < opts->numopts; i++) {
          u64 tmp;
          if (0 == strcmp(opts->name[i], "mtu")) {
            if (parse_u64(opts->value[i], &tmp)) {
              ZCM_DEBUG("Setting mtu=%" PRIu64, tmp);
              msg_maxsz = LF_ALIGN_UP(tmp, msg_align);
            }
          }
          if (0 == strcmp(opts->name[i], "depth")) {
            if (parse_u64(opts->value[i], &tmp)) {
              ZCM_DEBUG("Setting queue_depth=%" PRIu64, tmp);
              queue_depth = tmp;
            }
          }
        }

        const char *region_name = zcm_url_address(url);

        size_t region_size, region_align;
        lf_bcast_footprint(queue_depth, msg_maxsz, msg_align, &region_size, &region_align);

        region_size = LF_ALIGN_UP(region_size, 16384); // FIXME Page size constant

        bool created = lf_shm_create(region_name, region_size);
        ZCM_DEBUG("Shm region created: %d", (int)created);

        mem = lf_shm_open(region_name, &shm_size);
        if (shm_size != region_size) {
          ZCM_DEBUG("SHM Region size mismatch: expected %zu, got %zu", region_size, shm_size);
          lf_shm_close(mem, shm_size);
          return;
        }

        if (created) {
          bcast = lf_bcast_mem_init(mem, queue_depth, msg_maxsz, msg_align);
        } else {
          bcast = lf_bcast_mem_join(mem, queue_depth, msg_maxsz, msg_align);
        }

        if (!bcast) {
          ZCM_DEBUG("Failed to init or join shm region");
          lf_shm_close(mem, shm_size);
          return;
        }

        lf_bcast_sub_init(sub, bcast);

	int ret = posix_memalign((void**)&recv, msg_align, msg_maxsz);
	if (ret != 0) {
	  ZCM_DEBUG("Failed allocate recvbuf");
	  lf_bcast_mem_leave(bcast);
          lf_shm_close(mem, shm_size);
	  bcast = NULL;
          return;
	}
	
        ZCM_DEBUG("Created ipcshm transport");
    }

    ~ZCM_TRANS_CLASSNAME()
    {
      if (recv) free(recv);
      if (bcast) lf_bcast_mem_leave(bcast);
      if (mem) lf_shm_close(mem, shm_size);
    }

    bool good()
    {
      return !!bcast;
    }

    /********************** METHODS **********************/
    size_t get_mtu()
    {
      return msg_maxsz;
    }

    int sendmsg(zcm_msg_t msg)
    {
      if (msg.len > msg_maxsz) { // FIMXE: DOESN'T ACCOUNT FOR HEADER SPACE!!!
	ZCM_DEBUG("Message length: %zu", msg.len);
	ZCM_DEBUG("MTU Limit: %zu", msg_maxsz);
        return ZCM_EINVALID;
      }

      size_t channel_len = strlen(msg.channel);
      assert(channel_len <= ZCM_CHANNEL_MAXLEN);

      Msg *m = (Msg*)lf_bcast_buf_acquire(bcast);
      assert(m); // FIXME

      m->size = msg.len;
      assert(channel_len < sizeof(m->channel));
      memcpy(m->channel, msg.channel, channel_len+1);

      i64 start = wallclock();
      memcpy(m->payload, msg.buf, msg.len);
      i64 dt = wallclock() - start;
      printf("sendmsg memcpy tm: %ld\n", dt);
      
      lf_bcast_pub(bcast, m);
      return ZCM_EOK;
    }

    int recvmsg_enable(const char *channel, bool enable)
    {
        return ZCM_EOK;
    }

    int recvmsg(zcm_msg_t *msg, int timeout)
    {
        i64 timeout_nanos = (i64)timeout * 1000000;

        // Try to get the next message in the queue
        const void *buf = lf_bcast_sub_consume_begin(sub, timeout_nanos);
        if (!buf) return ZCM_EAGAIN;

        // Copy it very defensively: the memory is shared and may be invalidated
        // at any time while we copy
        const Msg *m = (const Msg*)buf;
        size_t len = m->size;

        // FIXME validate 'len'

        memcpy(recv->channel, m->channel, sizeof(recv->channel));

	i64 start = wallclock();
        memcpy(recv->payload, m->payload, len);
	i64 dt = wallclock() - start;
	printf("recvmsg memcpy tm: %ld\n", dt);

        bool ref_valid = lf_bcast_sub_consume_end(sub);
        bool channel_valid = !!strchr(recv->channel, 0);

        bool valid = ref_valid & channel_valid;
        if (!valid) {
          // Message was invalidated.. treat as drop
          return ZCM_EAGAIN;
        }

        // All good, prepare return struct
        msg->utime = TimeUtil::utime();
        msg->channel = recv->channel;
        msg->len = len;
        msg->buf = (uint8_t*)recv->payload;
        return ZCM_EOK;
    }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME *cast(zcm_trans_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _get_mtu(zcm_trans_t *zt)
    { return cast(zt)->get_mtu(); }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static int _recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)
    { return cast(zt)->recvmsg_enable(channel, enable); }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
    { return cast(zt)->recvmsg(msg, timeout); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }

    /** If you choose to use the registrar, use a static registration member **/
    static const TransportRegister reg;
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_get_mtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsg_enable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    NULL, // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static zcm_trans_t *create(zcm_url_t *url)
{
    auto *trans = new ZCM_TRANS_CLASSNAME(url);
    if (trans->good())
        return trans;

    delete trans;
    return nullptr;
}

const TransportRegister ZCM_TRANS_CLASSNAME::reg(
    "ipcshm", "Transfer data using IPC through SHM", create);
