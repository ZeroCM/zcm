package zcm.zcm;

/**
 * Interface that all Java ZCM transport implementations must implement.
 * This interface provides a bridge between Java transport implementations
 * and the native ZCM library.
 */
public interface ZCMTransport {
    /**
     * Get the native transport pointer for use with the ZCM native library.
     * This method returns a pointer to the underlying C transport structure
     * that can be passed to zcm_init_from_trans().
     *
     * @return native transport pointer as a long value, or 0 if invalid
     */
    long getNativeTransport();

    /**
     * Clean up and destroy the transport, freeing all resources.
     * After calling this method, the transport should not be used.
     */
    void destroy();

    /**
     * Signal to the transport that the native transport pointer memory
     * will be managed by ZCM. This is invoked as soon as a ZCM is created
     * from this transport.
     */
    void releaseNativeTransportMemoryToZcm();
}
