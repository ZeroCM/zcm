package example.apps;

import java.io.*;
import java.util.Arrays;
import zcm.zcm.*;
import javazcm.types.bitfield_t;

public class BitfieldSub implements ZCMSubscriber
{
    ZCM zcm;
    ZCM.Subscription s;

    public BitfieldSub()
    {
        zcm = ZCM.getSingleton();
        zcm.start();
        sub();
    }

    public void sub()
    {
        s = zcm.subscribe("BITFIELD", this);
    }

    public void unsub()
    {
        zcm.unsubscribe(s);
    }

    public void messageReceived(ZCM zcm, String channel, ZCMDataInputStream ins)
    {
        System.out.println("Received message on channel " + channel);

        try {
            if (channel.equals("BITFIELD")) {
                bitfield_t b = new bitfield_t(ins);
                System.out.println("***********************");
                System.out.println(b.field1);
                System.out.print("[");
                for (int i = 0; i < b.field2.length; ++i) {
                    System.out.println(Arrays.toString(b.field2[i]));
                    System.out.print(", ");
                }
                System.out.println("]");
                System.out.println(b.field3);
                System.out.println(b.field4);
                System.out.println(b.field5);
                System.out.println(b.field6);
                System.out.println(b.field7);
                System.out.println(b.field8_dim1);
                System.out.println(b.field8_dim2);
                System.out.print("[");
                for (int i = 0; i < b.field8_dim1; ++i) {
                    System.out.print(Arrays.toString(b.field8[i]));
                    System.out.print(", ");
                }
                System.out.println("]");
                System.out.println(b.field9);
                System.out.println(b.field10);
                System.out.println(b.field11);
                System.out.print("[");
                for (int l = 0; l < 3; ++l) {
                    System.out.print("[");
                    for (int k = 0; k < 2; ++k) {
                        System.out.print("[");
                        for (int j = 0; j < 2; ++j) {
                            System.out.print(Arrays.toString(b.field12[l][k][j]));
                            System.out.print(", ");
                        }
                        System.out.print("]");
                    }
                    System.out.print("]");
                }
                System.out.println("]");
            }
        } catch (IOException ex) {
            System.out.println("Exception: " + ex);
        }
    }

    static void sleep(long ms)
    { try { Thread.sleep(ms); } catch (InterruptedException ex) {} }

    public static void main(String[] args)
    {
        BitfieldSub s = new BitfieldSub();
        while(true) {
            sleep(1000);
        }
    }
}
