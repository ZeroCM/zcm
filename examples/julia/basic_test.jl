using ZCM
using juliazcm.types: example_t

numReceived = 0
function handler(rbuf, channel::String, msg::example_t)
    global numReceived
    @assert (numReceived == msg.timestamp) "Received message with incorrect timestamp"
    numReceived = numReceived + 1
end

# a handler that receives the raw message bytes
function untyped_handler(rbuf, channel::String, msgdata::Vector{UInt8})
    decode(example_t, msgdata)
end

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

println("Constructed")

sub = subscribe(zcm, "EXAMPLE", handler, example_t)
sub2 = subscribe(zcm, "EXAMPLE", untyped_handler)

println("Subscribed")

msg = example_t()

start(zcm)

println("Started")

msg.timestamp = 0;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 1;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 2;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)

msg.timestamp = 3;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 4;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 5;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

println("Stopped")

unsubscribe(zcm, sub)
unsubscribe(zcm, sub2)

println("Unsubscribed")

@assert (numReceived == 6) "Didn't receive proper number of messages"

println("Success!")
