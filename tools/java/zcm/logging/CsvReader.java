package zcm.logging;

import java.io.*;
import java.lang.reflect.Constructor;
import java.util.HashMap;
import java.util.ArrayList;

import zcm.zcm.*;
import zcm.util.*;

public class CsvReader
{
    private BufferedReader input;
    private long inputSize = 0;
    private Log outputLog = null;
    private ZCM outputZcm = null;
    private CsvReaderPlugin plugin;

    private int numEventsRead = 0;

    private boolean verbose = false;
    private boolean done = false;

    // RRR (Tom) looks like there's a decent amount of code for verbose printing,
    // but only one print out actually uses it. Any reason we need to be so "verbose"? :)
    // RRR (Bendes) Left it here so we could add stuff as we needed
    //
    // RRR (Tom) I donno how I feel about requiring the user to pass in "inputSize",
    // when its only purpose is for printing completion percentage. Is the completion
    // percentage able to be calculated just from members of the BufferedReader?
    // Not sure if a file size and/or file position are accessible.
    // RRR (Bendes) Unfortunately not (at least i couldn't find a way). I don't
    //              like it either but I dont have a good way of doing it
    //              without moving the fileIO into the constructor (which i
    //              think is worse than this)
    public CsvReader(Log outputLog, String zcm_url,
                     BufferedReader input, long inputSize,
                     Constructor pluginCtor, boolean verbose)
    {
        this.input = input;
        this.inputSize = inputSize;
        this.outputLog = outputLog;
        if (pluginCtor != null) {
            try {
                plugin = (CsvReaderPlugin) pluginCtor.newInstance();
                System.out.println("Found plugin: " + plugin.getClass().getName());
            } catch (Exception ex) {
                System.out.println("ex: " + ex);
                ex.printStackTrace();
                System.exit(1);
            }
        }
        this.verbose = verbose;

        if (this.verbose) System.out.println("Debugging information turned on");

        if (outputLog == null) {
            try {
                outputZcm = new ZCM(zcm_url);
                System.out.println("Outputting messages over zcm");
            } catch (IOException e) {
                System.err.println("Unable to parse zcm url output");
                System.exit(1);
            }
        } else {
            System.out.println("Generating logfile from csv");
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

    public void run()
    {
        Runtime.getRuntime().addShutdownHook(new Thread() {
            public void run() {
                System.out.print("\b\b  ");
                try {
                    if (outputLog != null)
                        outputLog.close();
                } catch (IOException e) {
                    System.err.println("Unable to close output file");
                }
                System.out.println("Read " + numEventsRead + " events");
                System.out.println("Cleaning up and quitting");
            }
        });
        long lastPrintTime = 0;
        long bytesRead = 0;
        while (!done) {
            try {
                String line = input.readLine();
                if (line == null) {
                    System.out.println("\rProgress: 100%        ");
                    break;
                }
                ArrayList<Log.Event> events = plugin.readZcmType(line);
                // RRR (Tom) do we want to fail loudly here? It's not clear to
                // me what we're handling here, if not an error in the plugin?
                // RRR (Bendes) the plugin need not request an event be written: no error here
                //              Delete these comments if that answer is acceptable
                if (events == null || events.size() == 0) continue;
                numEventsRead += events.size();

                // RRR (Tom) seems like both outputLog & outputZcm not being null
                // should be a requirement we enforce in the constructor, not in
                // the run function.
                // RRR (Bendes) Only one of them need not be null
                //              Delete these comments if that answer is acceptable
                if (outputLog != null) {
                    for (Log.Event e : events)
                        outputLog.write(e);
                }
                if (outputZcm != null) {
                    for (Log.Event e : events)
                        outputZcm.publish(e.channel, e.data, 0, e.data.length);
                }
                bytesRead += line.length();
                double percent = (double) bytesRead / (double) this.inputSize * 100;
                long now = System.currentTimeMillis() * 1000;
                if (lastPrintTime + 5e5 < now) {
                    lastPrintTime = now;
                    System.out.print("\rProgress: " + String.format("%.2f", percent) + "%");
                    System.out.flush();
                }
            } catch (IOException e) {
                System.err.println("Unable to read line from csv");
            }
        }
    }

    public static class PluginClassVisitor implements ClassDiscoverer.ClassVisitor
    {
        public HashMap<String, Constructor> plugins = new HashMap<String, Constructor>();

        public PluginClassVisitor()
        {
            ClassDiscoverer.findClasses(this);
        }

        public void classFound(String jar, Class cls)
        {
            Class<?> c = cls;
            if (!c.equals(CsvReaderPlugin.class) && CsvReaderPlugin.class.isAssignableFrom(c)) {
                try {
                    Constructor ctr = c.getConstructor();
                    CsvReaderPlugin plugin = (CsvReaderPlugin) ctr.newInstance();
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

            for (String name : plugins.keySet())
                System.out.println("Found " + name);
        }
    }

    public static void usage()
    {
        System.out.println("usage: zcm-csv-reader [options]");
        System.out.println("");
        System.out.println("    Reads data from a csv file and converts the data into");
        System.out.println("    ZCM messages. ZCM messages are then either broadcast");
        System.out.println("    over a zcm transport or written to a logfile.");
        System.out.println("");
        System.out.println("Options:");
        System.out.println("");
        System.out.println("  -c, --csv=<log.csv>         Input csv file.");
        System.err.println("  -u, --zcm-url=URL           Output traffic over the specified ZCM URL");
        System.out.println("                              Please provide only one of -u/-l");
        System.out.println("  -o, --output=<output.log>   Output traffic to log file");
        System.out.println("  -p, --plugin=<class.name>   Use csv reader plugin");
        System.out.println("      --list-plugins          List available csv reader plugins");
        System.out.println("  -h, --help                  Display this help message");
    }

    public static void main(String args[])
    {
        String zcm_url     = null;
        String csvfile     = null;
        String logfile     = null;
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
            } else if (arg.equals("-c") || arg.equals("--csv")) {
                String param = null;
                if (toks.length == 2) param = toks[1];
                else if (i + 1 < args.length) param = args[++i];
                if (param == null) {
                    System.out.println(toks[0] + " requires an argument");
                    usage();
                    System.exit(1);
                }
                csvfile = param;
            } else if (arg.equals("-o") || arg.equals("--output")) {
                String param = null;
                if (toks.length == 2) param = toks[1];
                else if (i + 1 < args.length) param = args[++i];
                if (param == null) {
                    System.out.println(toks[0] + " requires an argument");
                    usage();
                    System.exit(1);
                }
                logfile = param;
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

        // Check input getopt
        if (csvfile == null) {
            System.err.println("Please specify an input csv file");
            usage();
            System.exit(1);
        }

        // Check output getopt
        if (zcm_url == null && logfile == null) {
            System.err.println("Please specify at least one source of zcm data");
            usage();
            System.exit(1);
        }

        Constructor pluginCtor = null;
        if (pluginName != null) {
            System.out.println("Searching path for plugins");
            PluginClassVisitor pcv = new PluginClassVisitor();
            pluginCtor = pcv.plugins.get(pluginName);
            if (pluginCtor == null) {
                System.err.println("Unable to find specified plugin");
                System.exit(1);
            }
        }

        // Setup input file
        BufferedReader input = null;
        long fileSize = 0;
        try {
            File inputFile = new File(csvfile);
            fileSize = inputFile.length();
            input = new BufferedReader(new FileReader(inputFile));
        } catch(Exception e) {
            System.err.println("Unable to open " + csvfile);
            System.exit(1);
        }

        // Setup output file.
        Log output = null;
        if (logfile != null) {
            try {
                output = new Log(logfile, "rw");
            } catch(Exception e) {
                System.err.println("Unable to open " + logfile);
                System.exit(1);
            }
        }

        new CsvReader(output, zcm_url, input, fileSize, pluginCtor, verbose).run();
    }
}
