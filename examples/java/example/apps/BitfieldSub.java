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
            bitfield_t msg = new bitfield_t(ins);
            System.out.println("msg.field1 = " + msg.field1);
            System.out.println("msg.field2[0][0] = " + msg.field2[0][0]);
            System.out.println("msg.field2[0][1] = " + msg.field2[0][1]);
            System.out.println("msg.field2[0][2] = " + msg.field2[0][2]);
            System.out.println("msg.field2[0][3] = " + msg.field2[0][3]);
            System.out.println("msg.field2[1][0] = " + msg.field2[1][0]);
            System.out.println("msg.field2[1][1] = " + msg.field2[1][1]);
            System.out.println("msg.field2[1][2] = " + msg.field2[1][2]);
            System.out.println("msg.field2[1][3] = " + msg.field2[1][3]);
            System.out.println("msg.field3 = " + msg.field3);
            System.out.println("msg.field4 = " + msg.field4);
            System.out.println("msg.field5 = " + msg.field5);
            System.out.println("msg.field6 = " + msg.field6);
            System.out.println("msg.field7 = " + msg.field7);
            System.out.println("msg.field8_dim1 = " + msg.field8_dim1);
            System.out.println("msg.field8_dim2 = " + msg.field8_dim2);
            System.out.println("msg.field8.length = " + msg.field8.length);
            System.out.println("msg.field9 = " + msg.field9);
            System.out.println("msg.field10 = " + msg.field10);
            System.out.println("msg.field11 = " + msg.field11);
            for (int i = 0; i < 2; ++i) {
                for (int j = 0; j < 2; ++j) {
                    for (int k = 0; k < 2; ++k) {
                        for (int l = 0; l < 2; ++l) {
                            System.out.println("msg.field12[i][j][k][l] = " + msg.field12[i][j][k][l]);
                        }
                    }
                }
            }
            System.out.println("msg.field15 = " + msg.field15);
            System.out.println("msg.field16 = " + msg.field16);
            System.out.println("msg.field19 = " + msg.field19);
            System.out.println("msg.field20 = " + msg.field20);
            System.out.println("msg.field22 = " + msg.field22);
            System.out.println("msg.field23 = " + msg.field23);
            System.out.println("msg.field24 = " + msg.field24);

            assert (msg.field1 == -1);
            assert (msg.field2[0][0] == -1);
            assert (msg.field2[0][1] ==  0);
            assert (msg.field2[0][2] == -1);
            assert (msg.field2[0][3] ==  0);
            assert (msg.field2[1][0] == -1);
            assert (msg.field2[1][1] ==  0);
            assert (msg.field2[1][2] == -1);
            assert (msg.field2[1][3] ==  0);
            assert (msg.field3 == -1);
            assert (msg.field4 == -3);
            assert (msg.field5 == 7);
            assert (msg.field6 == 1);
            assert (msg.field7 == 3);
            assert (msg.field8_dim1 == 0);
            assert (msg.field8_dim2 == 0);
            assert (msg.field8.length == 0);
            assert (msg.field9 == ~(1 << 27) + 1);
            assert (msg.field10 == (((long)1 << 52) | 1));
            assert (msg.field11 == -1);
            for (int i = 0; i < 2; ++i) {
                for (int j = 0; j < 2; ++j) {
                    for (int k = 0; k < 2; ++k) {
                        for (int l = 0; l < 2; ++l) {
                            assert (msg.field12[i][j][k][l] == (byte)(k + l + 1));
                        }
                    }
                }
            }
            assert (msg.field15 == -60);
            assert (msg.field16 == 2);
            assert (msg.field19 == -60);
            assert (msg.field20 == 2);
            assert (msg.field22 == msg.FIELD22_TEST);
            assert (msg.field23 == msg.FIELD23_TEST);
            assert (msg.field24 == msg.FIELD24_TEST);
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
