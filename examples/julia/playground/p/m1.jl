module m1
@static if VERSION < v"1.0.0-"
    parentmodule = module_parent
    pushfirst! = unshift!
    popfirst! = shift!
    basemodule = parentmodule(current_module()).basemodule
else
    basemodule = parentmodule(@__MODULE__).basemodule
end

__modulepath = joinpath(dirname(@__FILE__), "m1")


try
    # Submodules
    include(joinpath(__modulepath, "m2.jl"))

    # Types
    include(joinpath(__modulepath, "_t1.jl"))
    include(joinpath(__modulepath, "_t2.jl"))
finally
end

end
