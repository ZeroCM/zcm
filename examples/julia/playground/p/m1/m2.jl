module m2
@static if VERSION < v"1.0.0-"
    parentmodule = module_parent
    pushfirst! = unshift!
    popfirst! = shift!
    basemodule = parentmodule(current_module()).basemodule
else
    basemodule = parentmodule(@__MODULE__).basemodule
end

__modulepath = joinpath(dirname(@__FILE__), "m2")

try
    # Submodules

    # Types
    include(joinpath(__modulepath, "_t3.jl"))
finally
end

end
