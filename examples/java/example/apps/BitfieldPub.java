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
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 2; ++j) {
                for (int k = 0; k < 2; ++k) {
                    for (int l = 0; l < 2; ++l) {
                        b.field12[i][j][k][l] = (byte)(k + l + 1);
                    }
                }
            }
        }
        b.field15 = 0b1000100;
        b.field16 = 0b0000010;
        b.field18 = -1;
        b.field19 = 0b1000100;
        b.field20 = 0b0000010;
        b.field22 = b.FIELD22_TEST;
        b.field23 = b.FIELD23_TEST;
        b.field24 = b.FIELD24_TEST;
        b.field25 = (byte)0xff;
        b.field26 = (byte)0xff;
        b.field27 = (byte)0xff;
        b.field28 = (byte)0xff;
        b.field29 = (short)0xffff;
        b.field30 = (short)0xffff;
        b.field31 = (int)0xffffffff;
        b.field32 = (int)0xffffffff;
        b.field33 = (long)0xffffffffffffffffL;
        b.field34 = (long)0xffffffffffffffffL;

        assert (b.SIGN_TEST_0  ==  (1L <<  4) - 1);
        assert (b.SIGN_TEST_1  == -(1L <<  4)    );
        assert (b.SIGN_TEST_2  ==  (1L <<  7) - 1);
        assert (b.SIGN_TEST_3  == -(1L <<  7)    );

        assert (b.SIGN_TEST_4  ==  (1L << 13) - 1);
        assert (b.SIGN_TEST_5  == -(1L << 13)    );
        assert (b.SIGN_TEST_6  ==  (1L << 15) - 1);
        assert (b.SIGN_TEST_7  == -(1L << 15)    );

        assert (b.SIGN_TEST_8  ==  (1L << 25) - 1);
        assert (b.SIGN_TEST_9  == -(1L << 25)    );
        assert (b.SIGN_TEST_10 ==  (1L << 31) - 1);
        assert (b.SIGN_TEST_11 == -(1L << 31)    );

        assert (b.SIGN_TEST_12 ==  -1);
        assert (b.SIGN_TEST_13 ==  (1L << 56) - 1);
        assert (b.SIGN_TEST_14 == -(1L << 56)    );
        assert (b.SIGN_TEST_15 ==  (1L << 63) - 1);
        assert (b.SIGN_TEST_16 == -(1L << 63)    );

        assert (b.SIGN_TEST_12 ==  -1);
        assert (b.SIGN_TEST_13 ==  (1L << 56) - 1);
        assert (b.SIGN_TEST_14 == -(1L << 56)    );
        assert (b.SIGN_TEST_15 ==  (1L << 63) - 1);
        assert (b.SIGN_TEST_16 == -(1L << 63)    );

        assert (b.SIGN_TEST_17 == 0x07);
        assert (b.SIGN_TEST_18 == 0x7f);
        assert (b.SIGN_TEST_19 == 0x07);
        assert (b.SIGN_TEST_20 == 0x7f);
        assert (b.SIGN_TEST_21 == 0x0007);
        assert (b.SIGN_TEST_22 == 0x7fff);
        assert (b.SIGN_TEST_23 == 0x00000007);
        assert (b.SIGN_TEST_24 == 0x7fffffff);
        assert (b.SIGN_TEST_25 == 0x0000000000000001L);
        assert (b.SIGN_TEST_26 == 0x0000000000000007L);
        assert (b.SIGN_TEST_27 == 0x7fffffffffffffffL);

        assert (b.SIGN_TEST_28 == 0x7f);
        assert (b.SIGN_TEST_29 == -1);
        assert (b.SIGN_TEST_30 == 0x7f);
        assert (b.SIGN_TEST_31 == -1);
        assert (b.SIGN_TEST_32 == 127);
        assert (b.SIGN_TEST_33 == -1);
        assert (b.SIGN_TEST_34 == 0x7fff);
        assert (b.SIGN_TEST_35 == -1);
        assert (b.SIGN_TEST_36 == 32767);
        assert (b.SIGN_TEST_37 == -1);
        assert (b.SIGN_TEST_38 == 0x7fffffff);
        assert (b.SIGN_TEST_39 == -1);
        assert (b.SIGN_TEST_40 == 2147483647);
        assert (b.SIGN_TEST_41 == -1);
        assert (b.SIGN_TEST_42 == 0x7fffffffffffffffL);
        assert (b.SIGN_TEST_43 == -1);
        assert (b.SIGN_TEST_44 == 9223372036854775807L);
        assert (b.SIGN_TEST_45 == -1);

        while (true) {
            zcm.publish("BITFIELD", b);
            try { Thread.sleep(1000); } catch (InterruptedException ex) {}
        }
    }
}
