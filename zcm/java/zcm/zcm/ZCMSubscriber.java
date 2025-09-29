package zcm.zcm;

import java.io.*;

/** A class which listens for messages on a particular channel. **/
public abstract class ZCMSubscriber
{
    /* Overwrite at least one of these functions */

    /**
     * Invoked by ZCM when a message is received.
     *
     * This method is invoked from the ZCM thread.
     *
     * @param zcm the ZCM instance that received the message.
     * @param channel the channel on which the message was received.
     * @param ins the message contents.
     */
     public void messageReceived(ZCM zcm, String channel, ZCMDataInputStream ins)
     {}

     /**
      * Invoked by ZCM when a message is received, with timestamp information.
      *
      * This method is invoked from the ZCM thread. Default implementation
      * calls the legacy messageReceived method for backward compatibility.
      * Override this method to access the receive timestamp.
      *
      * @param zcm the ZCM instance that received the message.
      * @param channel the channel on which the message was received.
      * @param recvUtime the timestamp when the message was received (microseconds since epoch).
      * @param ins the message contents.
      */
    public void messageReceived(ZCM zcm, String channel, long recvUtime, ZCMDataInputStream ins)
    {
        messageReceived(zcm, channel, ins);
    }
}
