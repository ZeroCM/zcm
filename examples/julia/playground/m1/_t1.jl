import ZCM

export t1
mutable struct t1 <: ZCM.AbstractZcmType
    x::ZCM.AbstractZcmType # m1.t2
    # y::ZCM.AbstractZcmType # m2.t4

    function t1()

        self = new()
        self.x = basemodule.m1.t2()
        # self.y = basemodule.m2.t2()

        return self
    end
end
