using ZCM

import juliazcm.types.test_package: packaged_t
import juliazcm.types: example_t

function prepMsg!(m::packaged_t, tf::Bool)
    m.packaged = tf;
    m.a.packaged = tf;
    m.a.e.timestamp = (tf ? 1 : 0);
    m.a.e.p.packaged = tf;
end

function checkMsg(m::packaged_t, tf::Bool)
    @assert (m.packaged       == tf)           "Received msg with incorrect packaged flag"
    @assert (m.a.packaged     == tf)           "Received msg with incorrect a.packaged flag"
    @assert (m.a.e.timestamp  == (tf ? 1 : 0)) "Received msg with incorrect a.e.timestamp"
    @assert (m.a.e.p.packaged == tf)           "Received msg with incorrect a.e.p.packaged flag"
end

numReceived = 0
function handler(rbuf, channel::String, msg::packaged_t)
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel)")
    global numReceived
    checkMsg(msg, (numReceived % 2) == 0)
    numReceived += 1
end

zcm = Zcm("inproc")
if (!good(zcm))
    println("Unable to initialize zcm");
    exit()
end

sub = subscribe(zcm, "EXAMPLE", handler, packaged_t)

msg = packaged_t()

start(zcm)

prepMsg!(msg, true);
publish(zcm, "EXAMPLE", msg)
prepMsg!(msg, false);
publish(zcm, "EXAMPLE", msg)
prepMsg!(msg, true);
publish(zcm, "EXAMPLE", msg)

# This *should* assert
#=
msg.a = example_t()
publish(zcm, "EXAMPLE", msg)
# =#

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)

@assert (numReceived == 3) "Didn't receive proper number of messages"
println("Success!")
