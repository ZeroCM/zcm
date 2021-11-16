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
    @assert (msg.field22 == msg.FIELD22_TEST) "Bad decode of field 22"
    @assert (msg.field23 == msg.FIELD23_TEST) "Bad decode of field 23"
    @assert (msg.field24 == msg.FIELD24_TEST) "Bad decode of field 24"
    @assert (msg.field25 == 3) "Bad decode of field 25"
    @assert (msg.field26 == 255) "Bad decode of field 26"
    @assert (msg.field27 == 3) "Bad decode of field 27"
    @assert (msg.field28 == 0x7f) "Bad decode of field 28"
    @assert (msg.field29 == 3) "Bad decode of field 29"
    @assert (msg.field30 == 0x7fff) "Bad decode of field 30"
    @assert (msg.field31 == 0xf) "Bad decode of field 31"
    @assert (msg.field32 == 0x7fffffff) "Bad decode of field 32"
    @assert (msg.field33 == 0xf) "Bad decode of field 33"
    @assert (msg.field34 == 0x7fffffffffffffff) "Bad decode of field 34"
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
b.field22 = b.FIELD22_TEST;
b.field23 = b.FIELD23_TEST;
b.field24 = b.FIELD24_TEST;
b.field25 = 0xff;
b.field26 = 0xff;
b.field27 = 0x7f;
b.field28 = 0x7f;
b.field29 = 0x7fff;
b.field30 = 0x7fff;
b.field31 = 0x7fffffff;
b.field32 = 0x7fffffff;
b.field33 = 0x7fffffffffffffff;
b.field34 = 0x7fffffffffffffff;

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

@assert (b.SIGN_TEST_28 == 0x7f) "SIGN_TEST_28 does not have the correct value"
@assert (b.SIGN_TEST_29 == 0xff) "SIGN_TEST_29 does not have the correct value"
@assert (b.SIGN_TEST_30 == 0x7f) "SIGN_TEST_30 does not have the correct value"
@assert (b.SIGN_TEST_31 == -1) "SIGN_TEST_31 does not have the correct value"
@assert (b.SIGN_TEST_32 == 127) "SIGN_TEST_32 does not have the correct value"
@assert (b.SIGN_TEST_33 == -1) "SIGN_TEST_33 does not have the correct value"
@assert (b.SIGN_TEST_34 == 0x7fff) "SIGN_TEST_34 does not have the correct value"
@assert (b.SIGN_TEST_35 == -1) "SIGN_TEST_35 does not have the correct value"
@assert (b.SIGN_TEST_36 == 32767) "SIGN_TEST_36 does not have the correct value"
@assert (b.SIGN_TEST_37 == -1) "SIGN_TEST_37 does not have the correct value"
@assert (b.SIGN_TEST_38 == 0x7fffffff) "SIGN_TEST_38 does not have the correct value"
@assert (b.SIGN_TEST_39 == -1) "SIGN_TEST_39 does not have the correct value"
@assert (b.SIGN_TEST_40 == 2147483647) "SIGN_TEST_40 does not have the correct value"
@assert (b.SIGN_TEST_41 == -1) "SIGN_TEST_41 does not have the correct value"
@assert (b.SIGN_TEST_42 == 0x7fffffffffffffff) "SIGN_TEST_42 does not have the correct value"
@assert (b.SIGN_TEST_43 == -1) "SIGN_TEST_43 does not have the correct value"
@assert (b.SIGN_TEST_44 == 9223372036854775807) "SIGN_TEST_44 does not have the correct value"
@assert (b.SIGN_TEST_45 == -1) "SIGN_TEST_45 does not have the correct value"

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
