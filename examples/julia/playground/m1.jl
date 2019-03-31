module m1

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
