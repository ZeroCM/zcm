module p
@static if VERSION < v"1.0.0-"
    parentmodule = module_parent
    pushfirst! = unshift!
    popfirst! = shift!
    basemodule = current_module()
else
    basemodule = @__MODULE__
end

__modulepath = joinpath(dirname(@__FILE__), "p")

try
    # Submodules
    include(joinpath(__modulepath, "m1.jl"))
    include(joinpath(__modulepath, "m2.jl"))

    # Types
    include(joinpath(__modulepath, "_t5.jl"))
finally
end

end

