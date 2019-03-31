module m2
@static if VERSION < v"1.0.0-"
    parentmodule = module_parent
    pushfirst! = unshift!
    popfirst! = shift!
    basemodule = parentmodule(current_module())
else
    basemodule = parentmodule(parentmodule(@__MODULE__))
end

__modulepath = joinpath(dirname(@__FILE__), "m2")

try
    # Submodules

    # Types
    include(joinpath(__modulepath, "_t4.jl"))
    include(joinpath(__modulepath, "_t6.jl"))
finally
end

end
