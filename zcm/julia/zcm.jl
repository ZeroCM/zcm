module ZCM


# C ptr types
module Native

type Zcm
end

#type RecvBuf
#end

type Sub
end

#type Eventlog
#end
#
#type EventlogEvent
#end

end


# TODO: Just testing an example, this is an example of how you can wrap the user's function ptr
#       in a way that will be passable to cfunction()

type CompareUsr
    lessthan::Function;
    usr::Any;
    CompareUsr(lessthan::Function, usr) = new(lessthan, usr);
end

function qsort!_compare{T}(a_::Ptr{T}, b_::Ptr{T}, compareUsr_::Ptr{Void})
    a = unsafe_load(a_)
    b = unsafe_load(b_)
    compareUsr = unsafe_pointer_to_objref(compareUsr_)::CompareUsr;
    ret::Cint = compareUsr.lessthan(a, b, compareUsr.usr) ? -1 : +1;
	return ret;
end

function qsort!{T}(A::Vector{T}, lessthan::Function, usr::Any = 0)
    compareUsr = CompareUsr(lessthan, usr);
    compare_c = cfunction(qsort!_compare, Cint, (Ptr{T}, Ptr{T}, Ptr{Void}))
    ccall(("qsort_r", "libc"), Void, (Ptr{T}, Csize_t, Csize_t, Ptr{Void}, Any),
          A, length(A), sizeof(T), compare_c, compareUsr)
    return A
end

# do this: ZCM.qsort!(data, function (a, b, usr) usr <= 0 ? a < b : a > b; end, 0)



# Exported Objects and Methods
type RecvBuf
    recv_utime::Int64;
    zcm       ::Ptr{Native.Zcm};
    data      ::Ptr{UInt8};
    data_size ::UInt32;
end
export RecvBuf;

# Possible leads on being able to do this with start() and stop()
# examples:
#     https://github.com/JuliaGPU/OpenCL.jl/blob/716add3c4315727ff611cc3ac1b6b086be909a95/src/event.jl#L97-L140
#     https://github.com/JuliaInterop/ZMQ.jl/blob/57786eaac5641bd56ca52b3806e4dd766c892409/src/ZMQ.jl#L329-L358
# discussions:
#     https://github.com/JuliaLang/julia/issues/17573
#     https://groups.google.com/forum/#!msg/julia-users/ztN-UgS9N8c/bcjLBZ8O5w4J
#     https://github.com/JuliaLang/julia/pull/5657

type __zcm_handler
    handler::Any; # Can be either a Function or a Functor, TODO add prototype
    usr::Any;
    function __zcm_handler(handler, usr)
        new(handler, usr);
    end
end

function __zcm_handler_wrapper(rbuf::Ptr{RecvBuf}, channel::Cstring, usr_::Ptr{Void})
    println("hello from wrapper");
    usr = unsafe_pointer_to_objref(usr_)::__zcm_handler;
    usr.handler(unsafe_load(rbuf), unsafe_string(channel), usr.usr);
    return nothing;
end

type Sub
    sub::Ptr{Native.Sub};

    # References to protect our callbacks from Julia's garbage collector
    handler_jl::__zcm_handler;
    handler_c ::Ptr{Void};

    Sub(sub::Ptr{Native.Sub}, handler_jl::__zcm_handler, handler_c::Ptr{Void}) =
        new(sub, handler_jl, handler_c);
end

