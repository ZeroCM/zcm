package example.apps;

import zcm.zcm.*;
import javazcm.types.bitfield_t;

public class BitfieldPub
{
    public static void main(String[] args)
    {
        ZCM zcm = ZCM.getSingleton();

        bitfield_t b = new bitfield_t();
        b.field1 = 3;
        b.field2[0][0] = 1;
        b.field2[0][1] = 0;
        b.field2[0][2] = 1;
        b.field2[0][3] = 0;
        b.field2[1][0] = 1;
        b.field2[1][1] = 0;
        b.field2[1][2] = 1;
        b.field2[1][3] = 0;
        b.field3 = 0xf;
        b.field4 = 5;
        b.field5 = 7;
        b.field6 = 1;
        b.field7 = 3;
        b.field9 = 1 << 27;
        b.field10 = ((long)1 << 52) | 1;
        b.field11 = 3;
        byte[][][][] field12 = { { { { 1, 2 }, { -1, 2 } }, { { 1, -2 }, { 1, 2 } } }, { { { 1, 2 }, { 1, 2 } }, { { 1, 2 }, { 1, 2 } } }, { { { 1, 2 }, { 1, 2 } }, { { 1, 2 }, { 1, 2 } } } };
        b.field12 = field12;

        while (true) {
            zcm.publish("BITFIELD", b);
            try { Thread.sleep(1000); } catch (InterruptedException ex) {}
        }
    }
}
