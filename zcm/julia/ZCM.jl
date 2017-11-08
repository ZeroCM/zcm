module ZCM

export Zcm,
       subscribe,
       unsubscribe,
       publish,
       good,
       flush,
       start,
       stop,
       handle,
       handle_nonblock,
       close,
       LogEvent,
       LogFile,
       read_next_event,
       read_prev_event,
       read_event_at_offset,
       write_event

import Base: flush,
             start,
             unsafe_convert

# Various compatibility fixes to ensure this runs on Julia v0.5 and v0.6
@static if VERSION < v"0.6.0-"
    # this syntax was deprecated in Julia v0.6. This definition has
    # to be wrapped in an include_string to avoid throwing deprecation
    # warnings on v0.6. This can be eliminated when v0.5 support is
    # dropped.
    include_string("abstract AbstractZCMType")

    # takebuf_array was deprecated in favor of take! in v0.6
    _takebuf_array(buf) = takebuf_array(buf)
else
    include_string("abstract type AbstractZCMType end")

    _takebuf_array(buf) = take!(buf)
end

# Function stubs. Autogenerated ZCM types will extend these functions
# with new methods.
function encode end
function decode end


# RRR / Note: Julia requires that the memory layout of the C structs is consistent
#             between their definitions in zcm headers and this file

# C ptr types for types that we don't need the internals of
module Native

type Zcm
end

type Sub
end

type UvSub
end

immutable RecvBuf
    recv_utime ::Int64
    zcm        ::Ptr{Void}
    # TODO: This makes the assumption that char in C is 8 bits, which is not required to be true
    data       ::Ptr{UInt8}
    data_size  ::UInt32
end

type EventLog
end

type EventLogEvent
    eventnum  ::Int64
    timestamp ::Int64
    chanlen   ::Int32
    datalen   ::Int32
    channel   ::Ptr{UInt8}
    data      ::Ptr{UInt8}
end

end

using .Native: RecvBuf

"""
The SubscriptionOptions type contains all of the information necessary to
decode and handle a message on the Julia side. This is the structure which is
passed in to zcm_subscribe, and it is thus also passed to the handler_wrapper()
cfunction.
"""
immutable SubscriptionOptions{T <: AbstractZCMType, F}
    msgtype::Type{T}
    handler::F
end

"""
The Subscription type contains the SubscriptionOptions and also all of the
various C pointers to the libuv handlers.
"""
immutable Subscription{T <: SubscriptionOptions}
    options::T
    c_handler::Ptr{Void}
    uv_wrapper::Ptr{Native.UvSub}
    uv_handler::Ptr{Void}
    native_sub::Ptr{Native.Sub}
end

type Zcm
    zcm::Ptr{Native.Zcm}
    subscriptions::Vector{Subscription}

    function Zcm(url::AbstractString = "")
        pointer = ccall(("zcm_create", "libzcm"), Ptr{Native.Zcm}, (Cstring,), url);
        instance = new(pointer, Subscription[])
        finalizer(instance, close)
        return instance
    end
end

function close(zcm::Zcm)
    if zcm.zcm != C_NULL
        ccall(("zcm_destroy", "libzcm"), Void,
              (Ptr{Native.Zcm},), zcm)
        zcm.zcm = C_NULL
    end
end

# Defines the conversion when we pass this to a C function expecting a pointer
unsafe_convert(::Type{Ptr{Native.Zcm}}, zcm::Zcm) = zcm.zcm

function good(zcm::Zcm)
    (zcm.zcm != C_NULL) && (errno(zcm) == 0)
end

function errno(zcm::Zcm)
    ccall(("zcm_errno", "libzcm"), Cint, (Ptr{Native.Zcm},), zcm)
end

function strerror(zcm::Zcm)
    val =  ccall(("zcm_strerror", "libzcm"), Cstring, (Ptr{Native.Zcm},), zcm)
    if (val == C_NULL)
        return "unable to get strerror"
    else
        return unsafe_string(val)
    end
end

function handler_wrapper(rbuf::Native.RecvBuf, channelbytes::Cstring, opts::SubscriptionOptions)
    channel = unsafe_string(channelbytes)
    msgdata = unsafe_wrap(Vector{UInt8}, rbuf.data, rbuf.data_size)
    msg = decode(opts.msgtype, msgdata)
    opts.handler(channel, msg)
    return nothing
end

function subscribe{T <: SubscriptionOptions}(zcm::Zcm, channel::AbstractString, options::T)
    # The C function is called with the receive buffer, channel name, and the
    # SubscriptionOptions struct
    c_handler = cfunction(handler_wrapper, Void, (Ref{Native.RecvBuf}, Cstring, Ref{T}))
    uv_wrapper = ccall(("uv_zcm_msg_handler_create", "libzcmjulia"),
                       Ptr{Native.UvSub},
                       (Ptr{Void}, Ptr{Void}),
                       c_handler, Ref(options))
    uv_handler = cglobal(("uv_zcm_msg_handler_trigger", "libzcmjulia"))
    csub = ccall(("zcm_subscribe", "libzcm"), Ptr{Native.Sub},
                (Ptr{Native.Zcm}, Cstring, Ptr{Void}, Ptr{Native.UvSub}),
                zcm, channel, uv_handler, uv_wrapper)
    sub = Subscription(options, c_handler, uv_wrapper, uv_handler, csub)
    push!(zcm.subscriptions, sub)
    return sub
end

