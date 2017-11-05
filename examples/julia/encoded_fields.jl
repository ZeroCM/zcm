#!/usr/bin/python

include("/usr/local/share/julia/zcm.jl");
include("../build/types/encoded_t.jl");
include("../build/types/example_t.jl");

import ZCM;

# make a new zcm object and launch the handle thread
zcm = ZCM.Zcm("inproc")
if (!zcm.good())
    println("Unable to initialize zcm");
    exit()
end

# declare a new msg and populate it
msg = example_t()
msg.timestamp = 10

encoded = encoded_t()
encoded.msg = msg.encode()
encoded.n   = length(encoded.msg)

decoded = example_t()
decoded.decode(encoded.msg)
@assert (decoded.timestamp == msg.timestamp) "Encode/decode mismatch"

println("Success")
