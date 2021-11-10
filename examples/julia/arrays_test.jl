using ZCM
using juliazcm.types: example_t,
                      arrays_t

numReceived = 0
function handler(rbuf, channel::String, msg::arrays_t)
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel)")
    global numReceived

    @assert (numReceived == msg.n) "Wrong data"

    for i=1:3
        @assert (msg.prim_onedim_static[i] == true) "Wrong data"
        @assert (msg.nonprim_onedim_static[i].timestamp == numReceived) "Wrong data"

        for j=1:3
            @assert (msg.prim_twodim_static_static[i,j] == i * j) "Wrong data"
            @assert (msg.nonprim_twodim_static_static[i,j].timestamp == numReceived) "Wrong data"
        end
    end

    for i=1:msg.n
        @assert (msg.prim_onedim_dynamic[i] == i) "Wrong data"
        @assert (msg.nonprim_onedim_dynamic[i].timestamp == numReceived) "Wrong data"

        for j=1:3
            @assert (msg.prim_twodim_static_dynamic[j,i]  == i * j) "Wrong data"
            @assert (msg.prim_twodim_dynamic_static[i,j]  == i * j) "Wrong data"
            @assert (msg.nonprim_twodim_static_dynamic[j,i].timestamp == numReceived) "Wrong data"
            @assert (msg.nonprim_twodim_dynamic_static[i,j].timestamp == numReceived) "Wrong data"
        end

        for j=1:msg.m
            @assert (msg.prim_twodim_dynamic_dynamic[j,i] == i * j) "Wrong data"
            @assert (msg.nonprim_twodim_dynamic_dynamic[j,i].timestamp == numReceived) "Wrong data"
        end
    end

    numReceived = numReceived + 1
end

# a handler that receives the raw message bytes
function untyped_handler(rbuf, channel::String, msgdata::Vector{UInt8})
    ccall(:jl_, Nothing, (Any,), "Received raw message data on channel $(channel)")
    decode(arrays_t, msgdata)
end

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm")
end

sub = subscribe(zcm, "EXAMPLE", handler, arrays_t)
sub2 = subscribe(zcm, "EXAMPLE", untyped_handler)

function populate!(msg::arrays_t, ex::example_t, num::Int64)

    ex.position = [ 1.0, 2.0, 3.0 ];
    ex.orientation = [ 1.0, 2.0, 3.0, 4.0 ];
    ex.num_ranges = 3;
    ex.ranges = [1, 2, 3];
    ex.name = "example";
    ex.enabled = true

    ex.timestamp = num
    msg.m = num
    msg.n = num

    msg.prim_onedim_static             = [ true  for i=1:3            ]
    msg.prim_onedim_dynamic            = [ i     for i=1:num          ]
    msg.prim_twodim_static_static      = [ (i*j) for i=1:3,   j=1:3   ]
    msg.prim_twodim_static_dynamic     = [ (i*j) for i=1:3,   j=1:num ]
    msg.prim_twodim_dynamic_static     = [ (i*j) for i=1:num, j=1:3   ]
    msg.prim_twodim_dynamic_dynamic    = [ (i*j) for i=1:num, j=1:num ]

    msg.nonprim_onedim_static          = [ ex    for i=1:3            ]
    msg.nonprim_onedim_dynamic         = [ ex    for i=1:num          ]
    msg.nonprim_twodim_static_static   = [ ex    for i=1:3,   j=1:3   ]
    msg.nonprim_twodim_static_dynamic  = [ ex    for i=1:3,   j=1:num ]
    msg.nonprim_twodim_dynamic_static  = [ ex    for i=1:num, j=1:3   ]
    msg.nonprim_twodim_dynamic_dynamic = [ ex    for i=1:num, j=1:num ]
end

ex  = example_t()
msg = arrays_t()

start(zcm)

populate!(msg, ex, 0);
publish(zcm, "EXAMPLE", msg)

populate!(msg, ex, 1);
publish(zcm, "EXAMPLE", msg)

populate!(msg, ex, 2);
publish(zcm, "EXAMPLE", msg)

sleep(0.5)

populate!(msg, ex, 3);
publish(zcm, "EXAMPLE", msg)
populate!(msg, ex, 4);
publish(zcm, "EXAMPLE", msg)
populate!(msg, ex, 5);
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)
unsubscribe(zcm, sub2)

@assert (numReceived == 6) "Didn't receive proper number of messages"

println("Success!")

#=
buf = encode(msg)
exSize = size(encode(ex), 1) - 8
for i=(1+8+1):exSize:size(buf,1)
    t = ntoh(reinterpret(Int64, buf[i:i+7])[1])
    println(t)
    exam = ZCM._decode_one(example_t, IOBuffer(buf[i:i+exSize-1]))
    println(exam)
end
=#
