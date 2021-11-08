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

b = bitfield_t()
b.field1 = 3;
b.field2[1, 1] = 1;
b.field2[1, 2] = 0;
b.field2[1, 3] = 1;
b.field2[1, 4] = 0;
b.field2[2, 1] = 1;
b.field2[2, 2] = 0;
b.field2[2, 3] = 1;
b.field2[2, 4] = 0;
b.field3 = 0xf;
b.field4 = 5;
b.field5 = 7;
b.field9 = 1 << 27;
b.field10 = (Int64(1) << 52) | 1;

start(zcm)
while (true)
    publish(zcm, "BITFIELD", b)
    sleep(1)
end
stop(zcm)

unsubscribe(zcm, sub)
