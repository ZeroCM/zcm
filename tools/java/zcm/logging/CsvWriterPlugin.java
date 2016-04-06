package zcm.logging;

import java.io.PrintWriter;

public abstract class CsvWriterPlugin
{
    public CsvWriter c;

    public CsvWriterPlugin(CsvWriter c)
    {
        this.c = c;
    }

    // return the array of fingerprints you can handle printing
    public abstract Long[] handleFingerprints();

    // Can call c.printZcmType or c.printArray for any submembers of the
    // zcmtype or types that this class can handle for default printing behavior
    public void printZcmType(Object o, String channel, long utime, PrintWriter output)
    {
        output.print(channel + ",");
        output.print(utime + ",");
    }
}
