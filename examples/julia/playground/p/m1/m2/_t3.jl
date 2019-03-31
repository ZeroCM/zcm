import ZCM

export t3
mutable struct t3 <: ZCM.AbstractZcmType
    x::ZCM.AbstractZcmType # p.t5

    function t3()

        self = new()
        self.x = basemodule.t5()

        return self
    end
end
