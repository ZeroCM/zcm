module _t5

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
