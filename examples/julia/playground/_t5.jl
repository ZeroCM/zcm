module _t5
@static if VERSION < v"1.0.0-"
    parentmodule = module_parent
    pushfirst! = unshift!
    popfirst! = shift!
end

basemodule = parentmodule(@__MODULE__)
if (basemodule == @__MODULE__)
    basemodule = Main
end

import ZCM

export t5
mutable struct t5 <: ZCM.AbstractZcmType
    x::Int64

    function t5()

        self = new()
        self.x = 0

        return self
    end
end

end
