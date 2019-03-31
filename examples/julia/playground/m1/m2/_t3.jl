import ZCM

export t3
mutable struct t3 <: ZCM.AbstractZcmType
    x::ZCM.AbstractZcmType # t5

    function t3()

        self = new()
        self.x = basemodule._t5.t5()

        return self
    end
end
