using ZCM
using juliazcm.types: example_t

numReceived = 0
handler = function(rbuf, channel::String, msg::example_t)
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel)")
    global numReceived
    @assert (numReceived == msg.timestamp) "Received message with incorrect timestamp"
    numReceived = numReceived + 1
end

zcm = Zcm("nonblock-inproc")
if (!good(zcm))
    println("Unable to initialize zcm");
    exit()
end

sub = subscribe(zcm, "EXAMPLE", handler, example_t)

msg = example_t()

msg.timestamp = 0;
publish(zcm, "EXAMPLE", msg)
handle(zcm, 0)
msg.timestamp = 1;
publish(zcm, "EXAMPLE", msg)
handle(zcm, 0)
msg.timestamp = 2;
publish(zcm, "EXAMPLE", msg)
handle(zcm, 0)

unsubscribe(zcm, sub)

@assert (numReceived == 3) "Didn't receive proper number of messages"
println("Success!")
