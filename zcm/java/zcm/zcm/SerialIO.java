package zcm.zcm;

import java.nio.ByteBuffer;

/**
 * Interface for serial I/O operations used by the generic serial transport.
 * Implementations of this interface handle the actual reading from and writing to
 * the underlying serial hardware or communication channel.
 */
public interface SerialIO {
    /**
     * Read data from the serial interface.
     * This method should read up to maxLen bytes from the serial interface
     * and store them in the provided ByteBuffer. The buffer is positioned at 0
     * and has a limit of maxLen.
     *
     * @param buffer ByteBuffer to store the read data (direct buffer, zero-copy)
     * @param maxLen maximum number of bytes to read
     * @return number of bytes actually read, or 0 if no data available
     */
    int get(ByteBuffer buffer, int maxLen);

    /**
     * Write data to the serial interface.
     * This method should write len bytes from the ByteBuffer to the serial interface.
     * The buffer is positioned at 0 and contains len bytes to write.
     *
     * @param buffer ByteBuffer containing data to write (direct buffer, zero-copy)
     * @param len number of bytes to write from the buffer
     * @return number of bytes actually written
     */
    int put(ByteBuffer buffer, int len);
}
