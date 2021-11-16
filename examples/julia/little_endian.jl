using ZCM
using juliazcm.types: little_endian_t

numReceived = 0
function handler(rbuf, channel::String, msg::little_endian_t)
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel)")
    global numReceived
    @assert (numReceived == msg.timestamp) "Received message with incorrect timestamp"
    numReceived = numReceived + 1
end

# a handler that receives the raw message bytes
function untyped_handler(rbuf, channel::String, msgdata::Vector{UInt8})
    ccall(:jl_, Nothing, (Any,), "Received raw message data on channel $(channel)")
    buf = IOBuffer(msgdata)
    @assert (ntoh(reinterpret(Int64, read(buf, 8))[1]) == ZCM.getHash(little_endian_t))
            "Hash should always be encoded as network format"
    decode(little_endian_t, msgdata)
end

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

sub = subscribe(zcm, "EXAMPLE", handler, little_endian_t)
sub2 = subscribe(zcm, "EXAMPLE", untyped_handler)

msg = little_endian_t()

@assert (msg.IS_LITTLE_ENDIAN == 1) "Type must be little endian"

start(zcm)

msg.timestamp = 0;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 1;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 2;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)

msg.timestamp = 3;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 4;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 5;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)

@assert (numReceived == 6) "Didn't receive proper number of messages"

println("Success!")
