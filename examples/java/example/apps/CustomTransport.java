package example.apps;

import zcm.zcm.*;
import javazcm.types.example_t;
import java.io.IOException;

public class CustomTransport implements ZCMSubscriber
{
    public void messageReceived(ZCM zcm, String channel, ZCMDataInputStream ins)
    {
        try {
            example_t msg = new example_t(ins);
            System.out.println("msg timestamp: " + msg.timestamp);
        } catch (IOException ex) {
            System.out.println("Exception: " + ex);
        }
    }

    public static void main(String[] args) throws IOException
    {
        CustomTransport t = new CustomTransport();

        // Create a loopback SerialIO implementation for testing
        LoopbackSerialIO serialIO = new LoopbackSerialIO();

        // Create the ZCM transport using the SerialIO
        ZCMTransport transport = new ZCMGenericSerialTransport(serialIO, 1 << 14, 1 << 17);
        ZCM zcm = new ZCM(transport);

        String channel = "EXAMPLE";

        zcm.subscribe(channel, t);
        zcm.start();

        example_t msg = new example_t();
        msg.position = new double[] { 1, 2, 3 };
        msg.orientation = new double[] { 1, 0, 0, 0 };
        msg.ranges = new short[] {
            0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
        };
        msg.num_ranges = msg.ranges.length;
        msg.name = "example string";
        msg.enabled = true;

        while (true) {
            msg.timestamp = System.currentTimeMillis() * 1000;
            zcm.publish(channel, msg);
            try { Thread.sleep(100); } catch (InterruptedException ex) {}
        }
    }
}
