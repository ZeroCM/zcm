module ZCM

# RRR / Note: Julia requires that the memory layout of the C structs is consistent
#             between their definitions in zcm headers and this file

# C ptr types for types that we don't need the internals of
module Native

type Zcm
end

type Sub
end

type EventLog
end

type EventLogEvent
    eventnum  ::Int64;
    timestamp ::Int64;
    chanlen   ::Int32;
    datalen   ::Int32;
    channel   ::Ptr{UInt8};
    data      ::Ptr{UInt8};
end

type UvSub
end

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



# RRR: probably want to move the nativer version of this into the Native section and then
#      make a julia-usable version (that casts the data Ptr to an array). See LogEvent
# RRR: make variable naming consistent
# RRR: improve orgainzation and make headers more visible
# Exported Objects and Methods
type RecvBuf
    recv_utime::Int64;
    zcm       ::Ptr{Native.Zcm};
    data      ::Ptr{UInt8};
    data_size ::UInt32;
    RecvBuf() = new();
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
    handler::Any;
    usr::Any;

    # handler must have the function or functor prototype :
    #   (rbuf::RecvBuf, channel::String, msg::MsgType, usr)
    __zcm_handler(handler, usr) = new(handler, usr);
end

function __zcm_handler_wrapper(rbuf::Ptr{RecvBuf}, channel::Cstring, usr_::Ptr{Void})
    println("hello from wrapper");
    # usr = unsafe_pointer_to_objref(usr_)::__zcm_handler;
    # usr.handler(unsafe_load(rbuf), unsafe_string(channel), usr.usr);
    return nothing;
end

type Sub
    sub::Ptr{Native.Sub};

    # References to protect our callbacks from Julia's garbage collector
    handler_jl::__zcm_handler;
    handler_c ::Ptr{Void};
    uv_handler::Ptr{Void};
    uv_wrapper::Ptr{Native.UvSub};

    Sub(sub::Ptr{Native.Sub}, handler_jl::__zcm_handler, handler_c::Ptr{Void},
        uv_handler::Ptr{Void}, uv_wrapper::Ptr{Native.UvSub}) =
        new(sub, handler_jl, handler_c, uv_handler, uv_wrapper);
end

