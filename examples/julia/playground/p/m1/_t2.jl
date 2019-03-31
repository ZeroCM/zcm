import ZCM

export t2
mutable struct t2 <: ZCM.AbstractZcmType
    x::ZCM.AbstractZcmType # p.m1.m2.t3

    function t2()

        self = new()
        self.x = basemodule.p.m1.m2.t3()

        return self
    end
end
