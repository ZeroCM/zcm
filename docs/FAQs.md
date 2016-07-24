<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# Frequently Asked Questions

### In trying to run the publish test project from the tutorial, i get the error `./publish: error while loading shared libraries: libzcm.so: cannot open shared object file: No such file or directory`

If you see this error when trying to use anything zcm related, then you probably have a problem with your LD\_LIBRARY\_PATH environment variable. Try echoing $LD\_LIBRARY\_PATH and verifying that it points to `/usr/lib:/usr/local/lib`. If it doesn't (which is weird), point it there yourself with:

    export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib

Add that line to the bottom of your ~/.bashrc to make it permanent.



### I'm trying to run my program and ZCM's not working and I don't know why

Have you tried running your program with `ZCM_DEBUG=1` in front of it? This enables debug output from zcm which should help you diagnose your problems



### Why does my program freeze when i try to subscribe / unsubscribe from within a callback?

Calling the subscribe / unsubscribe functions modifies the same internal data structures as
the dispatch code reads during callbacks. Due to a lock, calling these functions from within a
callback causes a deadlock. You should therefore alter the subscriptions outside of your callbacks.
If your code really needs to change subscriptions in response to a received message, try using a queue
to pass the work to another thread or in your main-loop if using `zcm_handle`.
