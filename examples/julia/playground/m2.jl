module m2
@static if VERSION < v"1.0.0-"
    parentmodule = module_parent
    pushfirst! = unshift!
    popfirst! = shift!
end

__modulepath = joinpath(dirname(@__FILE__), "m2")

basemodule = parentmodule(@__MODULE__)
if (basemodule == @__MODULE__)
    basemodule = Main
end

function __init__()
    Base.eval(basemodule, Meta.parse("import m1"))
end

try
    # Submodules

    # Types
    include(joinpath(__modulepath, "_t4.jl"))
finally
end

end
