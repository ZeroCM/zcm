package zcm.zcm;
import java.io.IOException;

class ZCMJNI
{
    static {
        System.loadLibrary("zcmjni");
    }

    private long nativePtr = 0;
    private native boolean initializeNative(String url);
    public ZCMJNI(String url) throws IOException
    {
        if (!initializeNative(url)) {
            String msg = (url != null) ?
                "Failed to create ZCM for '" + url + "'" :
                "Failed to create ZCM using the default url";
            throw new IOException(msg);
        }
    }

    // This method publishes the provided data on the requested channel
    public native int publish(String channel, byte[] data, int offset, int length);

    // This method registers the ZCM object for an upcall to receiveMessage()
    public native Object subscribe(String channel, ZCM zcm, Object usr);

    // This method registers the ZCM object for an upcall to receiveMessage()
    public native int unsubscribe(Object usr);
}
