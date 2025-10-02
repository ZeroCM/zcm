package zcm.zcm;

import java.net.*;
import java.io.*;
import java.util.*;
import java.nio.*;

/** Zero Communications and Marshalling Java implementation **/
public class ZCM implements AutoCloseable
{
    public class Subscription
    {
        Object nativeSub;
        ZCMSubscriber javaSub;
    }

    boolean closed = false;

    static ZCM singleton;

    ZCMDataOutputStream encodeBuffer = new ZCMDataOutputStream(new byte[1024]);
    ZCMJNI zcmjni;
    ZCMTransport transport = null;

    /** Create a new ZCM object, connecting to one or more URLs. If
     * no URL is specified, ZCM_DEFAULT_URL is used.
     **/
    public ZCM() throws IOException { this((String)null); }
    public ZCM(String url) throws IOException
    {
        zcmjni = new ZCMJNI(url);
    }

    /** Create a new ZCM object using the provided transport.
     * The transport must be valid and properly initialized.
     **/
    public ZCM(ZCMTransport _transport) throws IOException
    {
        transport = _transport;
        if (transport == null) {
            throw new IllegalArgumentException("Transport cannot be null");
        }
        zcmjni = new ZCMJNI(transport);
    }

    public void start() { zcmjni.start(); }
    public void stop()  { zcmjni.stop(); }

    /** Retrieve a default instance of ZCM using either the environment
     * variable ZCM_DEFAULT_URL or the default. If an exception
     * occurs, System.exit(-1) is called.
     **/
    public static ZCM getSingleton()
    {
        if (singleton == null) {
            try {
                // TODO: add back in capability to use the ZCM_DEFAULT_URL env variable
                //       as the default for getSingleton
                singleton = new ZCM();
            } catch (Exception ex) {
                System.err.println("ZCM singleton fail: "+ex);
                System.exit(-1);
                return null;
            }
        }

        return singleton;
    }

    /** Publish a string on a channel. This method does not use the
     * ZCM type definitions and thus is not type safe. This method is
     * primarily provided for testing purposes and may be removed in
     * the future.
     **/
    public int publish(String channel, String s) throws IOException
    {
        if (this.closed) throw new IllegalStateException();
        s = s + "\0";
        byte[] b = s.getBytes();
        return publish(channel, b, 0, b.length);
    }

    /** Publish an ZCM-defined type on a channel. If more than one URL was
     * specified, the message will be sent on each.
     **/
    public synchronized int publish(String channel, ZCMEncodable e)
    {
        if (this.closed) throw new IllegalStateException();

        try {
            encodeBuffer.reset();

            e.encode(encodeBuffer);

            return publish(channel, encodeBuffer.getBuffer(), 0, encodeBuffer.size());
        } catch (IOException ex) {
            System.err.println("ZCM publish fail: "+ex);
        }
        return -1;
    }

    /** Publish raw data on a channel, bypassing the ZCM type
     * specification. If more than one URL was specified when the ZCM
     * object was created, the message will be sent on each.
     **/
    public int publish(String channel, byte[] data, int offset, int length)
        throws IOException
    {
        if (this.closed) throw new IllegalStateException();
        return zcmjni.publish(channel, data, offset, length);
    }

    public Subscription subscribe(String channel, ZCMSubscriber sub)
    {
        if (this.closed) throw new IllegalStateException();

        Subscription subs = new Subscription();
        subs.javaSub = sub;

        subs.nativeSub = zcmjni.subscribe(channel, this, subs);

        return subs;
    }

    public int unsubscribe(Subscription subs) {
        if (this.closed) throw new IllegalStateException();
        return zcmjni.unsubscribe(subs.nativeSub);
    }

    /** Not for use by end users. Provider back ends call this method
     * when they receive a message. The subscribers that match the
     * channel name are synchronously notified.
     **/
    public void receiveMessage(String channel, long recvUtime,
                               byte data[], int offset, int length,
                               Subscription subs)
    {
        if (this.closed) throw new IllegalStateException();
        subs.javaSub.messageReceived(this, channel, recvUtime,
                                     new ZCMDataInputStream(data, offset, length));
    }

    /** Call this function to release all resources used by the ZCM instance.  After calling this
     * function, the ZCM instance should consume no resources, and cannot be used to
     * receive or transmit messages.
     */
    public void close()
    {
        if (this.closed) throw new IllegalStateException();
        if (transport != null) {
            transport.releaseNativeTransport();
            zcmjni.stop();
        }
        zcmjni.destroy();
        if (transport != null) transport.destroy();
        this.closed = true;
    }

    /** Get the native zcm_t* pointer for use in JNI code.
     * @return native zcm_t* pointer as long, or 0 if not initialized
     */
    public long getNativeZcmPtr()
    {
        if (this.closed) throw new IllegalStateException();
        return zcmjni.getNativeZcmPtr();
    }

    ////////////////////////////////////////////////////////////////

    /** Minimalist test code.
     **/
    public static void main(String args[])
    {
        ZCM zcm;

        try {
            zcm = new ZCM();
        } catch (IOException ex) {
            System.err.println("ex: "+ex);
            return;
        }

        SimpleSubscriber subscriber1 = new SimpleSubscriber();
        SimpleSubscriber subscriber2 = new SimpleSubscriber();

        ZCM.Subscription subs1 = zcm.subscribe(".*", subscriber1);
        ZCM.Subscription subs2 = zcm.subscribe(".*", subscriber2);

        int numMsgsSent = 0;

        zcm.start();
        while (true) {
            if (subscriber1.getNumMsgsReceived() >= 10)
                break;
            try {
                Thread.sleep(250);
                zcm.publish("TEST", "foobar");
                numMsgsSent++;
            } catch (Exception ex) {
                System.err.println("ex: "+ex);
            }
        }
        zcm.stop();

        zcm.unsubscribe(subs1);
        zcm.unsubscribe(subs2);

        zcm.close();
    }

    static class SimpleSubscriber implements ZCMSubscriber
    {
        public int numMsgsReceived = 0;

        public synchronized int getNumMsgsReceived() { return numMsgsReceived; }

        public void messageReceived(ZCM zcm, String channel, long utime,
                                    ZCMDataInputStream dins)
        {
            synchronized (this) {
                numMsgsReceived++;
            }
            System.err.println("RECV: "+channel);
        }
    }
}
