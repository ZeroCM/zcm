package example.plugins;

import java.io.IOException;
import java.util.ArrayList;

import zcm.logging.CsvReaderPlugin;
import zcm.logging.Log;
import zcm.zcm.ZCMDataOutputStream;

import javazcm.types.example_t;

public class ExampleCsvReaderPlugin extends CsvReaderPlugin
{
    private long eventNo = 0;

    private enum Fields {
        LOG_UTIME,
        TIMESTAMP,
        POS_X,
        POS_Y,
        POS_Z,
        QUAT_W,
        QUAT_X,
        QUAT_Y,
        QUAT_Z,
        NUM_RANGES,
        RANGES,
        NAME,
        ENABLED,
        NUM_FIELDS
    }

    public ArrayList<Log.Event> readZcmType(String line)
    {
        String arr[] = line.split(",");

        example_t e = new example_t();

        long logEventUtime = Long.parseLong(arr[Fields.LOG_UTIME.ordinal()]);

        e.timestamp   = Long.parseLong(arr[Fields.TIMESTAMP.ordinal()]);

        e.position[0] = Double.parseDouble(arr[Fields.POS_X.ordinal()]);
        e.position[1] = Double.parseDouble(arr[Fields.POS_Y.ordinal()]);
        e.position[2] = Double.parseDouble(arr[Fields.POS_Z.ordinal()]);

        e.orientation[0] = Double.parseDouble(arr[Fields.QUAT_W.ordinal()]);
        e.orientation[1] = Double.parseDouble(arr[Fields.QUAT_X.ordinal()]);
        e.orientation[2] = Double.parseDouble(arr[Fields.QUAT_Y.ordinal()]);
        e.orientation[3] = Double.parseDouble(arr[Fields.QUAT_Z.ordinal()]);

        e.num_ranges  = Integer.parseInt(arr[Fields.NUM_RANGES.ordinal()]);
        e.ranges = new short[e.num_ranges];
        for (int i = 0; i < e.num_ranges; ++i)
            e.ranges[i] = Short.parseShort(arr[Fields.RANGES.ordinal() + i]);

        e.name = arr[Fields.NAME.ordinal() + e.num_ranges - 1];
        e.enabled = Boolean.parseBoolean(arr[Fields.ENABLED.ordinal() + e.num_ranges - 1]);

        ZCMDataOutputStream encodeBuffer = new ZCMDataOutputStream();
        try {
            e.encode(encodeBuffer);
        } catch (IOException ex) {
            System.err.println("Error encoding example2_t");
            ex.printStackTrace();
            return null;
        }

        // construct output
        Log.Event le = new Log.Event();
        le.data = encodeBuffer.toByteArray();
        le.utime = logEventUtime;
        le.eventNumber = eventNo++;
        le.channel = "EXAMPLE";

        ArrayList<Log.Event> events = new ArrayList<Log.Event>();
        events.add(le);
        return events;
    }
}
