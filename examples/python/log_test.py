#!/usr/bin/python

from zerocm import ZCM, LogFile, LogEvent
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
log.writeEvent(event)
log.close()

log = LogFile('testlog.log', 'r')
evt = log.readNextEvent()
assert evt.getEventnum() == 0, "Event nums dont match"
assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
assert evt.getChannel() == event.getChannel(), "Channels dont match"
assert evt.getData() == event.getData(), "Data doesn't match"

evt = log.readPrevEvent()
assert evt.getEventnum() == 0, "Event nums dont match"
assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
assert evt.getChannel() == event.getChannel(), "Channels dont match"
assert evt.getData() == event.getData(), "Data doesn't match"
log.close()

from subprocess import call
call(["rm", "testlog.log"])

print("Success")
