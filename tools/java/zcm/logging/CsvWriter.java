package zcm.logging;

import java.io.*;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Type;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import zcm.zcm.*;
import zcm.spy.*;
import zcm.util.*;

import skyspecs.common.util.TimeUtil;

public class CsvWriter implements ZCMSubscriber
{
    private ZCMTypeDatabase handlers;

    private PrintWriter output;
    private Log log;
    private CsvWriterPlugin plugin = null;

    private boolean verbose = false;
    private boolean done = false;

    public CsvWriter(PrintWriter output, Log log, String zcm_url,
                     Constructor pluginCtr, boolean verbose)
    {
        this.log = log;
        this.output = output;
        if (pluginCtr != null) {
            try {
                this.plugin = (CsvWriterPlugin) pluginCtr.newInstance(this);
                System.out.println("Found plugin: " + this.plugin.getClass().getName());
            } catch (Exception ex) {
                System.out.println("ex: " + ex);
                ex.printStackTrace();
                System.exit(1);
            }
        }
        this.verbose = verbose;

        if (this.verbose) System.out.println("Debugging information turned on");

        System.out.println("Searching for zcmtypes in your path...");
        handlers = new ZCMTypeDatabase();

        if (log == null) {
            try {
                new ZCM(zcm_url).subscribeAll(this);
                System.out.println("Generating csv from live zcm data. Press ctrl+C to end.");
            } catch (IOException e) {
                System.err.println("Unable to parse zcm url for input");
                System.exit(1);
            }
        } else {
            System.out.println("Generating csv from logfile");
        }
    }

    private void verbosePrintln(String str)
    {
        if(verbose) System.out.println(str);
    }

    private void verbosePrint(String str)
    {
        if(verbose) System.out.print(str);
    }

    public void messageReceived(ZCM zcm, String channel, ZCMDataInputStream dins)
    {
        verbosePrintln(channel);
        _messageReceived(zcm, channel, TimeUtil.utime(), dins);
    }

    private void _messageReceived(ZCM zcm, String channel, long utime, ZCMDataInputStream dins)
    {
        try {
            int msgSize = dins.available();
            long fingerprint = (msgSize >= 8) ? dins.readLong() : -1;
            dins.reset();

            Class<?> cls = handlers.getClassByFingerprint(fingerprint);

            if (cls == null) {
                verbosePrintln("Unable to find class on channel " + channel
                               + " based on fingerprint " + fingerprint);
                return;
            }
            Object o = cls.getConstructor(DataInput.class).newInstance(dins);

            if (this.plugin != null) {
                this.plugin.printCustom(o, channel, utime, output);
            } else {
                CsvWriterPlugin.printDefault(o, channel, utime, output);
            }

            output.flush();

        } catch (Exception e) {
            System.err.println("Encountered error while decoding " + channel + " zcm type");
            e.printStackTrace();
        }
    }

    public void run()
    {
        Runtime.getRuntime().addShutdownHook(new Thread() {
            public void run() {
                done = true;
                System.out.println("\rCleaning up and quitting ");
            }
        });
        if(log == null) {
            while (!done) try { Thread.sleep(1000); } catch(Exception e){}
        } else {
            long lastPrintTime = 0;
            while (!done) {
                try {
                    Log.Event ev = log.readNext();
                    Double percent = log.getPositionFraction() * 100;
                    long now = TimeUtil.utime();
                    if (lastPrintTime + 5e5 < now) {
                        lastPrintTime = now;
                        System.out.print("\rProgress: " + String.format("%.2f", percent) + "%");
                        System.out.flush();
                    }
                    _messageReceived(null, ev.channel, ev.utime,
                                     new ZCMDataInputStream(ev.data, 0,
                                                            ev.data.length));
                } catch (EOFException ex) {
                    System.out.println("\rProgress: 100%        ");
                    done = true;
                } catch (IOException e) {
                    System.err.println("Unable to decode zcmtype entry in log file");
                }
            }
        }
    }

    public static class PluginClassVisitor implements ClassDiscoverer.ClassVisitor
    {
        public HashMap<String, Constructor> plugins = new HashMap<String, Constructor>();
        private ZCMTypeDatabase handlers = null;

        public PluginClassVisitor()
        {
            ClassDiscoverer.findClasses(this);
        }

        public void classFound(String jar, Class cls)
        {
            Class<?> c = cls;
            if (!c.equals(CsvWriterPlugin.class) && CsvWriterPlugin.class.isAssignableFrom(c)) {
                try {
                    Constructor ctr = c.getConstructor(CsvWriter.class);
                    CsvWriterPlugin plugin = (CsvWriterPlugin) ctr.newInstance((CsvWriter)null);
                    plugins.put(plugin.getClass().getName(), ctr);
                } catch (Exception ex) {
                    System.out.println("ex: " + ex);
                    ex.printStackTrace();
                }
            }
        }

