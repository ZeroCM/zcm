include("/usr/local/share/julia/zcm.jl");
include("../build/types/example_t.jl");

import ZCM;
using ZCM;

numReceived = 0
handler = function(rbuf::ZCM.RecvBuf, channel::String, msg::example_t, usr)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (numReceived == msg.timestamp) "Received message with incorrect timestamp"
    numReceived = numReceived + 1
end

zcm = Zcm("inproc")
if (!zcm_good(zcm))
    println("Unable to initialize zcm");
    exit()
end

sub = zcm_subscribe(zcm, "EXAMPLE", example_t, handler, Void)

msg = example_t()

zcm_start(zcm)

msg.timestamp = 0;
zcm_publish(zcm, "EXAMPLE", msg)
msg.timestamp = 1;
zcm_publish(zcm, "EXAMPLE", msg)
msg.timestamp = 2;
zcm_publish(zcm, "EXAMPLE", msg)

sleep(0.5)
zcm_stop(zcm)

zcm_start(zcm)

msg.timestamp = 3;
zcm_publish(zcm, "EXAMPLE", msg)
msg.timestamp = 4;
zcm_publish(zcm, "EXAMPLE", msg)
msg.timestamp = 5;
zcm_publish(zcm, "EXAMPLE", msg)

sleep(0.5)
zcm_stop(zcm)

zcm_unsubscribe(zcm, sub)

@assert (numReceived == 6) "Didn't receive proper number of messages"
println("Success!")
