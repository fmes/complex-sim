#ComplexSim
SMP-aware, C-based, complex network simulation framework

Complex-sim is a general purpose, event-driven and activity-driven software for running simulations on SMP systems. It is built over a control engine, which drives a workerpool and an event subsystem.

The part related to the simulation of complex networks is still in development, and makes use of the C-based API of the [Igraph Library](http://igraph.sourceforge.net/). 

For a detailed description of the architecture and design of the simulator and/or citation, please refer this [paper](http://ieeexplore.ieee.org/xpls/abs_all.jsp?arnumber=6245701&tag=1)

##Requirements

[Log4c](http://log4c.sourceforge.net/)

[Igraph Library] (http://igraph.sourceforge.net/)

[Autoconf](http://www.gnu.org/software/autoconf/)

[Automake](http://www.gnu.org/software/automake/)

##Building and installing

Supposing that the library igraph has been installed into /opt/igraph, that Log4c has been installed in a standard location (e.g. /usr/lib) and that Complex-sim will be installed into /opt/complexsim:

```
~$ autoreconf -fi
~$ CFLAGS=`pkgconf --cflags igraph` LDFLAGS=`pkgconf --libs igraph` ./configure --prefix=/opt/complexsim
~$ make && make install
```
The procedure described above allows the user to build the library in many systems. 

##Tutorial and samples

TODO
