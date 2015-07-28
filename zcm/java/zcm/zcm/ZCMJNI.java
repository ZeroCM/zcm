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
        if (!initializeNative(url))
            throw new IOException("Failed to create ZCM for '"+url+"'");
    }

    // This method publishes the provided data on the requested channel
    public native int publish(String channel, byte[] data, int offset, int length);

    // This method registers the ZCM object for an upcall to receiveMessage()
    public native int subscribe(String channel, ZCM zcm);

    // This method dispatches the next message (blocking indefinitely)
    public native int handle();
}
