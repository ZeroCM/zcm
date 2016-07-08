from libc.stdint cimport int64_t, int32_t, uint32_t, uint8_t

cdef extern from "Python.h":
    void PyEval_InitThreads()

cdef extern from "zcm/zcm.h":
    ctypedef struct zcm_t:
        pass
    ctypedef struct zcm_sub_t:
        pass
    ctypedef struct zcm_recv_buf_t:
        char* data
        uint32_t data_size
        pass
    ctypedef void (*zcm_msg_handler_t)(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)

    zcm_t *zcm_create (const char *url)
    void   zcm_destroy(zcm_t *zcm)

    int         zcm_errno   (zcm_t *zcm)
    const char* zcm_strerror(zcm_t *zcm)

    zcm_sub_t *zcm_subscribe  (zcm_t *zcm, const char *channel, zcm_msg_handler_t cb, void *usr)
    int        zcm_unsubscribe(zcm_t *zcm, zcm_sub_t *sub)
    int        zcm_publish    (zcm_t *zcm, const char *channel, const void *data, uint32_t dlen)
    void       zcm_flush      (zcm_t *zcm)

    void   zcm_run   (zcm_t *zcm)
    void   zcm_start (zcm_t *zcm)
    void   zcm_stop  (zcm_t *zcm)
    int    zcm_handle(zcm_t *zcm)

    ctypedef struct zcm_eventlog_t:
        pass
    ctypedef struct zcm_eventlog_event_t:
        int64_t  eventnum
        int64_t  timestamp
        int32_t  channellen
        int32_t  datalen
        char    *channel
        void    *data

    zcm_eventlog_t *zcm_eventlog_create(const char *path, const char *mode)
    void            zcm_eventlog_destroy(zcm_eventlog_t *eventlog)

    int zcm_eventlog_seek_to_timestamp(zcm_eventlog_t *eventlog, int64_t ts)

    zcm_eventlog_event_t *zcm_eventlog_read_next_event(zcm_eventlog_t *eventlog)
    zcm_eventlog_event_t *zcm_eventlog_read_prev_event(zcm_eventlog_t *eventlog)
    void                  zcm_eventlog_free_event(zcm_eventlog_event_t *event)
    int                   zcm_eventlog_write_event(zcm_eventlog_t *eventlog, \
                                                   zcm_eventlog_event_t *event)

cdef class ZCMSubscription:
    cdef zcm_sub_t* sub
    cdef object handler
    cdef object msgtype

cdef void handler_cb(const zcm_recv_buf_t *rbuf, const char *channel, void *usr) with gil:
    subs = (<ZCMSubscription>usr)
    msg = subs.msgtype.decode(rbuf.data[:rbuf.data_size])
    subs.handler(channel, msg)

cdef class ZCM:
    cdef zcm_t* zcm
    def __cinit__(self, bytes url=<bytes>""):
        PyEval_InitThreads()
        self.zcm = zcm_create(url)
    def __dealloc__(self):
        zcm_destroy(self.zcm)
    def good(self):
        return self.zcm != NULL
    def err(self):
        return zcm_errno(self.zcm)
    def strerror(self):
        return <bytes>zcm_strerror(self.zcm)
    def subscribe(self, bytes channel, msgtype, handler):
        cdef ZCMSubscription subs = ZCMSubscription()
        subs.handler = handler
        subs.msgtype = msgtype
        subs.sub = zcm_subscribe(self.zcm, channel, handler_cb, <void*> subs)
        return subs
    def unsubscribe(self, ZCMSubscription sub):
        zcm_unsubscribe(self.zcm, sub.sub)
    def publish(self, bytes channel, object msg):
        _data = msg.encode()
        cdef const char* data = _data
        zcm_publish(self.zcm, channel, data, len(_data) * sizeof(uint8_t))
    def flush(self):
        zcm_flush(self.zcm)
    def run(self):
        zcm_run(self.zcm)
    def start(self):
        zcm_start(self.zcm)
    def stop(self):
        zcm_stop(self.zcm)
    def handle(self):
        zcm_handle(self.zcm)

cdef class LogEvent:
    cdef int64_t eventnum
    cdef int64_t timestamp
    cdef object  channel
    cdef object  data
    def __cinit__(self):
        pass
    def setEventnum(self, num):
        self.eventnum = num
    def getEventnum(self):
        return self.eventnum
    def setTimestamp(self, time):
        self.timestamp = time
    def getTimestamp(self):
        return self.timestamp
    def setChannel(self, bytes chan):
        self.channel = chan
    def getChannel(self):
        return self.channel
    def setData(self, bytes data):
        self.data = data
    def getData(self):
        return self.data

cdef class LogFile:
    cdef zcm_eventlog_t* eventlog
    cdef zcm_eventlog_event_t* lastevent
    def __cinit__(self, bytes path, bytes mode):
        self.eventlog = zcm_eventlog_create(path, mode)
        self.lastevent = NULL
    def __dealloc__(self):
        self.close()
    def close(self):
        if self.eventlog != NULL:
            zcm_eventlog_destroy(self.eventlog)
            self.eventlog = NULL
        if self.lastevent != NULL:
            zcm_eventlog_free_event(self.lastevent)
            self.lastevent = NULL
    def good(self):
        return self.eventlog != NULL
    def seekToTimestamp(self, int64_t timestamp):
        return zcm_eventlog_seek_to_timestamp(self.eventlog, timestamp)
    cdef __setCurrentEvent(self, zcm_eventlog_event_t* evt):
        if self.lastevent != NULL:
            zcm_eventlog_free_event(self.lastevent)
        self.lastevent = evt
        cdef LogEvent curEvent = LogEvent()
        if evt == NULL:
            return None
        curEvent.setEventnum  (evt.eventnum)
        curEvent.setChannel   (evt.channel[:evt.channellen])
        curEvent.setTimestamp (evt.timestamp)
        curEvent.setData      ((<char*>evt.data)[:evt.datalen])
        return curEvent
    def readNextEvent(self):
        cdef zcm_eventlog_event_t* evt = zcm_eventlog_read_next_event(self.eventlog)
        return self.__setCurrentEvent(evt)
    def readPrevEvent(self):
        cdef zcm_eventlog_event_t* evt = zcm_eventlog_read_prev_event(self.eventlog)
        return self.__setCurrentEvent(evt)
    def writeEvent(self, LogEvent event):
        cdef zcm_eventlog_event_t evt
        evt.eventnum   = event.eventnum
        evt.timestamp  = event.timestamp
        evt.channellen = len(event.channel)
        evt.datalen    = len(event.data)
        evt.channel    = <char*> event.channel
        evt.data       = <char*> event.data
        return zcm_eventlog_write_event(self.eventlog, &evt);
