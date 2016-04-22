package zcm.logging;

import java.io.*;
import java.lang.reflect.Constructor;
import java.util.ArrayList;
import java.util.HashMap;

import zcm.zcm.*;
import zcm.spy.*;
import zcm.util.*;

public class Transcoder
{
    private ZCMTypeDatabase handlers;

    private Log input;
    private Log output;
    private TranscoderPlugin plugin = null;

    private boolean verbose = false;
    private boolean done = false;

    public Transcoder(Log output, Log log, Constructor pluginCtr, boolean verbose)
    {
        this.input  = input;
        this.output = output;
        try {
            this.plugin = (TranscoderPlugin) pluginCtr.newInstance();
            System.out.println("Found plugin: " + this.plugin.getClass().getName());
        } catch (Exception ex) {
            System.out.println("ex: " + ex);
            ex.printStackTrace();
            System.exit(1);
        }
        this.verbose = verbose;

        if (this.verbose) System.out.println("Debugging information turned on");

        System.out.println("Searching for zcmtypes in your path...");
        handlers = new ZCMTypeDatabase();
        if (this.verbose) handlers.print();
    }

    private void verbosePrintln(String str)
    {
        if(verbose) System.out.println(str);
    }

    private void verbosePrint(String str)
    {
        if(verbose) System.out.print(str);
    }

    private void messageReceived(ZCM zcm, String channel, long utime, ZCMDataInputStream dins)
    {
        try {
            int msgSize = dins.available();
            long fingerprint = (msgSize >= 8) ? dins.readLong() : -1;
            dins.reset();

            Class<?> cls = handlers.getClassByFingerprint(fingerprint);

            if (cls == null) {
                verbosePrintln("Unable to find class on channel " + channel
                               + " based on fingerprint " +
                               Long.toHexString(fingerprint));
                return;
            }
            Object o = cls.getConstructor(DataInput.class).newInstance(dins);

            ArrayList<Log.Event> events;
            events = this.plugin.transcodeMessage(channel, o, utime);
            if (events == null || events.size() == 0) return;
            for (int i = 0; i < events.size(); ++i)
                output.write(events.get(i));

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
                System.out.println("\b\b  ");
                System.out.println("Cleaning up and quitting");
            }
        });

        System.out.println("Transcoding logfile");

        long lastPrintTime = 0;
        while (!done) {
            try {
                Log.Event ev = input.readNext();
                Double percent = input.getPositionFraction() * 100;
                long now = System.currentTimeMillis() * 1000;
                if (lastPrintTime + 5e5 < now) {
                    lastPrintTime = now;
                    System.out.print("\rProgress: " + String.format("%.2f", percent) + "%");
                    System.out.flush();
                }
                messageReceived(null, ev.channel, ev.utime,
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
            if (!c.equals(TranscoderPlugin.class) && TranscoderPlugin.class.isAssignableFrom(c)) {
                try {
                    Constructor ctr = c.getConstructor();
                    TranscoderPlugin plugin = (TranscoderPlugin) ctr.newInstance();
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
                    TranscoderPlugin plugin = (TranscoderPlugin) ctr.newInstance();
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
        System.out.println("usage: zcm-log-transcoder [options]");
        System.out.println("");
        System.out.println("    Reads zcm traffic out of a log and transcodes");
        System.out.println("    the data via a Transcoder plugin. Check out the");
        System.out.println("    TranscoderPlugin interface to make a plugin that");
        System.out.println("    runs with this program");
        System.out.println("");
        System.out.println("Options:");
        System.out.println("");
        System.out.println("  -i, --input=<logfile.log>   Input log file.");
        System.out.println("  -o, --output=<output.log>   Output log file");
        System.out.println("  -p, --plugin=<class.name>   Transcoder plugin to use");
        System.out.println("      --list-plugins          List available transcoder plugins");
        System.out.println("  -h, --help                  Display this help message");
    }

    public static void main(String args[])
    {
        String infile      = null;
        String outfile     = null;
        String pluginName  = null;

        boolean list_plugins = false;
        boolean verbose = false;

        int i;
        for (i = 0; i < args.length; ++i) {
            String toks[] = args[i].split("=");
            String arg = toks[0];
            if (arg.equals("-i") || arg.equals("--input")) {
                String param = null;
                if (toks.length == 2) param = toks[1];
                else if (i + 1 < args.length) param = args[++i];
                if (param == null) {
                    System.out.println(toks[0] + " requires an argument");
                    usage();
                    System.exit(1);
                }
                infile = param;
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

        // Check output getopt
        if (infile == null) {
            System.err.println("Please specify input log file");
            usage();
            System.exit(1);
        }

        // Check output getopt
        if (outfile == null) {
            System.err.println("Please specify output log file");
            usage();
            System.exit(1);
        }

        if (pluginName == null) {
            System.err.println("Please specify transcoder plugin");
            usage();
            System.exit(1);
        }

        System.out.println("Searching path for plugins");
        PluginClassVisitor pcv = new PluginClassVisitor();
        Constructor pluginCtr = pcv.plugins.get(pluginName);
        if (pluginCtr == null) {
            System.err.println("Unable to find specified plugin");
            System.exit(1);
        }

        // Setup input file
        Log input = null;
        if (infile != null) {
            try {
                input = new Log(infile, "r");
            } catch(Exception e) {
                System.err.println("Unable to open " + infile);
                System.exit(1);
            }
        }

        // Setup output file.
        Log output = null;
        if (outfile != null) {
            try {
                output = new Log(outfile, "r");
            } catch(Exception e) {
                System.err.println("Unable to open " + outfile);
                System.exit(1);
            }
        }

        new Transcoder(output, input, pluginCtr, verbose).run();
    }
}
