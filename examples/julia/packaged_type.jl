push!(LOAD_PATH, "../build/types")

using ZCM

import test_package: packaged_t
import _example_t: example_t

numReceived = 0
function handler(rbuf, channel::String, msg::packaged_t)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (((numReceived % 2) == 0) == msg.packaged) "Received msg with incorrect packaged flag"
    numReceived = numReceived + 1
end

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
