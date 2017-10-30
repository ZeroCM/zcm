include("/usr/local/share/julia/zcm.jl");

import ZCM;

numReceived = 0
handler = function(rbuf::ZCM.RecvBuf, channel::String, usr)
    println("Received message on channel: ", channel)
    global numReceived = numReceived + 1
end

zcm = ZCM.Zcm("nonblock-inproc")

sub = zcm.subscribeRaw("EXAMPLE", handler, Void)

zcm.publishRaw("EXAMPLE", [0x00], UInt32(length([0x00])))
zcm.handleNonblock()
zcm.publishRaw("EXAMPLE", [0x00], UInt32(length([0x00])))
zcm.handleNonblock()
zcm.publishRaw("EXAMPLE", [0x00], UInt32(length([0x00])))
zcm.handleNonblock()

zcm.unsubscribe(sub)

@assert (numReceived == 3) "Didn't receive proper number of messages"
println("Success!")
