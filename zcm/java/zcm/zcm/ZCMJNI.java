package zcm.zcm;
import java.io.IOException;

class ZCMJNI
{
    static {
        ZCMNativeLoader.loadLibrary();
    }

    private long nativePtr = 0;
    private native boolean initializeNative(String url);
    private native boolean initializeNativeFromTransport(long transportPtr);

    public ZCMJNI(String url) throws IOException
    {
        if (!initializeNative(url)) {
            String msg = (url != null)
                ? "Failed to create ZCM for '" + url + "'"
                : "Failed to create ZCM using the default url";
            throw new IOException(msg);
        }
    }

    public ZCMJNI(ZCMTransport transport) throws IOException
    {
        if (transport == null) {
            throw new IllegalArgumentException("Transport cannot be null");
        }
        if (!initializeNativeFromTransport(transport.getNativeTransport())) {
            throw new IOException("Failed to create ZCM from transport");
        }
        transport.releaseNativeTransportMemoryToZcm();
    }

    public native void destroy();

    public native void start();
    public native void stop();

    public native int publish(String channel, byte[] data, int offset, int length);

    public native Object subscribe(String channel, ZCM zcm, Object usr);
    public native int unsubscribe(Object usr);

    public native long getNativeZcmPtr();
}