type Zcm
    zcm::Ptr{Native.Zcm};

    # RRR: make a good() call?
    good        ::Function  # () ::Bool
    errno       ::Function; # () ::Int32
    strerror    ::Function; # () ::String

    # TODO refine these comments (handler and msg not well defined)
    subscribeRaw::Function; # (channel::String, handler::Handler, usr::Any) ::Ptr{Native.Sub}
    subscribe   ::Function; # (channel::String, handler::Handler, usr::Any) ::Ptr{Native.Sub}
    unsubscribe ::Function; # (sub::Ptr{Native.Sub}) ::Int32
    publishRaw  ::Function; # (channel::String, data::Array{Uint8}, datalen::Uint32) ::Int32
    publish     ::Function; # (channel::String, msg::Msg) ::Int32
    flush       ::Function; # () ::Void

    run         ::Function; # () ::Void
    start       ::Function; # () ::Void
    stop        ::Function; # () ::Void
    handle      ::Function; # () ::Void

    # http://docs.julialang.org/en/stable/manual/calling-c-and-fortran-code/
    # http://julialang.org/blog/2013/05/callback

    # RRR: make prints throughout this file optional. Note that printing in a finalizer is not
    #      ok because it can cause errors on exit if stdout has already been cleaned up
    function Zcm(url::AbstractString)
        println("Creating zcm with url : ", url);
        instance = new();
        instance.zcm = ccall(("zcm_create", "libzcm"), Ptr{Native.Zcm}, (Cstring,), url);

        # user can force cleanup of their instance by calling `finalize(zcm)`
        finalizer(instance, function(zcm::Zcm)
                                if (zcm.zcm != C_NULL)
                                    ccall(("zcm_destroy", "libzcm"), Void,
                                          (Ptr{Native.Zcm},), zcm.zcm);
                                    zcm.zcm = C_NULL;
                                end
                            end);

        instance.good = function()
            return (instance.zcm != C_NULL) && (instance.errno() == 0);
        end

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
                                   (Ptr{RecvBuf}, Cstring, Ptr{Void},));
            uv_wrapper = ccall(("uv_zcm_msg_handler_create", "libzcmuv"), Ptr{Native.UvSub},
                               (Ptr{Void}, Any,), handler_c, handler_jl);
            uv_handler = cglobal(("uv_zcm_msg_handler_trigger", "libzcmuv"));

            return Sub(ccall(("zcm_subscribe", "libzcm"), Ptr{Native.Sub},
                             (Ptr{Native.Zcm}, Cstring, Ptr{Void}, Ptr{Native.UvSub},),
                             instance.zcm, channel, uv_handler, uv_wrapper),
                       handler_jl, handler_c, uv_handler, uv_wrapper);
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
            ret = ccall(("zcm_unsubscribe", "libzcm"), Cint,
                         (Ptr{Native.Zcm}, Ptr{Native.Sub}), instance.zcm, sub.sub);
            ccall(("uv_zcm_msg_handler_destroy", "libzcmuv"), Void,
                  (Ptr{Native.UvSub},), sub.uv_wrapper);
            return ret;
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
            ccall(("zcm_run", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        instance.start = function()
            ccall(("zcm_start", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        instance.stop = function()
            ccall(("zcm_stop", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        instance.handle = function()
            ccall(("zcm_handle", "libzcm"), Void, (Ptr{Native.Zcm},), instance.zcm);
        end

        return instance;
    end
end
export Zcm;

# RRR: go over the signedness of all these types from within zcm ... some of them are dumb
type LogEvent
    event     ::Ptr{Native.EventLogEvent};

    eventnum  ::Int64;
    timestamp ::Int64;
    channel   ::String;
    data      ::Array{UInt8};

    function LogEvent(event::Ptr{Native.EventLogEvent})
        instance  = new();
        instance.event = event;

        if (event != C_NULL)
            loadedEvent = unsafe_load(event);

            instance.eventnum  = loadedEvent.eventnum;
            instance.timestamp = loadedEvent.timestamp;
            if (loadedEvent.channel != C_NULL)
                instance.channel = unsafe_string(loadedEvent.channel, loadedEvent.chanlen);
            else
                instance.channel = "Channel was NULL"
            end
            if (loadedEvent.data != C_NULL)
                instance.data    = unsafe_wrap(Array, loadedEvent.data, loadedEvent.datalen);
            else
                instance.data    = [0x1, 0x2, 0x3, 0x4];
            end
        end

        # user can force cleanup of their instance by calling `finalize(zcm)`
        finalizer(instance, function(event::LogEvent)
                                if (event.event != C_NULL)
                                    ccall(("zcm_eventlog_free_event", "libzcm"), Void,
                                          (Ptr{Native.EventLogEvent},), event.event);
                                    event.event = C_NULL;
                                end
                            end);

        return instance;
    end
end

export LogEvent;
type LogFile
    eventLog::Ptr{Native.EventLog};

    good           ::Function; # () ::Bool

    seekToTimestamp   ::Function; # (timestamp::Int64) ::Int32

    readNextEvent     ::Function; # () ::LogEvent
    readPrevEvent     ::Function; # () ::LogEvent
    readEventAtOffset ::Function; # (offset::Int64) ::LogEvent
    writeEvent        ::Function; # (event::LogEvent) ::Int32

    function LogFile(path::AbstractString, mode::AbstractString)
        println("Creating zcm eventlog from path : ", path, " with mode ", mode);
        instance = new();
        instance.eventLog = ccall(("zcm_eventlog_create", "libzcm"), Ptr{Native.EventLog},
                                  (Cstring, Cstring), path, mode);

        # user can force cleanup of their instance by calling `finalize(zcm)`
        finalizer(instance, function(log::LogFile)
                                if (log.eventLog != C_NULL)
                                    ccall(("zcm_eventlog_destroy", "libzcm"), Void,
                                          (Ptr{Native.EventLog},), log.eventLog);
                                    log.eventLog = C_NULL;
                                end
                            end);

        instance.good = function()
            return instance.eventLog != C_NULL;
        end

        instance.readNextEvent = function()
            event = ccall(("zcm_eventlog_read_next_event", "libzcm"), Ptr{Native.EventLogEvent},
                          (Ptr{Native.EventLog},), instance.eventLog)
            return LogEvent(event);
        end

        instance.readPrevEvent = function()
            event = ccall(("zcm_eventlog_read_prev_event", "libzcm"), Ptr{Native.EventLogEvent},
                          (Ptr{Native.EventLog},), instance.eventLog)
            return LogEvent(event);
        end

        instance.readEventAtOffset = function(offset::Int64)
            event = ccall(("zcm_eventlog_read_event_at_offset", "libzcm"), Ptr{Native.EventLogEvent},
                          (Ptr{Native.EventLog}, Int64), instance.eventLog, offset)
            return LogEvent(event);
        end

        # RRR: need to make a way to encode the event into a native object before we'll
        #      be able to write different data out to the file
        instance.writeEvent = function(event::LogEvent)
            return ccall(("zcm_eventlog_write_event", "libzcm"), Int32,
                         (Ptr{Native.EventLog}, Ptr{Native.EventLogEvent}),
                         instance.eventLog, event.event);
        end

        return instance;
    end


end
export LogFile;


end

