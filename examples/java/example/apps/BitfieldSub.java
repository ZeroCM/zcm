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
            assert (msg.field11 == 3);
            for (int i = 0; i < 2; ++i) {
                for (int j = 0; j < 2; ++j) {
                    for (int k = 0; k < 2; ++k) {
                        for (int l = 0; l < 2; ++l) {
                            assert (msg.field12[i][j][k][l] == k + l + 1);
                        }
                    }
                }
            }
            assert (msg.field15 == -60);
            assert (msg.field16 == 2);
            assert (msg.field18 == 15);
            assert (msg.field19 == 68);
            assert (msg.field20 == 2);
            assert (msg.field22 == msg.FIELD22_TEST);
            assert (msg.field23 == msg.FIELD23_TEST);
            assert (msg.field24 == msg.FIELD24_TEST);
            assert (msg.field25 == 3);
            assert (msg.field26 == (byte)0xff);
            assert (msg.field27 == 3);
            assert (msg.field28 == 0x7f);
            assert (msg.field29 == 3);
            assert (msg.field30 == 0x7fff);
            assert (msg.field31 == 0xf);
            assert (msg.field32 == 0x7fffffff);
            assert (msg.field33 == 0xf);
            assert (msg.field34 == 0x7fffffffffffffffL);
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
