package example.plugins;

import java.io.PrintWriter;

import zcm.logging.CsvWriterPlugin;

import example.zcmtypes.example_t;

public class ExampleCsvWriterPlugin extends CsvWriterPlugin
{
    private long eventNo = 0;

    public Long[] handleFingerprints()
    {
        return new Long[]{ example_t.ZCM_FINGERPRINT };
    }

    public int printCustom(String channel, Object o, long utime, PrintWriter output)
    {
        if (o instanceof example_t) {
            example_t e = (example_t) o;
            output.print(e.timestamp);
            output.print(",");
            super.printArray(e.position, output);
            super.printArray(e.orientation, output);
            output.print(e.num_ranges);
            output.print(",");
            super.printArray(e.ranges, output);
            output.print(e.name);
            output.print(",");
            output.println(e.enabled);
            return 1;
        }
        return 0;
    }
}
