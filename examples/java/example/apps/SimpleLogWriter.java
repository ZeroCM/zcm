package example.apps;

import java.io.*;
import zcm.zcm.*;

/**
 * A minimal example of writing events to a ZCM log file.
 * This demonstrates the basic usage of the ZCM Log class for event logging.
 */
public class SimpleLogWriter {

    public static void main(String[] args) {
        try {
            // Create a log file for writing
            Log log = new Log("events.log", "rw");

            // Create some sample events
            Log.Event event1 = new Log.Event();
            event1.eventNumber = 0;
            event1.utime = System.currentTimeMillis() * 1000; // Convert to microseconds
            event1.channel = "SENSOR_DATA";
            event1.data = "temperature:25.3".getBytes();

            Log.Event event2 = new Log.Event();
            event2.eventNumber = 1;
            event2.utime = System.currentTimeMillis() * 1000 + 1000000; // 1 second later
            event2.channel = "STATUS";
            event2.data = "system:online".getBytes();

            // Write events to log
            log.write(event1);
            log.write(event2);

            // Flush and close
            log.flush();
            log.close();

            System.out.println("Successfully wrote 2 events to events.log");

        } catch (IOException e) {
            System.err.println("Error writing to log: " + e.getMessage());
        }
    }
}