"""
    subscribe(zcm::Zcm, channel::AbstractString, msgtype::Type, handler, additional_args...)

Adds a subscription using ZCM object `zcm` for messages of type `msgtype` on
the given channel. The `handler` must be a function or callable object, and
will be called with:

    handler(channel::String, msg::msgtype)

If additional arguments are supplied to `subscribe()` after the handler,
then they will also be passed to the handler each time it is called. So:

    subscribe(zcm, channel, msgtype, handler, X, Y, Z)

will cause `handler()` to be invoked with:

    handler(channel, msg, X, Y, Z)
"""
function subscribe{T <: AbstractZCMType}(zcm::Zcm,
                                         channel::AbstractString,
                                         msgtype::Type{T},
                                         handler, additional_args...)
    callback = (channel, message) -> handler(channel, message, additional_args...)
    subscribe(zcm, channel, SubscriptionOptions(msgtype, callback))
end

function unsubscribe(zcm::Zcm, sub::Subscription)
    ret = ccall(("zcm_unsubscribe", "libzcm"), Cint,
                 (Ptr{Native.Zcm}, Ptr{Native.Sub}), zcm, sub.native_sub)
    ccall(("uv_zcm_msg_handler_destroy", "libzcmjulia"), Void,
          (Ptr{Native.UvSub},), sub.uv_wrapper)
    deleteat!(zcm.subscriptions, findin(zcm.subscriptions, [sub]))
    return ret
end

function publish(zcm::Zcm, channel::AbstractString, data::Vector{UInt8})
    return ccall(("zcm_publish", "libzcm"), Cint,
                 (Ptr{Native.Zcm}, Cstring, Ptr{Void}, UInt32),
                 zcm, convert(String, channel), data, length(data))
end

# TODO: force msg to be derived from our zcm msg basetype
function publish(zcm::Zcm, channel::AbstractString, msg)
    publish(zcm, channel, encode(msg))
end

function flush(zcm::Zcm)
    ccall(("zcm_flush", "libzcm"), Void, (Ptr{Native.Zcm},), zcm)
end

function start(zcm::Zcm)
    ccall(("zcm_start", "libzcm"), Void, (Ptr{Native.Zcm},), zcm)
end

function stop(zcm::Zcm)
    ccall(("zcm_stop", "libzcm"), Void, (Ptr{Native.Zcm},), zcm)
end

function handle(zcm::Zcm)
    ccall(("zcm_handle", "libzcm"), Cint, (Ptr{Native.Zcm},), zcm)
end

function handle_nonblock(zcm::Zcm)
    ccall(("zcm_handle_nonblock", "libzcm"), Cint, (Ptr{Native.Zcm},), zcm)
end

# RRR: go over the signedness of all these types from within zcm ... some of them are dumb
type LogEvent
    event   ::Ptr{Native.EventLogEvent}
    num     ::Int64
    utime   ::Int64
    channel ::String
    data    ::Array{UInt8}

    function LogEvent(event::Ptr{Native.EventLogEvent})
        instance  = new()
        instance.event = event

        if (event != C_NULL)
            loadedEvent = unsafe_load(event)

            instance.num   = loadedEvent.eventnum
            instance.utime = loadedEvent.timestamp
            if (loadedEvent.channel != C_NULL)
                instance.channel = unsafe_string(loadedEvent.channel, loadedEvent.chanlen)
            else
                instance.channel = ""
            end
            if (loadedEvent.data != C_NULL)
                instance.data    = unsafe_wrap(Array, loadedEvent.data, loadedEvent.datalen)
            else
                instance.data    = []
            end
        end

        # user can force cleanup of their instance by calling `finalize(zcm)`
        finalizer(instance, function(event::LogEvent)
                                if (event.event != C_NULL)
                                    ccall(("zcm_eventlog_free_event", "libzcm"), Void,
                                          (Ptr{Native.EventLogEvent},), event.event)
                                    event.event = C_NULL
                                end
                            end)

        return instance
    end
end

type LogFile
    eventLog::Ptr{Native.EventLog}

    function LogFile(path::AbstractString, mode::AbstractString)
        println("Creating zcm eventlog from path : ", path, " with mode ", mode)
        instance = new()
        instance.eventLog = ccall(("zcm_eventlog_create", "libzcm"), Ptr{Native.EventLog},
                                  (Cstring, Cstring), path, mode)

        # user can force cleanup of their instance by calling `finalize(zcm)`
        finalizer(instance, function(log::LogFile)
                                if (log.eventLog != C_NULL)
                                    ccall(("zcm_eventlog_destroy", "libzcm"), Void,
                                          (Ptr{Native.EventLog},), log.eventLog)
                                    log.eventLog = C_NULL
                                end
                            end)

        return instance
    end
end

function good(lf::LogFile)
    return lf.eventLog != C_NULL
end

function read_next_event(lf::LogFile)
    event = ccall(("zcm_eventlog_read_next_event", "libzcm"), Ptr{Native.EventLogEvent},
                  (Ptr{Native.EventLog},), lf.eventLog)
    return LogEvent(event)
end

function read_prev_event(lf::LogFile)
    event = ccall(("zcm_eventlog_read_prev_event", "libzcm"), Ptr{Native.EventLogEvent},
                  (Ptr{Native.EventLog},), lf.eventLog)
    return LogEvent(event)
end

function read_event_at_offset(lf::LogFile, offset::Int64)
    event = ccall(("zcm_eventlog_read_event_at_offset", "libzcm"), Ptr{Native.EventLogEvent},
                  (Ptr{Native.EventLog}, Int64), lf.eventLog, offset)
    return LogEvent(event)
end

# RRR: need to make a way to encode the event into a native object before we'll
#      be able to write different data out to the file
function write_event(lf::LogFile, event::LogEvent)
    return ccall(("zcm_eventlog_write_event", "libzcm"), Cint,
                 (Ptr{Native.EventLog}, Ptr{Native.EventLogEvent}),
                 lf.eventLog, event.event)
end

end # module ZCM

