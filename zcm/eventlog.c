#include "zcm/eventlog.h"
#include "zcm/util/ioutils.h"
#include <assert.h>
#include <string.h>

#include <linux/mman.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <libexplain/mmap.h>
#include <errno.h>

#include "util/debug.h"

#define MAGIC ((int32_t) 0xEDA1DA01L)

zcm_eventlog_t *zcm_eventlog_create(const char *path, const char *mode)
{
    assert(!strcmp(mode, "r") || !strcmp(mode, "w") || !strcmp(mode, "a"));

    int prot;

    if (*mode == 'w') {
        prot = PROT_WRITE;
        mode = "wb";
    } else if (*mode == 'r') {
        prot = PROT_READ;
        mode = "rb";
    } else if (*mode == 'a') {
        prot = PROT_WRITE;
        mode = "ab";
    } else {
        return NULL;
    }

    ZCM_DEBUG("opening file");
    FILE* f = fopen(path, mode);
    if (!f) return NULL;

    int fd = fileno(f);

    size_t aligned_size;
    struct stat sb;
    uint8_t *data;
    if (prot != PROT_WRITE) {
        ZCM_DEBUG("getting size");
        if (fstat(fd, &sb) == -1) return NULL;

        aligned_size = ((sb.st_size / sysconf(_SC_PAGESIZE)) + 1) * sysconf(_SC_PAGESIZE);

        ZCM_DEBUG("mapping log into memory %lu %lu %lu", sb.st_size, aligned_size, sysconf(_SC_PAGESIZE));
        data = mmap(NULL, aligned_size, prot, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            ZCM_DEBUG("errno %s", explain_mmap(NULL, aligned_size, prot, MAP_SHARED, fd, 0));
            perror("");
            fclose(f);
            return NULL;
        }
        fclose(f);
    }

    ZCM_DEBUG("allocating memory");
    zcm_eventlog_t *l = (zcm_eventlog_t*) malloc(sizeof(zcm_eventlog_t));
    if (!l) return NULL;

    if (prot == PROT_WRITE) {
        l->aligned_size = 0;
        l->data = NULL;
        l->size = 0;
        l->f = f;
    } else {
        l->aligned_size = aligned_size;
        l->data = data;
        l->size = sb.st_size;
        l->f = NULL;
    }

    l->offset = 0;
    l->eventcount = 0;

    ZCM_DEBUG("done");
    return l;
}

void zcm_eventlog_destroy(zcm_eventlog_t *l)
{
    if (l->f) {
        fflush(l->f);
        fclose(l->f);
    } else {
        munmap(l->data, l->size);
    }
    free(l);
}

void zcm_eventlog_flush(zcm_eventlog_t *l)
{
    if (l->f) fflush(l->f);
}

int zcm_eventlog_seek_to_offset(zcm_eventlog_t *l, off_t offset, int whence)
{
    if (l->f) return fseeko(l->f, offset, whence);

    switch (whence) {
        case SEEK_SET:
            if (offset > l->size) return -1;
            l->offset = offset;
            break;
        case SEEK_CUR:
            if (l->offset + offset > l->size) return -1;
            l->offset += offset;
            break;
        case SEEK_END:
            if (offset > l->size) return -1;
            l->offset = l->size - offset;
            break;
        default:
            return -1;
    }
    return 0;
}

off_t zcm_eventlog_get_cursor(const zcm_eventlog_t* l)
{
    return l->f ? ftello(l->f) : l->offset;
}

// Returns 0 on success -1 on failure
static int sync_stream(zcm_eventlog_t *l)
{
    uint32_t magic = 0;
    int r;
    do {
        if (l->offset >= l->size) return -1;
        r = l->data[l->offset++];
        magic = (magic << 8) | r;
    } while( (int32_t)magic != MAGIC );
    return 0;
}

static int sync_stream_backwards(zcm_eventlog_t *l)
{
    uint32_t magic = 0;
    int r;
    do {
        if (l->offset < 2) return -1;
        l->offset -= 2;
        r = l->data[l->offset++];
        magic = ((magic >> 8) & 0x00ffffff) | (((uint32_t)r << 24) & 0xff000000);
    } while( (int32_t)magic != MAGIC );
    l->offset += sizeof(uint32_t) - 1;
    return 0;
}

static int64_t get_next_event_time(zcm_eventlog_t *l)
{
    if (sync_stream(l)) return -1;

    int64_t event_num;
    int64_t timestamp;
    if (0 != read64(l, &event_num)) return -1;
    if (0 != read64(l, &timestamp)) return -1;
    l->offset -= sizeof(int64_t) * 2 + sizeof(int32_t);

    l->eventcount = event_num;

    return timestamp;
}

