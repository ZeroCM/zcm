<a href="javascript:history.go(-1)">Back</a>
# Transport Layer

The transport layer is a very important feature for ZCM. With
this subsystem, end-users can craft their own custom transport implementations
to transfer zcm messages. This is acheived with a generic transport interface
that specifies the services a transport must provide and how ZCM will use
a given transport.

## Built-in Transports

With this interface contract, we can implement a wide set of transports.
Many common transport protocols have a ZCM variant built-in to the library.
The following table shows the built-in transports and the URLs that can
be used to *summon* the transport:

<table>
  <thead>
    <tr><th>  Type                </th><th>  URL Format                                              </th></tr>
  </thead>
    <tr><td>  Inter-thread        </td><td>  inproc                                                  </td></tr>
    <tr><td>  Inter-process (IPC) </td><td>  ipc                                                     </td></tr>
    <tr><td>  UDP Multicast       </td><td>  udpm://&lt;udpm-ipaddr&gt;:&lt;port&gt;?ttl=&lt;ttl&gt; </td></tr>
    <tr><td>  Serial              </td><td>  serial://&lt;path-to-device&gt;?baud=&lt;baud&gt; </td></tr>
</table>

Examples:

 - `zcm_create("ipc")`
 - `zcm_create("udpm://239.255.76.67:7667?ttl=0")`
 - `zcm_create("serial:///dev/ttyUSB0?baud=115200")`

When no url is provided to `zcm_create`, the ZCM_DEFAULT_URL environment variable is
queried for a valid url.

## Custom Transports

TODO

<hr>
<a href="javascript:history.go(-1)">Back</a>
