using ZCM
using juliazcm.types: bitfield_t

function handler(rbuf, channel::String, msg::bitfield_t)
    ccall(:jl_, Nothing, (Any,), "$(msg)\n")
end

zcm = Zcm("ipc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

sub = subscribe(zcm, "BITFIELD", handler, bitfield_t)

start(zcm)
while (true)
    sleep(0.5)
end
stop(zcm)

unsubscribe(zcm, sub)
