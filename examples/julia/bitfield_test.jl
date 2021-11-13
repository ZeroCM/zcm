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
    @assert (msg.field11 == 3) "Bad decode of field11"
    for i = 1:size(msg.field12, 1)
        for j = 1:size(msg.field12, 2)
            for k = 1:size(msg.field12, 3)
                for l = 1:size(msg.field12, 4)
                    @assert (msg.field12[i, j, k, l] == UInt8(k + l)) "Bad decode of field12"
                end
            end
        end
    end
    @assert (msg.field15 == -60) "Bad decode of field 15"
    @assert (msg.field16 == 2) "Bad decode of field 16"
    @assert (msg.field18 == 15) "Bad decode of field 18"
    @assert (msg.field19 == 68) "Bad decode of field 19"
    @assert (msg.field20 == 2) "Bad decode of field 20"
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
b.field11 = 3
for i = 1:size(b.field12, 1)
    for j = 1:size(b.field12, 2)
        for k = 1:size(b.field12, 3)
            for l = 1:size(b.field12, 4)
                b.field12[i, j, k, l] = UInt8(k + l)
            end
        end
    end
end
b.field15 = 0b1000100
b.field16 = 0b0000010
b.field18 = 0xff
b.field19 = 0b1000100
b.field20 = 0b0000010

@assert (b.FIELD22_TEST == 255) "FIELD22_TEST does not have the correct value"
@assert (b.FIELD23_TEST ==   3) "FIELD23_TEST does not have the correct value"
@assert (b.FIELD24_TEST ==   7) "FIELD24_TEST does not have the correct value"

@assert (b.SIGN_TEST_0  == 0x0f) "SIGN_TEST_0 does not have the correct value"
@assert (b.SIGN_TEST_1  ==  -16) "SIGN_TEST_1 does not have the correct value"
@assert (b.SIGN_TEST_2  == 0x7f) "SIGN_TEST_2 does not have the correct value"
@assert (b.SIGN_TEST_3  == -128) "SIGN_TEST_3 does not have the correct value"

@assert (b.SIGN_TEST_4  == 0x1fff) "SIGN_TEST_4 does not have the correct value"
@assert (b.SIGN_TEST_5  ==  -8192) "SIGN_TEST_5 does not have the correct value"
@assert (b.SIGN_TEST_6  == 0x7fff) "SIGN_TEST_6 does not have the correct value"
@assert (b.SIGN_TEST_7  == -32768) "SIGN_TEST_7 does not have the correct value"

@assert (b.SIGN_TEST_8  ==  0x01ffffff) "SIGN_TEST_8 does not have the correct value"
@assert (b.SIGN_TEST_9  ==   -33554432) "SIGN_TEST_9 does not have the correct value"
@assert (b.SIGN_TEST_10 ==  0x7fffffff) "SIGN_TEST_10 does not have the correct value"
@assert (b.SIGN_TEST_11 == -2147483648) "SIGN_TEST_11 does not have the correct value"

@assert (b.SIGN_TEST_12 == -1) "SIGN_TEST_12 does not have the correct value"
@assert (b.SIGN_TEST_13 == 72057594037927935) "SIGN_TEST_13 does not have the correct value"
@assert (b.SIGN_TEST_14 == -72057594037927936) "SIGN_TEST_14 does not have the correct value"
@assert (b.SIGN_TEST_15 == 9223372036854775807) "SIGN_TEST_15 does not have the correct value"
@assert (b.SIGN_TEST_16 == -9223372036854775808) "SIGN_TEST_16 does not have the correct value"

@assert (b.SIGN_TEST_17 == 7) "SIGN_TEST_17 does not have the correct value"
@assert (b.SIGN_TEST_18 == 0x7f) "SIGN_TEST_18 does not have the correct value"
@assert (b.SIGN_TEST_19 == 7) "SIGN_TEST_19 does not have the correct value"
@assert (b.SIGN_TEST_20 == 0x7f) "SIGN_TEST_20 does not have the correct value"
@assert (b.SIGN_TEST_21 == 7) "SIGN_TEST_21 does not have the correct value"
@assert (b.SIGN_TEST_22 == 0x7fff) "SIGN_TEST_22 does not have the correct value"
@assert (b.SIGN_TEST_23 == 7) "SIGN_TEST_23 does not have the correct value"
@assert (b.SIGN_TEST_24 == 0x7fffffff) "SIGN_TEST_24 does not have the correct value"
@assert (b.SIGN_TEST_25 == 1) "SIGN_TEST_25 does not have the correct value"
@assert (b.SIGN_TEST_26 == 7) "SIGN_TEST_26 does not have the correct value"
@assert (b.SIGN_TEST_27 == 9223372036854775807) "SIGN_TEST_27 does not have the correct value"

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
