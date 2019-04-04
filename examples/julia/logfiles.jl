using ZCM
using juliazcm.types: example_t

msg = example_t()

zlog = LogFile("/tmp/zcm.log", "w")
if (!good(zlog))
    error("Unable to initialize zcm log in write mode")
end

msg.timestamp = 0;
write_event(zlog, LogEvent("EXAMPLE", msg, Int64(0e6)))
msg.timestamp = 1;
write_event(zlog, LogEvent("EXAMPLE", msg, Int64(1e6)))
msg.timestamp = 2;
write_event(zlog, LogEvent("EXAMPLE", msg, Int64(2e6)))

# Note: it's actually important to call finalize here because we need to be sure
#       the underlying file is closed properly before we open it again. We could
#       instead call "destroy()" but finalize will also force the garbage collector
#       to deallocate the memory so we can be sure that it's completely gone.
finalize(zlog)

zlog = LogFile("/tmp/zcm.log", "a")
if (!good(zlog))
    error("Unable to initialize zcm log in append mode")
end

msg.timestamp = 3;
write_event(zlog, LogEvent("EXAMPLE", msg, Int64(3e6)))
msg.timestamp = 4;
write_event(zlog, LogEvent("EXAMPLE", msg, Int64(4e6)))
msg.timestamp = 5;
write_event(zlog, LogEvent("EXAMPLE", msg, Int64(5e6)))

# Note: it's actually important to call finalize here because we need to be sure
#       the underlying file is closed properly before we open it again
finalize(zlog)
zlog = LogFile("/tmp/zcm.log", "r")
if (!good(zlog))
    error("Unable to initialize zcm log in read mode")
end

i = 0
event = read_next_event(zlog)
while (good(event))
    global i, event
    msg = decode(example_t, event.data)

    @assert (event.utime   == Int64(i * 1e6)) "Bad event utime"
    @assert (event.channel == "EXAMPLE")      "Bad event channel"
    @assert (msg.timestamp == i)              "Bad msg timestamp"

    event = read_next_event(zlog)
    i += 1
end

@assert (i == 6) "Wrong number of events in file"
println("Success!")
