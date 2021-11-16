using ZCM
using juliazcm.types: example_t

zcm = Zcm("inproc")
if (!good(zcm))
    error("Unable to initialize zcm");
end

# This handler expects an additional argument `received_timestamps`. To
# ensure that argument is provided, we just have to pass it as an
# additional argument to `subscribe()`
function handler(rbuf, channel::String, msg::example_t, received_timestamps::Vector)
    ccall(:jl_, Nothing, (Any,), "Received message on channel $(channel)")
    @assert msg.timestamp == length(received_timestamps) "Received message with incorrect timestamp"
    push!(received_timestamps, msg.timestamp)
end

received_timestamps = Int[]
sub = subscribe(zcm, "EXAMPLE", handler, example_t, received_timestamps)

msg = example_t()

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

@assert received_timestamps == collect(0:5) "Didn't receive proper number of messages"

println("Success!")
