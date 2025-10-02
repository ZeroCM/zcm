package zcm.zcm;

import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Java wrapper for the ZCM generic serial transport.
 * This class provides a Java interface to the C generic serial transport implementation,
 * allowing Java applications to communicate over serial interfaces using ZCM.
 */
public class ZCMGenericSerialTransport implements ZCMTransport, AutoCloseable {

    static {
        ZCMNativeLoader.loadLibrary();
    }

    private long nativePtr = 0;
    private SerialIO serialIO;

    /**
     * Create a new generic serial transport.
     *
     * @param serialIO the SerialIO implementation for actual hardware communication
     * @param mtu Maximum Transmission Unit - maximum message size in bytes
     * @param bufSize buffer size for internal circular buffers
     * @throws IOException if the transport cannot be created
     */
    public ZCMGenericSerialTransport(SerialIO serialIO, int mtu, int bufSize) throws IOException {
        if (serialIO == null) {
            throw new IllegalArgumentException("SerialIO cannot be null");
        }
        if (mtu <= 0) {
            throw new IllegalArgumentException("MTU must be positive");
        }
        if (bufSize <= 0) {
            throw new IllegalArgumentException("Buffer size must be positive");
        }

        if (!ZCMNativeLoader.isLibraryLoaded()) {
            throw new AssertionError("Library not yet loaded");
        }

        this.serialIO = serialIO;

        if (!initializeNative(mtu, bufSize)) {
            throw new IOException("Failed to create ZCM Generic Serial Transport");
        }
    }

    /**
     * Initialize the native transport.
     *
     * @param mtu Maximum Transmission Unit
     * @param bufSize buffer size for internal buffers
     * @return true if successful, false otherwise
     */
    private native boolean initializeNative(int mtu, int bufSize);

    /**
     * Get the transport pointer for use with ZCM.
     * This method is called by ZCM to get the underlying transport.
     *
     * @return native transport pointer
     */
    private native long getTransportPtr();

    /**
     * Get the native transport pointer for use with ZCM.
     * This implements the ZCMTransport interface.
     *
     * @return native transport pointer
     */
    @Override
    public long getNativeTransport() {
        return getTransportPtr();
    }

    /**
     * Destroy the transport and free all resources.
     * This implements the ZCMTransport interface.
     */
    @Override
    public native void destroy();

    /**
     * Signal to the tranport to that it should no longer use
     * the native C transport and cease interactions with it.
     * This implements the ZCMTransport interface.
     */
    @Override
    public native void releaseNativeTransport();

    /**
     * Close the transport and free all resources.
     * This implements the AutoCloseable interface for try-with-resources support.
     */
    @Override
    public void close() {
        destroy();
    }

    /**
     * Called by native code to read data from the serial interface.
     * This method is called from the C layer when data needs to be read.
     *
     * @param buffer direct ByteBuffer wrapping the native data array
     * @param maxLen maximum number of bytes to read
     * @param timeoutMs number of ms this call may block for. 0 indicates nonblocking
     * @return number of bytes actually read
     */
    private int nativeGet(ByteBuffer buffer, int maxLen, int timeoutMs) {
        try {
            return serialIO.get(buffer, maxLen, timeoutMs);
        } catch (Exception e) {
            // Don't let exceptions propagate through JNI
            System.err.println("Exception in SerialIO.get(): " + e.getMessage());
            return 0;
        }
    }

    /**
     * Called by native code to write data to the serial interface.
     * This method is called from the C layer when data needs to be written.
     *
     * @param buffer direct ByteBuffer wrapping the native data array
     * @param len number of bytes to write
     * @param timeoutMs number of ms this call may block for. 0 indicates nonblocking
     * @return number of bytes actually written
     */
    private int nativePut(ByteBuffer buffer, int len, int timeoutMs) {
        try {
            return serialIO.put(buffer, len, timeoutMs);
        } catch (Exception e) {
            // Don't let exceptions propagate through JNI
            System.err.println("Exception in SerialIO.put(): " + e.getMessage());
            return 0;
        }
    }
}