type Zcm
    zcm::Ptr{Native.Zcm};

    errno       ::Function; # () ::Int32
    strerror    ::Function; # () ::String

    # TODO refine these comments
    subscribeRaw::Function; #  (channel::String, handler::Handler, usr::Any) ::Ptr{Native.Sub}
    subscribe   ::Function; #  (channel::String, handler::Handler, usr::Any) ::Ptr{Native.Sub}
    unsubscribe ::Function; #  (sub::Ptr{Native.Sub}) ::Int32
    publishRaw  ::Function; #  (channel::String, data::Array{Uint8}, datalen::Uint32) ::Int32
    publish     ::Function; #  (channel::String, msg::Msg) ::Int32
    flush       ::Function; #  () ::Void

    run         ::Function; #  () ::Void
    start       ::Function; #  () ::Void
    stop        ::Function; #  () ::Void
    handle      ::Function; #  () ::Void


    # http://docs.julialang.org/en/stable/manual/calling-c-and-fortran-code/
    # http://julialang.org/blog/2013/05/callback


    function Zcm(url::AbstractString)
        println("Creating zcm with url : ", url);
        instance = new();
        instance.zcm = ccall(("zcm_create", "libzcm"), Ptr{Native.Zcm}, (Cstring,), url);

        # user can force cleanup of their instance by calling `finalize(zcm)`
        finalizer(instance, function(zcm::Zcm)
                                if (zcm.zcm != C_NULL)
                                    println("Destroying zcm instance");
                                    ccall(("zcm_destroy", "libzcm"), Void,
                                          (Ptr{Native.Zcm},), zcm.zcm);
                                    zcm.zcm = C_NULL;
                                end
                            end);

        instance.errno = function()
            return ccall(("zcm_errno", "libzcm"), Cint, (Ptr{Native.Zcm},), instance.zcm);
        end

        instance.strerror = function()
            val =  ccall(("zcm_strerror", "libzcm"), Cstring, (Ptr{Native.Zcm},), instance.zcm);
            if (val == C_NULL)
                return "unable to get strerror";
            else
                return unsafe_string(val);
            end
        end

        # TODO: get this into docs:
        # Note to self: handler could be either a function or a functor, so long as it has
        #               handler(rbuf::RecvBuf, channel::String, usr) defined
        instance.subscribeRaw = function(channel::AbstractString, handler, usr)

            handler_jl = __zcm_handler(handler, usr);
            handler_c  = cfunction(__zcm_handler_wrapper, Void,
                                   (Ptr{RecvBuf}, Cstring, Ptr{Void}));

            return Sub(ccall(("zcm_subscribe", "libzcm"), Ptr{Native.Sub},
                             (Ptr{Native.Zcm}, Cstring, Ptr{Void}, Any),
                             instance.zcm, channel, handler_c, handler_jl),
                       handler_jl, handler_c);
        end

        # TODO: get this into docs:
        # Note to self: handler could be either a function or a functor, so long as it has
        #               handler(rbuf::RecvBuf, channel::String, msg::MsgType, usr) defined
        instance.subscribe = function(channel::AbstractString, MsgType::DataType, handler, usr)
            return instance.subscribeRaw(channel,
                                         function (rbuf::RecvBuf, channel::AbstractString, usr)
                                             msg = MsgType();
                                             # TODO: think we can use `unsafe_wrap` in zcmgen code
                                             #       to turn data ptr into an array
                                             msg.decode(rbuf.data, rbuf.data_size);
                                             handler(rbuf, channel, msg, usr);
                                         end, usr);
        end

        instance.unsubscribe = function(sub::Sub)
            return ccall(("zcm_unsubscribe", "libzcm"), Cint,
                         (Ptr{Native.Zcm}, Ptr{Native.Sub}), instance.zcm, sub.sub);
        end

        instance.publishRaw = function(channel::AbstractString, data::Array{UInt8}, datalen::UInt32)
            return ccall(("zcm_publish", "libzcm"), Cint,
                         (Ptr{Native.Zcm}, Cstring, Ptr{Void}, UInt32),
                         instance.zcm, channel, data, datalen);
        end

        # TODO: force msg to be derived from our zcm msg basetype
        instance.publish = function(channel::AbstractString, msg)
            return instance.publishRaw(channel, msg.encode(), msg.encodeLen());
        end

        instance.flush = function()
            return ccall(("zcm_flush", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        instance.run = function()
            throw("Not yet implemented: Breaks Julia as there is no way to exit this function");
            ccall(("zcm_run", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        instance.start = function()
            throw("Not yet implemented: parallelization issues with Julia and C callbacks");
            ccall(("zcm_start", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        instance.stop = function()
            throw("Not yet implemented: parallelization issues with Julia and C callbacks");
            ccall(("zcm_stop", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        instance.handle = function()
            ccall(("zcm_handle", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        return instance;
    end
end
export Zcm;

#type Eventlog
#end
#export Eventlog;
#
#type EventlogEvent
#end
#export EventlogEvent;


end

