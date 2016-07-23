#!/usr/bin/python

from zcm import ZCM, LogFile, LogEvent
import sys
sys.path.insert(0, '../build/types/')
from example_t import example_t

msg = example_t()
msg.timestamp = 10

event = LogEvent()
event.setTimestamp (1)
event.setChannel   ("test channel")
event.setData      (msg.encode())

log = LogFile('testlog.log', 'w')
i = 0
while i < 100:
    log.writeEvent(event)
    i = i + 1
log.close()

log = LogFile('testlog.log', 'r')
i = 0
evt = log.readNextEvent()
while evt:
    assert evt.getEventnum() == i, "Event nums dont match"
    assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
    assert evt.getChannel() == event.getChannel(), "Channels dont match"
    assert evt.getData() == event.getData(), "Data doesn't match"
    i = i + 1
    evt = log.readNextEvent()

evt = log.readPrevEvent()
while evt:
    i = i - 1
    assert evt.getEventnum() == i, "Event nums dont match"
    assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
    assert evt.getChannel() == event.getChannel(), "Channels dont match"
    assert evt.getData() == event.getData(), "Data doesn't match"
    evt = log.readPrevEvent()

from subprocess import call
call(["zcm-log-indexer", "-ltestlog.log", "-otestlog.dbz",
      "-t../build/types/libexamplezcmtypes.so", "-r"])

import json
from pprint import pprint

with open('testlog.dbz') as indexFile:
    index = json.load(indexFile)

i = 0
while i < 100:
    evt = log.readEventOffset(int(index[event.getChannel()][type(msg).__name__]['timestamp'][i]))
    assert evt.getEventnum() == i, "Event nums dont match"
    assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
    assert evt.getChannel() == event.getChannel(), "Channels dont match"
    assert evt.getData() == event.getData(), "Data doesn't match"
    i = i + 1

log.close()

call(["rm", "testlog.log", "testlog.dbz"])

print "Success"