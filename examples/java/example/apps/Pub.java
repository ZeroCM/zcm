package example.apps;

import zcm.zcm.*;
import javazcm.types.example_t;

public class Pub
{
    public static void main(String[] args)
    {
        ZCM zcm = ZCM.getSingleton();

        example_t msg = new example_t();
        msg.timestamp = System.currentTimeMillis()*1000;
        msg.position = new double[] { 1, 2, 3 };
        msg.orientation = new double[] { 1, 0, 0, 0 };
        msg.ranges = new short[] {
            0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
        };
        msg.num_ranges = msg.ranges.length;
        msg.name = "example string";
        msg.enabled = true;

        while (true) {
            zcm.publish("EXAMPLE", msg);
            try { Thread.sleep(100); } catch (InterruptedException ex) {}
        }
    }
}
