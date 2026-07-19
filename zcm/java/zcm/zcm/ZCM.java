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
        boolean unsubscribed;
    }

    private final Object lifecycleLock = new Object();
    private final Set<Subscription> subscriptions = new HashSet<Subscription>();
    private boolean closing = false;
    private boolean closed = false;
    private int inFlightCallbacks = 0;

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

    public void start()
    {
        synchronized (lifecycleLock) {
            ensureOpen();
            zcmjni.start();
        }
    }

    public void stop()
    {
        synchronized (lifecycleLock) {
            ensureOpen();
            zcmjni.stop();
        }
    }

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
        ensureOpen();
        s = s + "\0";
        byte[] b = s.getBytes();
        return publish(channel, b, 0, b.length);
    }

    /** Publish an ZCM-defined type on a channel. If more than one URL was
     * specified, the message will be sent on each.
     **/
    public synchronized int publish(String channel, ZCMEncodable e)
    {
        ensureOpen();

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
        synchronized (lifecycleLock) {
            ensureOpen();
            return zcmjni.publish(channel, data, offset, length);
        }
    }

    public Subscription subscribe(String channel, ZCMSubscriber sub)
    {
        synchronized (lifecycleLock) {
            ensureOpen();
            Subscription subs = new Subscription();
            subs.javaSub = sub;
            subs.nativeSub = zcmjni.subscribe(channel, this, subs);
            subscriptions.add(subs);
            return subs;
        }
    }

    public int unsubscribe(Subscription subs) {
        synchronized (lifecycleLock) {
            ensureOpen();
            return unsubscribeLocked(subs);
        }
    }

    /** Not for use by end users. Provider back ends call this method
     * when they receive a message. The subscribers that match the
     * channel name are synchronously notified.
     **/
    public void receiveMessage(String channel, long recvUtime,
                               byte data[], int offset, int length,
                               Subscription subs)
    {
        synchronized (lifecycleLock) {
            if (closing || closed || subs.unsubscribed || !subscriptions.contains(subs)) return;
            inFlightCallbacks++;
        }
        try {
            subs.javaSub.messageReceived(this, channel, recvUtime,
                                         new ZCMDataInputStream(data, offset, length));
        } finally {
            synchronized (lifecycleLock) {
                inFlightCallbacks--;
                if (inFlightCallbacks == 0) lifecycleLock.notifyAll();
            }
        }
    }

    /** Call this function to release all resources used by the ZCM instance.  After calling this
     * function, the ZCM instance should consume no resources, and cannot be used to
     * receive or transmit messages.
     */
    public void close()
    {
        Subscription[] subscriptionsToClose;
        synchronized (lifecycleLock) {
            if (closing || closed) throw new IllegalStateException();
            closing = true;
            subscriptionsToClose = subscriptions.toArray(new Subscription[subscriptions.size()]);
        }
        try {
            // stop() joins ZCM dispatch before JNI references are released.
            zcmjni.stop();
            waitForCallbacksToFinish();
            synchronized (lifecycleLock) {
                for (Subscription subscription : subscriptionsToClose) {
                    unsubscribeLocked(subscription);
                }
            }
            if (transport != null) transport.releaseNativeTransport();
            zcmjni.destroy();
            if (transport != null) transport.destroy();
        } finally {
            synchronized (lifecycleLock) {
                subscriptions.clear();
                closed = true;
                closing = false;
                lifecycleLock.notifyAll();
            }
        }
    }

    /** Get the native zcm_t* pointer for use in JNI code.
     * @return native zcm_t* pointer as long, or 0 if not initialized
     */
    public long getNativeZcmPtr()
    {
        synchronized (lifecycleLock) {
            ensureOpen();
            return zcmjni.getNativeZcmPtr();
        }
    }

    private void ensureOpen()
    {
        synchronized (lifecycleLock) {
            if (closing || closed) throw new IllegalStateException();
        }
    }

    private int unsubscribeLocked(Subscription subscription)
    {
        if (subscription == null || subscription.unsubscribed || !subscriptions.contains(subscription)) {
            throw new IllegalArgumentException("Subscription does not belong to this ZCM instance");
        }
        int result = zcmjni.unsubscribe(subscription.nativeSub);
        if (result == 0) {
            subscription.unsubscribed = true;
            subscriptions.remove(subscription);
        }
        return result;
    }

    private void waitForCallbacksToFinish()
    {
        boolean interrupted = false;
        synchronized (lifecycleLock) {
            while (inFlightCallbacks != 0) {
                try {
                    lifecycleLock.wait();
                } catch (InterruptedException exception) {
                    interrupted = true;
                }
            }
        }
        if (interrupted) Thread.currentThread().interrupt();
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
