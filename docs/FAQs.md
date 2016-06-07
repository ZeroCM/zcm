<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# Frequently Asked Questions

### In trying to run the publish test project from the tutorial, i get the error `./publish: error while loading shared libraries: libzcm.so: cannot open shared object file: No such file or directory`

If you see this error when trying to use anything zcm related, then you probably have a problem with your LD\_LIBRARY\_PATH environment variable. Try echoing $LD\_LIBRARY\_PATH and verifying that it points to `/usr/lib:/usr/local/lib`. If it doesn't (which is weird), point it there yourself with:

    export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib

Add that line to the bottom of your ~/.bashrc to make it permanent.
