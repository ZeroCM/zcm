package example.plugins;

import java.io.PrintWriter;

import zcm.logging.CsvWriterPlugin;

import javazcm.types.example_t;
import javazcm.types.example2_t;

public class ExampleCsvWriterPlugin extends CsvWriterPlugin
{
    public Long[] handleFingerprints()
    {
        return new Long[]{ example_t.ZCM_FINGERPRINT };
    }

    public int printCustom(String channel, Object o, long utime, PrintWriter output)
    {
        if (o instanceof example_t) {
            example_t e = (example_t) o;
            output.print(utime);
            output.print(",");
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
        } else if (o instanceof example2_t) {
            output.print(utime);
            output.print(",");
            example2_t e = (example2_t) o;
            output.print(e.timestamp2);
            output.print(",");
            super.printArray(e.position2, output);
            super.printArray(e.orientation2, output);
            output.print(e.num_ranges2);
            output.print(",");
            super.printArray(e.ranges2, output);
            output.print(e.name2);
            output.print(",");
            output.println(e.enabled2);
            return 1;
        }
        return 0;
    }
}
