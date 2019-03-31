module p

__modulepath = joinpath(dirname(@__FILE__), "p")

basemodule = parentmodule(p)

try
    # Submodules
    include(joinpath(__modulepath, "m1.jl"))
    include(joinpath(__modulepath, "m2.jl"))

    # Types
    include(joinpath(__modulepath, "_t5.jl"))
finally
end

end

