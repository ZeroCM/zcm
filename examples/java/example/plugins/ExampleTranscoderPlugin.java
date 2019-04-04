package example.plugins;

import java.io.IOException;
import java.util.ArrayList;

import zcm.logging.TranscoderPlugin;
import zcm.logging.Log;
import zcm.zcm.ZCMDataOutputStream;

import javazcm.types.example_t;
import javazcm.types.example2_t;

public class ExampleTranscoderPlugin extends TranscoderPlugin
{
    private long eventNo = 0;

    public Long[] handleFingerprints()
    {
        return new Long[]{ example_t.ZCM_FINGERPRINT,
                           example2_t.ZCM_FINGERPRINT };
    }

    public ArrayList<Log.Event> transcodeMessage(String channel, Object o, long utime, Log.Event ev)
    {
        if (o instanceof example_t) {
            ZCMDataOutputStream encodeBuffer = new ZCMDataOutputStream();

            example_t e = (example_t) o;
            example2_t e2 = new example2_t();

            e2.timestamp2   = e.timestamp;
            e2.position2    = e.position.clone();
            e2.orientation2 = e.orientation.clone();
            e2.num_ranges2  = e.num_ranges;
            e2.ranges2      = e.ranges.clone();
            e2.name2        = e.name;
            e2.enabled2     = e.enabled;

            try {
                e2.encode(encodeBuffer);
            } catch (IOException ex) {
                System.err.println("Error encoding example2_t");
                ex.printStackTrace();
                return null;
            }

            // construct output
            Log.Event le = new Log.Event();
            le.data = encodeBuffer.toByteArray();
            le.utime = utime;
            le.eventNumber = eventNo++;
            le.channel = channel;

            ArrayList<Log.Event> events = new ArrayList<Log.Event>();
            events.add(le);
            return events;
        }

        return null;
    }
}
