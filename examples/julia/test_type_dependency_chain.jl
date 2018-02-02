module DepChain

import ZCM

type _t1_base{t2 <: ZCM.AbstractZCMType} <: ZCM.AbstractZCMType
    x::t2
    function _t1_base()
        self = new()
        self.x = t2()
        return self
    end
end

type _t2_base{t3 <: ZCM.AbstractZCMType} <: ZCM.AbstractZCMType
    y::t3
    function _t2_base()
        self = new()
        self.y = t3()
        return self
    end
end

type _t3_base <: ZCM.AbstractZCMType
    z::Int
    function _t3_base()
        self = new()
        self.z = 0
        return self
    end
end

t1 = _t1_base{DepChain._t2_base{DepChain._t3_base}}
export t1

t2 = _t2_base{DepChain._t3_base}
export t2

t3 = _t3_base
export t3

# RRR: note this is only 0.5 syntax
abstract _t4 <: ZCM.AbstractZCMType
abstract _t5 <: ZCM.AbstractZCMType
abstract _t6 <: ZCM.AbstractZCMType

type t4 <: _t4
    x::_t5
    function t4()
        self = new()
        self.x = _createAbstractZCMType(_t5)
        return self
    end
end
# RRR: a base abstract creator would be put within ZCM.jl (like decode) and then we'd just extend it
function _createAbstractZCMType(::Type{_t4})
    return t4()
end
export t4

type t5 <: _t5
    y::_t6
    function t5()
        self = new()
        self.y = _createAbstractZCMType(_t6)
        return self
    end
end
function _createAbstractZCMType(::Type{_t5})
    return t5()
end
export t5

type t6 <: _t6
    z::Int
    function t6()
        self = new()
        self.z = 0
        return self
    end
end
function _createAbstractZCMType(::Type{_t6})
    return t6()
end
export t6


end
