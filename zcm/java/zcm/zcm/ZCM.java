package zcm.zcm;

import java.net.*;
import java.io.*;
import java.util.*;
import java.util.regex.*;
import java.nio.*;

/** Zero Communications and Marshalling Java implementation **/
public class ZCM
{
    static class SubscriptionRecord
    {
        String  regex;
        Pattern pat;
        ZCMSubscriber lcsub;
    }

    ArrayList<SubscriptionRecord> subscriptions = new ArrayList<SubscriptionRecord>();
    ArrayList<Provider> providers = new ArrayList<Provider>();

    HashMap<String,ArrayList<SubscriptionRecord>> subscriptionsMap = new HashMap<String,ArrayList<SubscriptionRecord>>();

    boolean closed = false;

    static ZCM singleton;

    ZCMDataOutputStream encodeBuffer = new ZCMDataOutputStream(new byte[1024]);
    ZCMJNI zcmjni;

    /** Create a new ZCM object, connecting to one or more URLs. If
     * no URL is specified, "ipc" is used.
     **/
    public ZCM() throws IOException { this("ipc"); }
    public ZCM(String url) throws IOException
    {
        zcmjni = new ZCMJNI(url);
    }

    /** Retrieve a default instance of ZCM using either the environment
     * variable ZCM_DEFAULT_URL or the default. If an exception
     * occurs, System.exit(-1) is called.
     **/
    public static ZCM getSingleton()
    {
        if (singleton == null) {
            try {
                singleton = new ZCM();
            } catch (Exception ex) {
                System.err.println("ZCM singleton fail: "+ex);
                System.exit(-1);
                return null;
            }
        }

        return singleton;
    }

    /** Return the number of subscriptions. **/
    public int getNumSubscriptions()
    {
        if (this.closed) throw new IllegalStateException();
        return subscriptions.size();
    }

    /** Publish a string on a channel. This method does not use the
     * ZCM type definitions and thus is not type safe. This method is
     * primarily provided for testing purposes and may be removed in
     * the future.
     **/
    public void publish(String channel, String s) throws IOException
    {
        if (this.closed) throw new IllegalStateException();
        s=s+"\0";
        byte[] b = s.getBytes();
        publish(channel, b, 0, b.length);
    }

    /** Publish an ZCM-defined type on a channel. If more than one URL was
     * specified, the message will be sent on each.
     **/
    public synchronized void publish(String channel, ZCMEncodable e)
    {
        if (this.closed) throw new IllegalStateException();

        try {
            encodeBuffer.reset();

            e.encode(encodeBuffer);

            publish(channel, encodeBuffer.getBuffer(), 0, encodeBuffer.size());
        } catch (IOException ex) {
            System.err.println("ZCM publish fail: "+ex);
        }
    }

    /** Publish raw data on a channel, bypassing the ZCM type
     * specification. If more than one URL was specified when the ZCM
     * object was created, the message will be sent on each.
     **/
    public synchronized void publish(String channel, byte[] data, int offset, int length)
        throws IOException
    {
        if (this.closed) throw new IllegalStateException();
        zcmjni.publish(channel, data, offset, length);
    }

    /** Subscribe to all channels whose name matches the regular
     * expression. Note that to subscribe to all channels, you must
     * specify ".*", not "*".
     **/
    public void subscribe(String regex, ZCMSubscriber sub)
    {
        if (this.closed) throw new IllegalStateException();
        SubscriptionRecord srec = new SubscriptionRecord();
        srec.regex = regex;
        srec.pat = Pattern.compile(regex);
        srec.lcsub = sub;

        synchronized(this) {
            zcmjni.subscribe(regex, this);
        }

        synchronized(subscriptions) {
            subscriptions.add(srec);

            for (String channel : subscriptionsMap.keySet()) {
                if (srec.pat.matcher(channel).matches()) {
                    ArrayList<SubscriptionRecord> subs = subscriptionsMap.get(channel);
                    subs.add(srec);
                }
            }
        }
    }

    /** Remove this particular regex/subscriber pair (UNTESTED AND API
     * MAY CHANGE). If regex is null, all subscriptions for 'sub' are
     * cancelled. If subscriber is null, any previous subscriptions
     * matching the regular expression will be cancelled. If both
     * 'sub' and 'regex' are null, all subscriptions will be
     * cancelled.
     **/
    public void unsubscribe(String regex, ZCMSubscriber sub) {
        if (this.closed) throw new IllegalStateException();

        synchronized(this) {
            for (Provider p : providers)
                p.unsubscribe (regex);
        }

        // TODO: providers don't seem to use anything beyond first channel

        synchronized(subscriptions) {

            // Find and remove subscriber from list
            for (Iterator<SubscriptionRecord> it = subscriptions.iterator(); it.hasNext(); ) {
                SubscriptionRecord sr = it.next();

                if ((sub == null || sr.lcsub == sub) &&
                    (regex == null || sr.regex.equals(regex))) {
                    it.remove();
                }
            }

            // Find and remove subscriber from map
            for (String channel : subscriptionsMap.keySet()) {
                for (Iterator<SubscriptionRecord> it = subscriptionsMap.get(channel).iterator(); it.hasNext(); ) {
                    SubscriptionRecord sr = it.next();

                    if ((sub == null || sr.lcsub == sub) &&
                        (regex == null || sr.regex.equals(regex))) {
                        it.remove();
                    }
                }
            }
        }
    }

    /** Not for use by end users. Provider back ends call this method
     * when they receive a message. The subscribers that match the
     * channel name are synchronously notified.
     **/
    public void receiveMessage(String channel, byte data[], int offset, int length)
    {
        if (this.closed) throw new IllegalStateException();
        synchronized (subscriptions) {
            ArrayList<SubscriptionRecord> srecs = subscriptionsMap.get(channel);

            if (srecs == null) {
                // must build this list!
                srecs = new ArrayList<SubscriptionRecord>();
                subscriptionsMap.put(channel, srecs);

                for (SubscriptionRecord srec : subscriptions) {
                    if (srec.pat.matcher(channel).matches())
                        srecs.add(srec);
                }
            }

            for (SubscriptionRecord srec : srecs) {
                srec.lcsub.messageReceived(this,
                                           channel,
                                           new ZCMDataInputStream(data, offset, length));
            }
        }
    }

    /** A convenience function that subscribes to all ZCM channels. **/
    public synchronized void subscribeAll(ZCMSubscriber sub)
    {
        subscribe(".*", sub);
    }

    /** Call this function to release all resources used by the ZCM instance.  After calling this
     * function, the ZCM instance should consume no resources, and cannot be used to
     * receive or transmit messages.
     */
    public synchronized void close()
    {
        if (this.closed) throw new IllegalStateException();
        for (Provider p : providers) {
            p.close();
        }
        providers = null;
        this.closed = true;
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

        zcm.subscribe(".*", new SimpleSubscriber());

        while (true) {
            try {
                Thread.sleep(100);
                zcm.publish("TEST", "foobar");
            } catch (Exception ex) {
                System.err.println("ex: "+ex);
            }
        }
    }

    static class SimpleSubscriber implements ZCMSubscriber
    {
        public void messageReceived(ZCM zcm, String channel, ZCMDataInputStream dins)
        {
            System.err.println("RECV: "+channel);
        }
    }
}
