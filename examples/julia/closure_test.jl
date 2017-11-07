include("../build/types/example_t.jl")

using ZCM

# Demonstration of using a closure as the message handler. The `handler()` function
# closes over the received_timestamps variable, so it can append to that list
# any time a message is received. 
received_timestamps = Int[]

handler = function(channel::String, msg::example_t)
    println("Received message on channel: ", channel)
    @assert msg.timestamp == length(received_timestamps) "Received message with incorrect timestamp"
    push!(received_timestamps, msg.timestamp)
end

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

sub = subscribe(zcm, "EXAMPLE", example_t, handler)

msg = example_t()

start(zcm)

msg.timestamp = 0;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 1;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 2;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

start(zcm)

msg.timestamp = 3;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 4;
publish(zcm, "EXAMPLE", msg)
msg.timestamp = 5;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)

@assert received_timestamps == collect(0:5) "Didn't receive proper number of messages"
