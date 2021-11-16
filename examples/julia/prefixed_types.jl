using ZCM

import juliazcmtypespfx.example.zcmtypes.test_package: packaged_t
import juliazcmtypespfx.example.zcmtypes:               example_t

function prepPackaged!(m::packaged_t, tf::Bool)
    m.packaged = tf;
    m.a.packaged = tf;
    m.a.e.timestamp = (tf ? 1 : 0);
    m.a.e.p.packaged = tf;
end

function checkPackaged(m::packaged_t, tf::Bool)
    @assert (m.packaged       == tf)           "Received msg with incorrect packaged flag"
    @assert (m.a.packaged     == tf)           "Received msg with incorrect a.packaged flag"
    @assert (m.a.e.timestamp  == (tf ? 1 : 0)) "Received msg with incorrect a.e.timestamp"
    @assert (m.a.e.p.packaged == tf)           "Received msg with incorrect a.e.p.packaged flag"
end

numPackagedReceived = 0
function handlerPackaged(rbuf, channel::String, msg::packaged_t)
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel)")
    global numPackagedReceived
    checkPackaged(msg, (numPackagedReceived % 2) == 0)
    numPackagedReceived += 1
end

function prepExample!(m::example_t, t::Int)
    m.timestamp = t;
end

function checkExample(m::example_t, t::Int)
    @assert (m.timestamp == t) "Received msg with incorrect timestamp"
end

numExampleReceived = 0
function handlerExample(rbuf, channel::String, msg::example_t)
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel)")
    global numExampleReceived
    checkExample(msg, numExampleReceived)
    numExampleReceived += 1
end

zcm = Zcm("inproc")
if (!good(zcm))
    println("Unable to initialize zcm");
    exit()
end

subPackaged = subscribe(zcm, "PACKAGED", handlerPackaged, packaged_t)
subExample  = subscribe(zcm, "EXAMPLE",  handlerExample,   example_t)

p_t = packaged_t()
e_t = example_t()

start(zcm)

prepPackaged!(p_t, true);
publish(zcm, "PACKAGED", p_t)
prepExample!(e_t, 0);
publish(zcm, "EXAMPLE", e_t)

prepPackaged!(p_t, false);
publish(zcm, "PACKAGED", p_t)
prepExample!(e_t, 1);
publish(zcm, "EXAMPLE", e_t)

prepPackaged!(p_t, true);
publish(zcm, "PACKAGED", p_t)
prepExample!(e_t, 2);
publish(zcm, "EXAMPLE", e_t)

sleep(0.5)
stop(zcm)

unsubscribe(zcm, subPackaged)
unsubscribe(zcm, subExample)

@assert (numPackagedReceived == 3) "Didn't receive proper number of packaged_t messages"
@assert (numExampleReceived  == 3) "Didn't receive proper number of example_t  messages"
println("Success!")
