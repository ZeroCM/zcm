include("/usr/local/share/julia/zcm.jl");
include("../build/types/test_package/test_package_packaged_t.jl");

import ZCM;
using ZCM;

numReceived = 0
handler = function(rbuf::ZCM.RecvBuf, channel::String, msg::test_package_packaged_t, usr)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (((numReceived % 2) == 0) == msg.packaged) "Received message with incorrect packaged flag"
    numReceived = numReceived + 1
end

zcm = Zcm("inproc")
if (!zcm_good(zcm))
    println("Unable to initialize zcm");
    exit()
end

sub = zcm_subscribe(zcm, "EXAMPLE", test_package_packaged_t, handler, Void)

msg = test_package_packaged_t()

zcm_start(zcm)

msg.packaged = true;
zcm_publish(zcm, "EXAMPLE", msg)
msg.packaged = false;
zcm_publish(zcm, "EXAMPLE", msg)
msg.packaged = true;
zcm_publish(zcm, "EXAMPLE", msg)

sleep(0.5)
zcm_stop(zcm)

zcm_unsubscribe(zcm, sub)

@assert (numReceived == 3) "Didn't receive proper number of messages"
println("Success!")
