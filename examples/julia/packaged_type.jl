push!(LOAD_PATH, "../build/types")

using ZCM
# RRR: I had to manually ensure that the order of the imports in build/types/test_package.jl
#      followed the dependency tree to prevent this test from breaking.
#      correct order is :
#        import _test_package_example3_t
#        import _test_package_packaged2_t
#        import _test_package_packaged_t
#
import test_package: packaged_t

numReceived = 0
function handler(rbuf, channel::String, msg::packaged_t)
    println("Received message on channel: ", channel)
    global numReceived
    @assert (((numReceived % 2) == 0) == msg.packaged) "Received message with incorrect packaged flag"
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

sleep(0.5)
stop(zcm)

unsubscribe(zcm, sub)

@assert (numReceived == 3) "Didn't receive proper number of messages"
println("Success!")
