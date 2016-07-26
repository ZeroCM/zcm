#!/usr/bin/python

from zcm import ZCM, LogFile, LogEvent
import sys, os
blddir= os.path.dirname(os.path.realpath(__file__)) + '/../build/'
sys.path.insert(0, blddir + "types/")
from example_t import example_t

msg = example_t()
msg.timestamp = 10

event = LogEvent()
event.setTimestamp (1)
event.setChannel   ("test channel")

log = LogFile('/tmp/testlog.log', 'w')
i = 0
while i < 100:
    msg.position[0] = i
    event.setData(msg.encode())
    log.writeEvent(event)
    i = i + 1
log.close()

from subprocess import call
cmd=["zcm-log-indexer", "-l/tmp/testlog.log", "-o/tmp/testlog.dbz",
      "-t" + blddir + "types/libexamplezcmtypes.so",
      "-p" + blddir + "cpp/libexample-indexer-plugin.so", "-r"]
call(cmd)
# To see the indexer command that runs here, uncomment the following two lines
print ' '.join(cmd)
#exit(1)

import json
with open('/tmp/testlog.dbz') as indexFile:
    index = json.load(indexFile)

log = LogFile('/tmp/testlog.log', 'r')

i = 0
while i < 100:
    msg.position[0] = i
    event.setData(msg.encode())
    # XXX Not sure a "long" is the right type here. Technically the number in
    #     the index was originally an off_t
    evt = log.readEventOffset(long(index['timestamp'][event.getChannel()][type(msg).__name__][i]))
    assert evt.getEventnum() == i, "Event nums dont match"
    assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
    assert evt.getChannel() == event.getChannel(), "Channels dont match"
    assert evt.getData() == event.getData(), "Data doesn't match"
    i = i + 1

i = 0
while i < 100:
    msg.position[0] = 100 - i - 1
    event.setData(msg.encode())
    # XXX Not sure a "long" is the right type here. Technically the number in
    #     the index was originally an off_t
    evt = log.readEventOffset(long(index['custom plugin'][event.getChannel()][type(msg).__name__][i]))
    assert evt.getEventnum() == 100 - i - 1, "Event nums dont match"
    assert evt.getTimestamp() == event.getTimestamp(), "Timestamps dont match"
    assert evt.getChannel() == event.getChannel(), "Channels dont match"
    assert evt.getData() == event.getData(), "Data doesn't match"
    i = i + 1

log.close()

call(["rm", "/tmp/testlog.log", "/tmp/testlog.dbz"])

print "Success"
