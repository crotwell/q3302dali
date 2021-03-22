# q3302dali
Q330 to DataLink mashup of q3302ew and ew2ringserver to get rid of earthworm shared memory buffers. This allows both one-second and miniseed packets from a Q330 to be sent directly to an IRIS Ringserver via DataLink.

# Compile

Note you will need lib330, libmseed and libdali to be in the directory above src to compile.

lib330 is part of the Earthworm distribution, http://earthwormcentral.org. The version in Earthworm 7.10 does not contain support for miniseed packets from the Q330, so retrieving lib330 from verson control at the developer site is needed.

libmseed is from IRIS, https://github.com/iris-edu/libmseed/

libdali is also from IRIS, https://github.com/iris-edu/libdali

# Run

You will most likely also need an instance of IRIS ringserver to
connect to, https://github.com/iris-edu/ringserver, but this is not needed of course to compile.

```
q3302dali <configfile>
```
