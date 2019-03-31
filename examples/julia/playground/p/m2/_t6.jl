import ZCM

export t6
mutable struct t6 <: ZCM.AbstractZcmType
    x::Int64

    function t6()

        self = new()
        self.x = 0

        return self
    end
end
