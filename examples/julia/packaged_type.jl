include("/usr/local/share/julia/zcm.jl");
include("../build/types/test_package/test_package_packaged_t.jl");

import ZCM;

numReceived = 0
handler = function(rbuf::ZCM.RecvBuf, channel::String, msg::test_package_packaged_t, usr)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (((numReceived % 2) == 0) == msg.packaged) "Received message with incorrect packaged flag"
    numReceived = numReceived + 1
end

zcm = ZCM.Zcm("inproc")
if (!zcm.good())
    println("Unable to initialize zcm");
    exit()
end

sub = zcm.subscribe("EXAMPLE", test_package_packaged_t, handler, Void)

msg = test_package_packaged_t()

zcm.start()

msg.packaged = true;
zcm.publish("EXAMPLE", msg)
msg.packaged = false;
zcm.publish("EXAMPLE", msg)
msg.packaged = true;
zcm.publish("EXAMPLE", msg)

sleep(0.5)
zcm.stop()

zcm.start()

msg.packaged = false;
zcm.publish("EXAMPLE", msg)
msg.packaged = true;
zcm.publish("EXAMPLE", msg)
msg.packaged = false;
zcm.publish("EXAMPLE", msg)

sleep(0.5)
zcm.stop()

zcm.unsubscribe(sub)

@assert (numReceived == 6) "Didn't receive proper number of messages"
println("Success!")
