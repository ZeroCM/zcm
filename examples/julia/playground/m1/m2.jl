module m2
@static if VERSION < v"1.0.0-"
    parentmodule = module_parent
    pushfirst! = unshift!
    popfirst! = shift!
end

__modulepath = joinpath(dirname(@__FILE__), "m2")

basemodule = parentmodule(m2).basemodule

function __init__()
    Base.eval(basemodule, Meta.parse("import _t5"))
end

try
    # Submodules

    # Types
    include(joinpath(__modulepath, "_t3.jl"))
finally
end

end
