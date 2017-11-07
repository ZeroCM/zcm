include("../build/types/example_t.jl");

using ZCM;

numReceived = 0
function handler(channel::String, msg::example_t)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (numReceived == msg.timestamp) "Received message with incorrect timestamp"
    numReceived = numReceived + 1
end

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

sub = subscribe(zcm, "EXAMPLE", example_t, handler)

msg = example_t()

start(zcm)

msg.timestamp = 0;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 1;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 2;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

start(zcm)

msg.timestamp = 3;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 4;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 5;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)

@assert (numReceived == 6) "Didn't receive proper number of messages"




println("Success!")
