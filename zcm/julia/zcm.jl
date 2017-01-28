module ZCM


# C ptr types
module Native

type Zcm
end

type RecvBuf
end

type Sub
end

type Eventlog
end

type EventlogEvent
end

end


# Exported Objects and Methods
type Zcm
    zcm::Ptr{Native.Zcm};

    errno      ::Function; # Int32  ()
    strerror   ::Function; # String ()

    # TODO refine these comments
    subscribe  ::Function; # Sub   (String channel, Handler handler, Any usr)
    unsubscribe::Function; # Int32 (Sub)
    publish    ::Function; # Int32 (String channel, Msg msg)
    publishRaw ::Function; # Int32 (String channel, Array{Uint8} data, Uint32 datalen)
    flush      ::Function; # Void  ()

    run        ::Function; # Void  ()
    start      ::Function; # Void  ()
    stop       ::Function; # Void  ()
    handle     ::Function; # Void  ()


    function Zcm(url::AbstractString)
        println("Creating zcm with url : ", url);
        instance = new(ccall(("zcm_create", "libzcm"), Ptr{Native.Zcm}, (Cstring,), url));

        # user can force cleanup of their instance by calling `finalize(zcm)`
        finalizer(instance, function (zcm::Zcm)
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

        return instance;
    end
end
export Zcm;

type RecvBuf
end
export RecvBuf;

type Sub
end
export Sub;

type Eventlog
end
export Eventlog;

type EventlogEvent
end
export EventlogEvent;


end

