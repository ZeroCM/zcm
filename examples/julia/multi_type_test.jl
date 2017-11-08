include("../build/types/example_t.jl");
include("../build/types/example2_t.jl");

using ZCM;

received_example_t = false
function handler1(rbuf, channel::String, msg::example_t)
    global received_example_t
    received_example_t = true
    println("Received example_t message on channel: ", channel)
end

received_example2_t = false
function handler2(rbuf, channel::String, msg::example2_t)
    global received_example2_t
    received_example2_t = true
    println("Received example2_t message on channel: ", channel)
end

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

sub1 = subscribe(zcm, "EXAMPLE1", handler1, example_t)
sub2 = subscribe(zcm, "EXAMPLE2", handler2, example2_t)

start(zcm)
publish(zcm, "EXAMPLE1", example_t())
publish(zcm, "EXAMPLE2", example2_t())
sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub1)
unsubscribe(zcm, sub2)

@assert received_example_t "Did not receive an example_t message"
@assert received_example2_t "Did not receive an example2_t message"

println("Success!")
