package example.apps;

import java.io.*;
import zcm.zcm.*;
import javazcm.types.example_t;

public class Sub implements ZCMSubscriber
{
    ZCM zcm;
    ZCM.Subscription s;

    public Sub()
    {
        zcm = ZCM.getSingleton();
        zcm.start();
        sub();
    }

    public void sub()
    {
        s = zcm.subscribe("EXAMPLE", this);
    }

    public void unsub()
    {
        zcm.unsubscribe(s);
    }

    public void messageReceived(ZCM zcm, String channel, ZCMDataInputStream ins)
    {
        System.out.println("Received message on channel " + channel);

        try {
            if (channel.equals("EXAMPLE")) {
                example_t msg = new example_t(ins);

                System.out.println("  timestamp    = " + msg.timestamp);
                System.out.println("  position     = [ " + msg.position[0] +
                                   ", " + msg.position[1] + ", " + msg.position[2] + " ]");
                System.out.println("  orientation  = [ " + msg.orientation[0] +
                                   ", " + msg.orientation[1] +
                                   ", " + msg.orientation[2] +
                                   ", " + msg.orientation[3] + " ]");

                System.out.print("  ranges       = [ ");
                for (int i=0; i<msg.num_ranges; i++) {
                    System.out.print("" + msg.ranges[i]);
                    if (i < msg.num_ranges-1)
                        System.out.print (", ");
                }
                System.out.println (" ]");
                System.out.println("  name         = '" + msg.name + "'");
                System.out.println("  enabled      = '" + msg.enabled + "'");
            }

        } catch (IOException ex) {
            System.out.println("Exception: " + ex);
        }
    }

    static void sleep(long ms)
    {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException ex) {}
    }

    public static void main(String[] args)
    {
        Sub s = new Sub();
        while(true) {
            sleep(5000);
            s.unsub();
            System.out.println("\nunsubscribing\n");
            sleep(5000);
            s.sub();
            System.out.println("\nresubscribing\n");
        }
    }
}