int zcm_eventlog_seek_to_timestamp(zcm_eventlog_t *l, int64_t timestamp)
{
    int64_t cur_time;
    double frac1 = 0;               // left bracket
    double frac2 = 1;               // right bracket
    double prev_frac = -1;
    double frac;                    // current position

    while (1) {
        frac = 0.5 * (frac1 + frac2);
        if (zcm_eventlog_seek_to_offset(l, frac * l->size, SEEK_SET) < 0)
            return -1;
        cur_time = get_next_event_time(l);
        if (cur_time < 0)
            return -1;

        if ((frac > frac2) || (frac < frac1) || (frac1 >= frac2))
            break;

        double df = frac - prev_frac;
        if (df < 0) df = -df;
        if (df < 1e-12) break;

        if (cur_time == timestamp) break;

        if (cur_time < timestamp) frac1 = frac;
        else frac2 = frac;

        prev_frac = frac;
    }

    return 0;
}

static zcm_eventlog_event_t *zcm_event_read_helper(zcm_eventlog_t *l, int rewindWhenDone)
{
    zcm_eventlog_event_t *le =
        (zcm_eventlog_event_t*) calloc(1, sizeof(zcm_eventlog_event_t));

    off_t startOffset = l->offset;

    if (read64(l, &le->eventnum)) {
        free(le);
        le = NULL;
        goto done;
    }

    if (read64(l, &le->timestamp)) {
        free(le);
        le = NULL;
        goto done;
    }

    if (read32(l, &le->channellen)) {
        free(le);
        le = NULL;
        goto done;
    }

    if (read32(l, &le->datalen)) {
        free(le);
        le = NULL;
        goto done;
    }

    // Sanity check the channel length and data length
    if (le->channellen <= 0 || le->channellen >= 1000) {
        fprintf(stderr, "Log event has invalid channel length: %d\n", le->channellen);
        free(le);
        le = NULL;
        goto done;
    }
    if (le->datalen < 0) {
        fprintf(stderr, "Log event has invalid data length: %d\n", le->datalen);
        free(le);
        le = NULL;
        goto done;
    }

    le->channel = (char *) calloc(1, le->channellen+1);
    int readRet = readn(l, (uint8_t*)le->channel, le->channellen);
    if (readRet < 0) {
        free(le->channel);
        free(le);
        le = NULL;
        goto done;
    }

    le->data = calloc(1, le->datalen+1);
    readRet = readn(l, le->data, le->datalen);
    if (readRet < 0) {
        free(le->channel);
        free(le->data);
        free(le);
        le = NULL;
        goto done;
    }

    // Check that there's a valid event or the EOF after this event.
    int32_t next_magic;
    if (0 == read32(l, &next_magic)) {
        if (next_magic != MAGIC) {
            fprintf(stderr, "Invalid header after log data %i != %i\n", next_magic, MAGIC);
            free(le->channel);
            free(le->data);
            free(le);
            le = NULL;
            return NULL;
        }
        l->offset -= 4;
    }

done:
    if (rewindWhenDone)
        zcm_eventlog_seek_to_offset(l, startOffset > 4 ? startOffset - 4 : 0, SEEK_SET);
    return le;
}

zcm_eventlog_event_t *zcm_eventlog_read_next_event(zcm_eventlog_t *l)
{
    if (sync_stream(l)) return NULL;
    return zcm_event_read_helper(l, 0);
}

zcm_eventlog_event_t *zcm_eventlog_read_prev_event(zcm_eventlog_t *l)
{
    if (sync_stream_backwards(l) < 0) return NULL;
    return zcm_event_read_helper(l, 1);
}

zcm_eventlog_event_t *zcm_eventlog_read_event_at_offset(zcm_eventlog_t *l, off_t offset)
{
    if (zcm_eventlog_seek_to_offset(l, offset, SEEK_SET) < 0) return NULL;
    if (sync_stream(l)) return NULL;
    return zcm_event_read_helper(l, 0);
}

void zcm_eventlog_free_event(zcm_eventlog_event_t *le)
{
    if (le->data) free(le->data);
    if (le->channel) free(le->channel);
    memset(le,0,sizeof(zcm_eventlog_event_t));
    free(le);
}

int zcm_eventlog_write_event(zcm_eventlog_t *l, const zcm_eventlog_event_t *le)
{
    if (0 != fwrite32(l->f, MAGIC)) return -1;

    if (0 != fwrite64(l->f, l->eventcount)) return -1;

    if (0 != fwrite64(l->f, le->timestamp)) return -1;
    if (0 != fwrite32(l->f, le->channellen)) return -1;
    if (0 != fwrite32(l->f, le->datalen)) return -1;

    if (le->channellen != fwrite(le->channel, 1, le->channellen, l->f))
        return -1;
    if (le->datalen != fwrite(le->data, 1, le->datalen, l->f))
        return -1;

    l->eventcount++;

    return 0;
}
