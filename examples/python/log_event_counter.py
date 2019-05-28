#!/usr/bin/python

from zerocm import ZCM, LogFile, LogEvent
import sys

if len(sys.argv) < 2:
    print("Usage: ./log_event_counter.py <logfile.log>")
    exit(1)

log = LogFile(sys.argv[1], 'r')

if not log.good():
    print("Unable to open log")
    exit(1)

i = 0
evt = log.readNextEvent()
while evt:
    i = i + 1
    evt = log.readNextEvent()
log.close()

print("Total events: " + str(i))
