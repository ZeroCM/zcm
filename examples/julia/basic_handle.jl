using ZCM
using juliazcm.types: example_t

numReceived = 0
function handler(rbuf, channel::String, msg::example_t)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (numReceived == msg.timestamp) "Received message with incorrect timestamp"
    numReceived = numReceived + 1
end

# a handler that receives the raw message bytes
function untyped_handler(rbuf, channel::String, msgdata::Vector{UInt8})
    println("Recieved raw message data on channel: ", channel)
    decode(example_t, msgdata)
end

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

sub = subscribe(zcm, "EXAMPLE", handler, example_t)
sub2 = subscribe(zcm, "EXAMPLE", untyped_handler)

msg = example_t()

msg.timestamp = 0;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 1;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 2;
publish(zcm, "EXAMPLE", msg)

handle(zcm)
handle(zcm)
handle(zcm)

unsubscribe(zcm, sub)
unsubscribe(zcm, sub2)

@assert (numReceived == 3) "Didn't receive proper number of messages"

println("Success!")
