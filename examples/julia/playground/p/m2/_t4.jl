import ZCM

export t4
mutable struct t4 <: ZCM.AbstractZcmType
    x::ZCM.AbstractZcmType # p.m1.t2

    function t4()

        self = new()
        self.x = basemodule.m1.t2()

        return self
    end
end
