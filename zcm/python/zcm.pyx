# feel free to explore this file. It's not very long
# distutils: language = c
# distutils: sources = ['../zcm.c']
# distutils: include_dirs = ['../', '../../']
# distutils: libraries = ['zcm']
# distutils: library_dirs = ['../../build/zcm/']

from libc.stdint cimport uint32_t, int64_t, uint8_t
from cython cimport view

cdef extern from "Python.h":
    void PyEval_InitThreads()

cdef extern from "zcm.h":
    ctypedef struct zcm_t:
        pass
    ctypedef struct zcm_sub_t:
        pass
    ctypedef struct zcm_recv_buf_t:
        char* data
        uint32_t data_size
        pass
    #this one       RRR this one what?
    ctypedef void (*zcm_msg_handler_t)(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)

    zcm_t *zcm_create (const char *url)
    void   zcm_destroy(zcm_t *zcm)

    zcm_sub_t *zcm_subscribe  (zcm_t *zcm, const char *channel, zcm_msg_handler_t cb, void *usr)
    int        zcm_unsubscribe(zcm_t *zcm, zcm_sub_t *sub)
    int        zcm_publish    (zcm_t *zcm, const char *channel, const void *data, uint32_t dlen)

    void   zcm_run   (zcm_t *zcm)
    void   zcm_start (zcm_t *zcm)
    void   zcm_stop  (zcm_t *zcm)
    int    zcm_handle(zcm_t *zcm)

cdef class ZCMSubscription:
    cdef zcm_sub_t* sub
    cdef object handler
    cdef object msgtype

cdef void handler_cb(const zcm_recv_buf_t *rbuf, const char *channel, void *usr) with gil:
    subs = (<ZCMSubscription>usr)
    cdef view.array arr = <char[:rbuf.data_size, :1]> rbuf.data
    msg = subs.msgtype.decode(arr)
    subs.handler(channel, msg)

cdef class ZCM:
    cdef zcm_t* zcm
    def __cinit__(self, bytes url):
        PyEval_InitThreads()
        self.zcm = zcm_create(url)
    def good(self):
        return self.zcm != NULL
    def publish(self, bytes channel, object msg):
        _data = msg.encode()
        cdef const char* data = _data
        zcm_publish(self.zcm, channel, data, len(_data) * sizeof(uint8_t))
    def subscribe(self, bytes channel, msgtype, handler):
        cdef ZCMSubscription subs = ZCMSubscription()
        subs.handler = handler
        subs.msgtype = msgtype
        subs.sub = zcm_subscribe(self.zcm, channel, handler_cb, <void*> subs)
        return subs
    def unsubscribe(self, ZCMSubscription sub):
        zcm_unsubscribe(self.zcm, sub.sub)
    def handle(self):
        zcm_handle(self.zcm)
    def run(self):
        zcm_run(self.zcm)
    def start(self):
        zcm_start(self.zcm)
    def stop(self):
        zcm_stop(self.zcm)
    def __dealloc__(self):
        zcm_destroy(self.zcm)
