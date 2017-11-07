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

zcm = ZCM.Zcm("inproc")
if (!zcm.good())
    println("Unable to initialize zcm");
    exit()
end

sub = zcm.subscribe("EXAMPLE", example_t, handler, Void)

msg = example_t()

zcm.start()

msg.timestamp = 0;
zcm.publish("EXAMPLE", msg)
msg.timestamp = 1;
zcm.publish("EXAMPLE", msg)
msg.timestamp = 2;
zcm.publish("EXAMPLE", msg)

sleep(0.5)
zcm.stop()

zcm.start()

msg.timestamp = 3;
zcm.publish("EXAMPLE", msg)
msg.timestamp = 4;
zcm.publish("EXAMPLE", msg)
msg.timestamp = 5;
zcm.publish("EXAMPLE", msg)

sleep(0.5)
zcm.stop()

zcm.unsubscribe(sub)

@assert (numReceived == 6) "Didn't receive proper number of messages"
println("Success!")
