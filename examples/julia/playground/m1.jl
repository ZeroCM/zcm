module m1
@static if VERSION < v"1.0.0-"
    parentmodule = module_parent
    pushfirst! = unshift!
    popfirst! = shift!
end

__modulepath = joinpath(dirname(@__FILE__), "m1")

basemodule = parentmodule(@__MODULE__)
if (basemodule == @__MODULE__)
    basemodule = Main
end

try
    # Submodules
    include(joinpath(__modulepath, "m2.jl"))

    # Types
    include(joinpath(__modulepath, "_t1.jl"))
    include(joinpath(__modulepath, "_t2.jl"))
finally
end

end
