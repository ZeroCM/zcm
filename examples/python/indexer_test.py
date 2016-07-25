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

log = LogFile('testlog.log', 'w')
i = 0
while i < 100:
    msg.position[0] = i
    event.setData(msg.encode())
    log.writeEvent(event)
    i = i + 1
log.close()

from subprocess import call
cmd=["zcm-log-indexer", "-ltestlog.log", "-otestlog.dbz",
      "-t../build/types/libexamplezcmtypes.so",
      "-p../build/cpp/libexample-indexer-plugin.so", "-r"]
# To see the indexer command that runs here, uncomment the following two lines
#print ' '.join(cmd)
#exit(1)
call(cmd)

import json
with open('testlog.dbz') as indexFile:
    index = json.load(indexFile)

log = LogFile('testlog.log', 'r')

i = 0
while i < 100:
    msg.position[0] = i
    event.setData(msg.encode())
    evt = log.readEventOffset(int(index['timestamp'][event.getChannel()][type(msg).__name__][i]))
    assert evt.getEventnum() == i, "Event nums dont match"
    assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
    assert evt.getChannel() == event.getChannel(), "Channels dont match"
    assert evt.getData() == event.getData(), "Data doesn't match"
    i = i + 1

i = 0
while i < 100:
    msg.position[0] = 100 - i - 1
    event.setData(msg.encode())
    evt = log.readEventOffset(int(index['custom plugin'][event.getChannel()][type(msg).__name__][i]))
    assert evt.getEventnum() == 100 - i - 1, "Event nums dont match"
    assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
    assert evt.getChannel() == event.getChannel(), "Channels dont match"
    assert evt.getData() == event.getData(), "Data doesn't match"
    i = i + 1

log.close()

call(["rm", "testlog.log", "testlog.dbz"])

print "Success"
