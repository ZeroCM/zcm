import ZCM

export t1
mutable struct t1 <: ZCM.AbstractZcmType
    x::ZCM.AbstractZcmType # p.m1.t2
    y::ZCM.AbstractZcmType # p.m2.t6

    function t1()

        self = new()
        self.x = basemodule.m1.t2()
        self.y = basemodule.m2.t6()

        return self
    end
end
