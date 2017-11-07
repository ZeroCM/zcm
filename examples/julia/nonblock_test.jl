include("/usr/local/share/julia/zcm.jl");
include("../build/types/example_t.jl");

import ZCM;

numReceived = 0
handler = function(rbuf::ZCM.RecvBuf, channel::String, msg::example_t, usr)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (numReceived == msg.timestamp) "Received message with incorrect timestamp"
    numReceived = numReceived + 1
end

zcm = ZCM.Zcm("nonblock-inproc")
if (!zcm.good())
    println("Unable to initialize zcm");
    exit()
end

sub = zcm.subscribe("EXAMPLE", example_t, handler, Void)

msg = example_t()

msg.timestamp = 0;
zcm.publish("EXAMPLE", msg)
zcm.handleNonblock()
msg.timestamp = 1;
zcm.publish("EXAMPLE", msg)
zcm.handleNonblock()
msg.timestamp = 2;
zcm.publish("EXAMPLE", msg)
zcm.handleNonblock()

zcm.unsubscribe(sub)

@assert (numReceived == 3) "Didn't receive proper number of messages"
println("Success!")
