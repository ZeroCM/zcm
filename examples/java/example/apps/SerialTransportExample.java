package example.apps;

import zcm.zcm.*;
import javazcm.types.example_t;
import java.io.IOException;

/**
 * A comprehensive example demonstrating ZCM communication using a custom serial transport.
 * This example shows both publishing and subscribing using a loopback SerialIO implementation,
 * which allows testing serial transport functionality without actual hardware.
 */
public class SerialTransportExample {

    private static class ExampleSubscriber implements ZCMSubscriber {
        public void messageReceived(ZCM zcm, String channel, long recvUtime, ZCMDataInputStream ins) {
            try {
                example_t msg = new example_t(ins);
                System.out.println("Received message on channel '" + channel + "':");
                System.out.println("  timestamp: " + msg.timestamp);
                System.out.println("  position: [" + msg.position[0] + ", " + msg.position[1] + ", " + msg.position[2] + "]");
                System.out.println("  orientation: [" + msg.orientation[0] + ", " + msg.orientation[1] + ", " + msg.orientation[2] + ", " + msg.orientation[3] + "]");
                System.out.println("  name: " + msg.name);
                System.out.println("  enabled: " + msg.enabled);
                System.out.println("  num_ranges: " + msg.num_ranges);
                System.out.print("  ranges: [");
                for (int i = 0; i < msg.num_ranges; i++) {
                    System.out.print(msg.ranges[i]);
                    if (i < msg.num_ranges - 1) System.out.print(", ");
                }
                System.out.println("]");
                System.out.println();
            } catch (IOException e) {
                System.err.println("Error deserializing message: " + e.getMessage());
            }
        }
    }

    public static void main(String[] args) {
        try {
            // Create a loopback SerialIO implementation for testing
            LoopbackSerialIO serialIO = new LoopbackSerialIO(50000); // 50KB buffer

            System.out.println("Creating ZCM transport with custom SerialIO...");

            // Create the ZCM transport using the SerialIO
            // MTU: 16KB, Buffer size: 128KB
            ZCMTransport transport = new ZCMGenericSerialTransport(serialIO, 1 << 14, 1 << 17);
            ZCM zcm = new ZCM(transport);

            System.out.println("Transport created successfully!");

            // Subscribe to messages
            System.out.println("Setting up subscriber...");
            ExampleSubscriber subscriber = new ExampleSubscriber();
            zcm.subscribe("EXAMPLE", subscriber);

            // Start ZCM in a separate thread for message handling
            zcm.start();

            System.out.println("Starting to publish messages...");

            // Create and publish messages
            example_t msg = new example_t();

            int messageCount = 0;
            while (messageCount < 10) {
                // Update message with current data
                msg.timestamp = System.currentTimeMillis() * 1000;
                msg.position = new double[] {
                    1.0 + messageCount * 0.1,
                    2.0 + messageCount * 0.2,
                    3.0 + messageCount * 0.3
                };
                msg.orientation = new double[] { 1, 0, 0, 0 };
                msg.ranges = new short[messageCount + 1];
                for (int i = 0; i <= messageCount; i++) {
                    msg.ranges[i] = (short) i;
                }
                msg.num_ranges = msg.ranges.length;
                msg.name = "example string #" + messageCount;
                msg.enabled = (messageCount % 2 == 0);

                System.out.println("Publishing message #" + messageCount + "...");
                zcm.publish("EXAMPLE", msg);

                messageCount++;

                // Wait a bit between messages
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException ex) {
                    System.out.println("Interrupted, stopping...");
                    break;
                }
            }

            // Give some time for the last messages to be processed
            System.out.println("Waiting for message processing to complete...");
            Thread.sleep(2000);

            // Stop ZCM
            zcm.stop();

            // Clean up
            if (transport instanceof AutoCloseable) {
                ((AutoCloseable) transport).close();
            }

            System.out.println("Example completed successfully!");

        } catch (IOException e) {
            System.err.println("Failed to create ZCM transport: " + e.getMessage());
            e.printStackTrace();
        } catch (InterruptedException e) {
            System.err.println("Thread interrupted: " + e.getMessage());
            e.printStackTrace();
        } catch (Exception e) {
            System.err.println("Unexpected error: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
