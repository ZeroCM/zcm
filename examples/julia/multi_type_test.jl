@static if VERSION < v"1.0.0-"
    pushfirst! = unshift!
end
pushfirst!(LOAD_PATH, "../build/types")

using ZCM
using _example_t
using _example2_t

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
