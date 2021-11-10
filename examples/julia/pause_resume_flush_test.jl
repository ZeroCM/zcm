using ZCM
using juliazcm.types: example_t

numReceived = 0
function handler(rbuf, channel::String, msg::example_t)
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel) at time $(msg.timestamp)")
    global numReceived
    @assert (numReceived == msg.timestamp) "Received message with incorrect timestamp"
    numReceived = numReceived + 1
end

# a handler that receives the raw message bytes
function untyped_handler(rbuf, channel::String, msgdata::Vector{UInt8})
    ccall(:jl_, Nothing, (Any,), "Received raw message data on channel $(channel)")
    decode(example_t, msgdata)
end

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

sub = subscribe(zcm, "EXAMPLE", handler, example_t)
sub2 = subscribe(zcm, "EXAMPLE", untyped_handler)

msg = example_t()

set_queue_size(zcm, 10)

start(zcm)

msg.timestamp = 0;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 1;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 2;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)

pause(zcm)

msg.timestamp = 3;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 4;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 5;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)

@assert (numReceived == 3) "Received a message while paused : $numReceived"

flush(zcm) # flushes messages out on publish

sleep(0.5)

flush(zcm) # flushes message in on receive

@assert (numReceived == 6) "Did not receive all messages after flush : $numReceived"

resume(zcm)

msg.timestamp = 6;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 7;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 8;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)
unsubscribe(zcm, sub2)

@assert (numReceived == 9) "Did not receive all messages after resume : $numReceived"

println("Success!")
