package example.apps;

import zcm.zcm.SerialIO;
import java.nio.ByteBuffer;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * A loopback SerialIO implementation for testing purposes.
 * This implementation uses internal queues to simulate serial communication,
 * where data written to the "serial port" can be read back from it.
 * This is useful for testing ZCM serial transport without actual hardware.
 */
public class LoopbackSerialIO implements SerialIO {

    private final BlockingQueue<Byte> dataQueue;
    private final int maxQueueSize;

    /**
     * Create a new loopback SerialIO with default queue size.
     */
    public LoopbackSerialIO() {
        this(10000); // Default queue size of 10KB
    }

    /**
     * Create a new loopback SerialIO with specified queue size.
     *
     * @param maxQueueSize maximum number of bytes to buffer internally
     */
    public LoopbackSerialIO(int maxQueueSize) {
        this.maxQueueSize = maxQueueSize;
        this.dataQueue = new LinkedBlockingQueue<>(maxQueueSize);
    }

    /**
     * Read data from the internal queue (simulating serial read).
     *
     * @param buffer ByteBuffer to store the read data
     * @param maxLen maximum number of bytes to read
     * @param timeoutMs timeout in milliseconds, 0 for non-blocking
     * @return number of bytes actually read
     */
    @Override
    public int get(ByteBuffer buffer, int maxLen, int timeoutMs) {
        if (buffer == null || maxLen <= 0) {
            return 0;
        }

        int bytesRead = 0;

        if (timeoutMs == 0) {
            // Non-blocking mode - drain available bytes directly into buffer
            while (bytesRead < maxLen && !dataQueue.isEmpty()) {
                Byte b = dataQueue.poll();
                if (b != null) {
                    buffer.put(b);
                    bytesRead++;
                }
            }
        } else {
            // Blocking mode - wait for at least one byte, then drain available
            try {
                Byte firstByte = dataQueue.poll(timeoutMs, TimeUnit.MILLISECONDS);
                if (firstByte != null) {
                    buffer.put(firstByte);
                    bytesRead = 1;
                    // Drain any additional available bytes (non-blocking)
                    while (bytesRead < maxLen && !dataQueue.isEmpty()) {
                        Byte b = dataQueue.poll();
                        if (b != null) {
                            buffer.put(b);
                            bytesRead++;
                        }
                    }
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return 0;
            }
        }

        return bytesRead;
    }

    /**
     * Write data to the internal queue (simulating serial write).
     *
     * @param buffer ByteBuffer containing data to write
     * @param len number of bytes to write from the buffer
     * @return number of bytes actually written
     */
    @Override
    public int put(ByteBuffer buffer, int len) {
        if (buffer == null || len <= 0) {
            return 0;
        }

        int bytesWritten = 0;
        int bytesToWrite = Math.min(len, buffer.remaining());

        for (int i = 0; i < bytesToWrite; i++) {
            byte b = buffer.get();
            boolean success = false;

            success = dataQueue.offer(b);

            if (success) {
                bytesWritten++;
            } else {
                // Queue is full or timeout, put the byte back and break
                buffer.position(buffer.position() - 1);
                break;
            }
        }

        return bytesWritten;
    }

    /**
     * Get the current number of bytes available for reading.
     *
     * @return number of bytes in the internal queue
     */
    public int available() {
        return dataQueue.size();
    }

    /**
     * Clear all data from the internal queue.
     */
    public void clear() {
        dataQueue.clear();
    }

    /**
     * Get the maximum queue size.
     *
     * @return maximum number of bytes this SerialIO can buffer
     */
    public int getMaxQueueSize() {
        return maxQueueSize;
    }
}
