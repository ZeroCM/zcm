push!(LOAD_PATH, "../build/types")
import test_package: packaged_t, packaged_recursive_t
import other_package: other_recursive_t

using ZCM;

numReceived = 0
function handler(rbuf, channel::String, msg::packaged_t)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (((numReceived % 2) == 0) == msg.packaged) "Received message with incorrect packaged flag"
    numReceived = numReceived + 1
end

msg = other_recursive_t()
msg.child = packaged_recursive_t()
msg.child.child = packaged_t()
@assert msg.child.child.packaged == false

zcm = Zcm("inproc")
if (!good(zcm))
    println("Unable to initialize zcm");
    exit()
end

sub = subscribe(zcm, "EXAMPLE", handler, packaged_t)

msg = packaged_t()

start(zcm)

msg.packaged = true;
publish(zcm, "EXAMPLE", msg)
msg.packaged = false;
publish(zcm, "EXAMPLE", msg)
msg.packaged = true;
publish(zcm, "EXAMPLE", msg)

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)

@assert (numReceived == 3) "Didn't receive proper number of messages"
println("Success!")
