package zcm.zcm;

/**
 * Utility class for loading ZCM native libraries.
 * This class ensures that the required native libraries are loaded exactly once
 * and provides a centralized place for library loading logic.
 */
public final class ZCMNativeLoader {

    private static volatile boolean isLoaded = false;
    private static final Object lock = new Object();

    // Private constructor to prevent instantiation
    private ZCMNativeLoader() {
        throw new AssertionError("ZCMNativeLoader should not be instantiated");
    }

    /**
     * Loads the ZCM native library if it hasn't been loaded already.
     * This method is thread-safe and ensures the library is loaded exactly once.
     *
     * @throws UnsatisfiedLinkError if the native library cannot be loaded
     */
    public static void loadLibrary() {
        if (!isLoaded) {
            synchronized (lock) {
                if (!isLoaded) {
                    try {
                        System.loadLibrary("zmq");
                    } catch (UnsatisfiedLinkError e) {}
                    try {
                        System.loadLibrary("zcm");
                    } catch (UnsatisfiedLinkError e) {
                        throw new UnsatisfiedLinkError(
                            "Failed to load ZCM native library 'zcm': " + e.getMessage());
                    }
                    try {
                        System.loadLibrary("zcmjni");
                    } catch (UnsatisfiedLinkError e) {
                        throw new UnsatisfiedLinkError(
                            "Failed to load ZCM native library 'zcmjni': " + e.getMessage());
                    }
                    isLoaded = true;
                }
            }
        }
    }

    /**
     * Returns whether the ZCM native library has been loaded.
     *
     * @return true if the library is loaded, false otherwise
     */
    public static boolean isLibraryLoaded() {
        return isLoaded;
    }

    /**
     * Static initializer to automatically load the library when this class is first referenced.
     */
    static {
        loadLibrary();
    }
}
