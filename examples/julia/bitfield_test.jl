using ZCM
using juliazcm.types: bitfield_t

numReceived = 0
function handler(rbuf, channel::String, msg::bitfield_t)
    global numReceived
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel)")
    @assert (msg.field1 == -1) "Bad decode of field1"
    @assert (msg.field2[1, 1] == -1) "Bad decode of field2"
    @assert (msg.field2[1, 2] ==  0) "Bad decode of field2"
    @assert (msg.field2[1, 3] == -1) "Bad decode of field2"
    @assert (msg.field2[1, 4] ==  0) "Bad decode of field2"
    @assert (msg.field2[2, 1] == -1) "Bad decode of field2"
    @assert (msg.field2[2, 2] ==  0) "Bad decode of field2"
    @assert (msg.field2[2, 3] == -1) "Bad decode of field2"
    @assert (msg.field2[2, 4] ==  0) "Bad decode of field2"
    @assert (msg.field3 == -1) "Bad decode of field3"
    @assert (msg.field4 == -3) "Bad decode of field4"
    @assert (msg.field5 == 7) "Bad decode of field5"
    @assert (msg.field6 == 0) "Bad decode of field6"
    @assert (msg.field7 == 0) "Bad decode of field7"
    @assert (msg.field8_dim1 == 0) "Bad decode of field8_dim1"
    @assert (msg.field8_dim2 == 0) "Bad decode of field8_dim2"
    @assert (msg.field8 == Matrix{Int8}(undef, 0, 0)) "Bad decode of field8"
    @assert (msg.field9 == ~(1 << 27) + 1) "Bad decode of field9"
    @assert (msg.field10 == (UInt64(1) << 52) | 1) "Bad decode of field10"
    numReceived = numReceived + 1
end

zcm = Zcm("inproc")
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

publish(zcm, "BITFIELD", b)
publish(zcm, "BITFIELD", b)
publish(zcm, "BITFIELD", b)

sleep(0.5)

publish(zcm, "BITFIELD", b)
publish(zcm, "BITFIELD", b)
publish(zcm, "BITFIELD", b)

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)

@assert (numReceived == 6) "Didn't receive proper number of messages"

println("Success!")
