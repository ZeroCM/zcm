module m2

__modulepath = joinpath(dirname(@__FILE__), "m2")

basemodule = parentmodule(parentmodule(parentmodule(m2)))

function __init__()
end

try
    # Submodules

    # Types
    include(joinpath(__modulepath, "_t3.jl"))
finally
end

end
