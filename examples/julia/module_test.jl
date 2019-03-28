@static if VERSION < v"1.0.0-"
    pushfirst! = unshift!
end
pushfirst!(LOAD_PATH, "../build/types")

module Test

import _example_t

end

import _example2_t

ret = 0

if Test._example_t.__basemodule != Test
    println("Error: base module is not working for types imported within modules")
    ret = 1
end

if _example2_t.__basemodule != Main
    println("Error: base module is not working types imported from Main")
    ret = 1
end

if ret == 0
    println("Success")
end

exit(ret)