        public void print()
        {
            if (plugins.keySet().size() == 0) {
                System.err.println("No plugins found");
                return;
            }

            if (handlers == null)
                handlers = new ZCMTypeDatabase();

            for (String name : plugins.keySet()) {
                System.out.println("Found " + name + " that handles:");
                Constructor ctr = plugins.get(name);
                try {
                    CsvWriterPlugin plugin = (CsvWriterPlugin) ctr.newInstance((CsvWriter)null);
                    for (Long l : plugin.handleFingerprints()) {
                        Class<?> cls = null;
                        try {
                            cls = handlers.getClassByFingerprint(l);
                            System.out.println("\t " + cls.getName());
                        } catch (Exception e) {
                            System.out.println("\t fingerprint: " + l);
                        }
                    }
                } catch (Exception ex) {
                    System.out.println("ex: " + ex);
                    ex.printStackTrace();
                }
            }
        }
    }

    public static void usage()
    {
        System.out.println("usage: zcm-csv-writer [options]");
        System.out.println("");
        System.out.println("    Listens to zcm message traffic on the default interface");
        System.out.println("    and converts the data into a csv file. Check out the");
        System.out.println("    CsvPlugin interface to make custom csv output formats");
        System.out.println("");
        System.out.println("Options:");
        System.out.println("");
        System.err.println("  -u, --zcm-url=URL           Use the specified ZCM URL");
        System.out.println("                              Please provide only one of -u/-l");
        System.out.println("  -l, --log=<logfile.log>     Run on log file.");
        System.out.println("                              Please provide only one of -l/-u");
        System.out.println("  -o, --output=<output.csv>   Output csv to file");
        System.out.println("  -p, --plugin=<class.name>   Use csv writer plugin");
        System.out.println("      --list-plugins          List available csv writer plugins");
        System.out.println("  -h, --help                  Display this help message");
    }

    public static void main(String args[])
    {
        String zcm_url     = null;
        String logfile     = null;
        String outfile     = null;
        String pluginName  = null;

        boolean list_plugins = false;
        boolean verbose = false;

        int i;
        for (i = 0; i < args.length; ++i) {
            String toks[] = args[i].split("=");
            String arg = toks[0];
            if (arg.equals("-u") || arg.equals("--zcm-url")) {
                String param = null;
                if (toks.length == 2) param = toks[1];
                else if (i + 1 < args.length) param = args[++i];
                if (param == null) {
                    System.out.println(toks[0] + " requires an argument");
                    usage();
                    System.exit(1);
                }
                zcm_url = param;
            } else if (arg.equals("-l") || arg.equals("--log")) {
                String param = null;
                if (toks.length == 2) param = toks[1];
                else if (i + 1 < args.length) param = args[++i];
                if (param == null) {
                    System.out.println(toks[0] + " requires an argument");
                    usage();
                    System.exit(1);
                }
                logfile = param;
            } else if (arg.equals("-o") || arg.equals("--output")) {
                String param = null;
                if (toks.length == 2) param = toks[1];
                else if (i + 1 < args.length) param = args[++i];
                if (param == null) {
                    System.out.println(toks[0] + " requires an argument");
                    usage();
                    System.exit(1);
                }
                outfile = param;
            } else if (arg.equals("-p") || arg.equals("--plugin")) {
                String param = null;
                if (toks.length == 2) param = toks[1];
                else if (i + 1 < args.length) param = args[++i];
                if (param == null) {
                    System.out.println(toks[0] + " requires an argument");
                    usage();
                    System.exit(1);
                }
                pluginName = param;
            } else if (arg.equals("--list-plugins")) {
                list_plugins = true;
            } else if (arg.equals("-v") || arg.equals("--verbose")) {
                verbose = true;
            } else if (arg.equals("-h") || arg.equals("--help")) {
                usage();
                System.exit(0);
            } else {
                System.err.println("Unrecognized command line option: '" + arg + "'");
                usage();
                System.exit(1);
            }
        }

        if (list_plugins) {
            System.out.println("Searching path for plugins");
            PluginClassVisitor pcv = new PluginClassVisitor();
            pcv.print();
            System.exit(0);
        }

        Constructor pluginCtr = null;
        if (pluginName != null) {
            try {
                Class c = Class.forName(pluginName);
                pluginCtr = c.getConstructor();
                if (!CsvReaderPlugin.class.isAssignableFrom(c)) {
                    System.err.println("Invalid plugin. Exiting.");
                    System.exit(1);
                }
            } catch (Exception e) {
                e.printStackTrace();
                System.err.println("Unable to find specified plugin");
                System.exit(1);
            }
        }

        // Check input getopt
        if (zcm_url != null && logfile != null) {
            System.err.println("Please specify only one source of zcm data");
            usage();
            System.exit(1);
        }

        // Check output getopt
        if (outfile == null) {
            System.err.println("Please specify output file");
            usage();
            System.exit(1);
        }

        // Setup input file
        Log input = null;
        if (logfile != null) {
            try {
                input = new Log(logfile, "r");
            } catch(Exception e) {
                System.err.println("Unable to open " + logfile);
                System.exit(1);
            }
        }

        // Setup output file.
        PrintWriter output = null;
        try {
            output = new PrintWriter(outfile, "UTF-8");
        } catch(Exception e) {
            System.err.println("Unable to open " + outfile);
            System.exit(1);
        }

        new CsvWriter(output, input, zcm_url, pluginCtr, verbose).run();
    }
}
