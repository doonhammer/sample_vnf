# Sample VNF

This is a very simple virtusl network function (VNF) that is intended to be a test vehicle for Service Function Chaining.
The VNF is a simple "bump in the wire" implementation. The interfaces are in promiscous mode and simple transmit all packets
received with no changes.

The implementation uses packet mmap to maximize performance but the code ha not been optimized.

It can work in either of two modes:

* Single Interface: All traffic goes in and out of the same interface.
* Dual Interface: Traffic goes in one interface and out the other.

# Software and System Requirements
The implementation has been tested on the following platforms:

*Platforms:*
Centos 7.3

THe VNF can be deployed on Bare metal, in a VM or in a Docker container.

# Building

To create the VNF clone the repository and

<pre><code>
$ make
</code></pre>

The only dependancy code has is gcc and libc.

To run the application, note sudo is required as the application creates RAW Sockets and uses Packet MMAP. 

<pre><code>
$ sudo ./bin/vnf -h
</code></pre>

To run in single interface mode:

<pre><code>
$ sudo ./bin/vnf -f 'interface name'
</code></pre>

To run in dual interface mode:

<pre><code>
$ sudo ./bin/vnf -f 'first interface name' -s 'second interface name' 
</code></pre>

There are options for tuning the mmap packet buffers. I suggest before  changing these parameters the user reads:

[packet mmap](https://www.kernel.org/doc/Documentation/networking/packet_mmap.txt)

# Troubleshooting

There is debug built into the code - compile withj -DDUBUG and that shoudl help.

# ToDo List

- [ ] Performance tuning