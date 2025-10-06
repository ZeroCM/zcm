package example.apps;

import zcm.zcm.SerialIO;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

/**
 * A simple test runner to demonstrate and validate SerialIO implementations
 * without requiring the full ZCM infrastructure.
 */
public class SerialIOTest {

    public static void main(String[] args) {
        System.out.println("ZCM SerialIO Test Suite");
        System.out.println("=======================");

        // Test LoopbackSerialIO
        testLoopbackSerialIO();

        System.out.println("\nAll tests completed!");
    }

    /**
     * Test the LoopbackSerialIO implementation
     */
    private static void testLoopbackSerialIO() {
        System.out.println("\n--- Testing LoopbackSerialIO ---");

        LoopbackSerialIO serialIO = new LoopbackSerialIO(1000);

        // Test data
        String testMessage = "Hello ZCM Serial!";
        byte[] testData = testMessage.getBytes(StandardCharsets.UTF_8);

        // Test writing
        ByteBuffer writeBuffer = ByteBuffer.allocateDirect(testData.length);
        writeBuffer.put(testData);
        writeBuffer.flip();

        int bytesWritten = serialIO.put(writeBuffer, testData.length);
        System.out.println("Wrote " + bytesWritten + " bytes: \"" + testMessage + "\"");

        // Test reading
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(1000);
        int bytesRead = serialIO.get(readBuffer, 1000, 0);

        if (bytesRead > 0) {
            readBuffer.flip();
            byte[] readData = new byte[bytesRead];
            readBuffer.get(readData);
            String receivedMessage = new String(readData, StandardCharsets.UTF_8);
            System.out.println("Read " + bytesRead + " bytes: \"" + receivedMessage + "\"");

            if (testMessage.equals(receivedMessage)) {
                System.out.println("LoopbackSerialIO test PASSED");
            } else {
                System.out.println("LoopbackSerialIO test FAILED - data mismatch");
            }
        } else {
            System.out.println("LoopbackSerialIO test FAILED - no data read");
        }

        // Test queue status
        System.out.println("Available bytes: " + serialIO.available());
        System.out.println("Max queue size: " + serialIO.getMaxQueueSize());
    }

    /**
     * Utility method to demonstrate ByteBuffer operations
     */
    private static void demonstrateByteBufferUsage() {
        System.out.println("\n--- ByteBuffer Usage Demo ---");

        // Create a direct ByteBuffer (recommended for SerialIO)
        ByteBuffer buffer = ByteBuffer.allocateDirect(100);
        System.out.println("Created direct buffer with capacity: " + buffer.capacity());

        // Write some data
        String data = "Sample data";
        buffer.put(data.getBytes(StandardCharsets.UTF_8));
        System.out.println("After writing, position: " + buffer.position() + ", remaining: " + buffer.remaining());

        // Flip buffer for reading
        buffer.flip();
        System.out.println("After flip, position: " + buffer.position() + ", limit: " + buffer.limit());

        // Read the data back
        byte[] readData = new byte[buffer.remaining()];
        buffer.get(readData);
        System.out.println("Read back: \"" + new String(readData, StandardCharsets.UTF_8) + "\"");

        // Clear buffer for reuse
        buffer.clear();
        System.out.println("After clear, position: " + buffer.position() + ", limit: " + buffer.limit());
    }
}
