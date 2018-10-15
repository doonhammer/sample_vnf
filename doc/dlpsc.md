# Day in the life of a packet for service chains.
 
## SFC Rational

The goal of adding service function chaining (SFC) to OVN/OVS is to enable a 
set of use cases, primarily in the security area. Additionally, any network 
function that needs to be inserted in the traffic path to inspect and/or 
enhance network payloads would be able to leverage this capability. A simple 
example would be video encoding/decoding or other forms of content enhancement.  


A primary goal of the SFC design is simplicity of both use and requirements on
virtual network functions (VNF) that leverage the feature over extensive 
features. SFC provides the ability to insert any VNF into the path of traffic
before a virtual workload (virtual machine or container) and apply policy. The
only requirement on the VNF is that is support "bump in the wire" or 
transparent proxy networking. Not having the VNF participate in L2 or L3
networking enables scalable deployment of VNFs and leverages OVNs logical 
networking capabilities., e.g. if the VNF is moved from hypervisor A to 
hypervisor B there are no changes required in the VNF only in the OVN/OVS
control plane. In large scale deployments this reduction in complexing 
becomes significant.

## Current Restrictions
SFC does not require any additional metadata within OVN/OVS or does it insert
any additional protocol headers such as proposed by IETF Network Service 
Headers (NSH), https://datatracker.ietf.org/doc/draft-ietf-sfc-nsh/. 
Consequently, SFC is a less feature rich solution, but as stated above it is 
much simpler to use and deploy.

### Supported Features
* Arbitrary long chains of VNFs.
* Only one chain per workload.
* Uses flow classifier implemented in OVN/OVS.
* SFC can only support a chain on egress (could support ingress by adding  
metadata in OVN/OVS).

### Not supported features (supported by NSH)
* Arbitrary number of chains per workload.
* Chains can be applied in both ingress and egress.
* Complex logic with chain (enables graphs of VNFs).
* Arbitrary flow classifiers can be used.

## Security Use Cases
The primary use case for security VNFs for SFC is to provide visibility and 
protection for traffic flowing in an East-West direction between both virtual 
machines and containers. This is required to prevent sophisticated attacks 
known as Advanced Persistent Threats (APT), that enter a network at an area of 
weak security, through phishing, social engineering, connect business partner 
etc, and then move laterally through an organization to reach a high value 
target.

Using SFC it enables organizations to deploy VNFs dynamically with their
workloads and integrate security into the deployment process as there is no
need to implement complicated L2/L3 networking just a simple insertion of a
VNF before a new workload.

Leveraging OVN/OVS enables the VNF to be deployed anywhere (does not need to
be co-resident with the workload) and the support that OVN/OVS provides for 
CNM/CNI plugins extends the solution to containers as well as virtual 
machines.

## Design objectives

* No requirements for VNFs to parse specific service chaining headers.  
Getting all VNF vendors to agree is hard and the additional overhead of  
additional encap/decap can be high.
* Ideally existing VNF vendors can use the solution with no modifications to  
their VNFs.
* No requirements for VNF to participate in networking as the issue of scale  
in dynamic networks becomes a major issue. Thousands of VMs/Containers and  
hundreds of VNFs are a problem to manage if the VNFs participate directly in  
the networking due to the rate of change and the need to maintain consistency.  
Use "bump in the wire" as the networking model for the VNFs.
* Leverage features of the infrastructure as much as possible, e.g. match in  
OVS/OVN and in future load balancing and Geneve tunnel parameters.
* Work with VNFs with one or two data interfaces
* Follow the API model proposed by Openstack Networking-SFC project.
 
## Discussion of MAC Learning Issue
 
There is an issue of a physical switch getting confused with MAC learning
with the current approach. This problem arises when a packet exiting a service
chain is delivered to a physical switch/network on its way to the final
destination. The packet will come to the physical switch port with the source
MAC address of the source application but will be coming from
an OVS port attached to a VNF if simple forwarding rules are applied.
 
The resolution to this issue is to send the packet back to the source
application and send it to the original destination from there. This will
address the MAC learning issue provided all the VNFs and the
application the service chain is being applied to are virtual.
 
There are two approaches to addressing the issue:
 
1. Re-circulate the packet back through the ingress pipeline when it exits  
the service chain starting at the stage after the service chain  
(S_SWITCH_IN_CHAIN). If the packet exits the virtual switch to the physical  
layer (switch) it will have the correct MAC Address for the port it enters  
the switch. The issue with this approach is that any metadata accumulated  
in the stages before (S_SWITCH_IN_CHAIN) will be lost.
 
2. Re-circulate the packet back through the full ingress pipeline on its way  
to the original destination from the service chain. This will add a  
performance penalty to service chaining, but it ensures any metadata  
generated by the stages before S_SWITCH_IN_CHAIN is correctly dealt with. To  
skip the skip the S_SWITCH_IN_CHAIN stage the second time through a flag  
would be set in a register. This would enable the packet to traverse the  
ingress stages and go the original destination after going through the  
service chain.
 
This approach adds a performance penalty as it requires the packet to traverse 
the stages before S_SWITCH_IN_CHAIN twice, but guarantees the metadata is 
correct after the service chain stage.
 
### Proposed Resolution
 
While neither approach is ideal, the first approach is workable as there is
no metadata that is required by the stages after S_SWITCH_IN_CHAIN, from the
stages before S_SWITCH_IN_CHAIN. This might change in the future, at that
point additional metadata could be propagated locally by the S_SWITCH_IN_CHAIN
stage over GENEVE tunnels. The metadata would not be Network Service
Header (NSH) metadata as it would be local to OVN/OVS and not exposed to the
VNFs in the service chain.
 
## Proposed Stages in Switch Pipeline
 
The new stages and their location in the switch ingress and egress pipelines
are shown below. The two stages added are S_SWITCH_IN_CHAIN (stage 10) and
S_SWITCH_OUT_CHAIN (stage 0). The later stage is a destination stage for the
loopback to the initial source application to prevent looping.
 
<pre><code>
    /* Logical switch ingress stages. */                                  \
    PIPELINE_STAGE(SWITCH, IN,  PORT_SEC_L2,    0, "ls_in_port_sec_l2")   \
    PIPELINE_STAGE(SWITCH, IN,  PORT_SEC_IP,    1, "ls_in_port_sec_ip")   \
    PIPELINE_STAGE(SWITCH, IN,  PORT_SEC_ND,    2, "ls_in_port_sec_nd")   \
    PIPELINE_STAGE(SWITCH, IN,  PRE_ACL,        3, "ls_in_pre_acl")       \
    PIPELINE_STAGE(SWITCH, IN,  PRE_LB,         4, "ls_in_pre_lb")        \
    PIPELINE_STAGE(SWITCH, IN,  PRE_STATEFUL,   5, "ls_in_pre_stateful")  \
    PIPELINE_STAGE(SWITCH, IN,  ACL,            6, "ls_in_acl")           \
    PIPELINE_STAGE(SWITCH, IN,  QOS_MARK,       7, "ls_in_qos_mark")      \
    PIPELINE_STAGE(SWITCH, IN,  LB,             8, "ls_in_lb")            \
    PIPELINE_STAGE(SWITCH, IN,  STATEFUL,       9, "ls_in_stateful")      \
    PIPELINE_STAGE(SWITCH, IN,  CHAIN,         10, "ls_in_chain")         \
    PIPELINE_STAGE(SWITCH, IN,  ARP_ND_RSP,    11, "ls_in_arp_rsp")       \
    PIPELINE_STAGE(SWITCH, IN,  DHCP_OPTIONS,  12, "ls_in_dhcp_options")  \
    PIPELINE_STAGE(SWITCH, IN,  DHCP_RESPONSE, 13, "ls_in_dhcp_response") \
    PIPELINE_STAGE(SWITCH, IN,  DNS_LOOKUP,    14, "ls_in_dns_lookup") \
    PIPELINE_STAGE(SWITCH, IN,  DNS_RESPONSE,  15, "ls_in_dns_response") \
    PIPELINE_STAGE(SWITCH, IN,  L2_LKUP,       16, "ls_in_l2_lkup")       \
                                                                          \
    /* Logical switch egress stages. */                                   \
    PIPELINE_STAGE(SWITCH, OUT, CHAIN,        0, "ls_out_chain")          \
    PIPELINE_STAGE(SWITCH, OUT, PRE_LB,       1, "ls_out_pre_lb")         \
    PIPELINE_STAGE(SWITCH, OUT, PRE_ACL,      2, "ls_out_pre_acl")        \
    PIPELINE_STAGE(SWITCH, OUT, PRE_STATEFUL, 3, "ls_out_pre_stateful")   \
    PIPELINE_STAGE(SWITCH, OUT, LB,           4, "ls_out_lb")             \
    PIPELINE_STAGE(SWITCH, OUT, ACL,          5, "ls_out_acl")            \
    PIPELINE_STAGE(SWITCH, OUT, QOS_MARK,     6, "ls_out_qos_mark")       \
    PIPELINE_STAGE(SWITCH, OUT, STATEFUL,     7, "ls_out_stateful")       \
    PIPELINE_STAGE(SWITCH, OUT, PORT_SEC_IP,  8, "ls_out_port_sec_ip")    \
    PIPELINE_STAGE(SWITCH, OUT, PORT_SEC_L2,  9, "ls_out_port_sec_l2")    \
</code></pre>
 
## Use Cases
 
The use cases below will be based on the first approach. The use cases walk
through the packet flow for several potential deployment models of
service chaining.
 
### Case 1
 
The simplest case is two applications and a virtual network function (VNF) all
on the same host. The goal is to steer traffic going to App2 from App1 (and
any other application) through VNF1. The VNF is shown with two ports for
clarity but two ports are not required. The solution will work if the
VNF has one or two ports. The configuration is shown below:
 
<pre><code>
+-------------------------------------------+
|                     Host1                 |
|   +--------+     +--------+    +--------+ |
|   |        |     |        |    |        | |
|   |VM-App1 |     |  VNF1  |    |VM-App2 | |
|   |        |     |        |    |        | |
|   +----+---+     +-+----+-+    +----+---+ |
|        |           |    |           |     |
|       P1          P2   P3          P4     |
|        |           |    |           |     |
|        |           |    |           |     |
|  +-----+-----------+----+-----------+--+  |
|  |                                     |  |
|  |                 OVS SW0             |  |
|  |                                     |  |
|  +-------------------------------------+  |
+-------------------------------------------+
</pre></code>
 
Assume you have port P1 to VM-App1 running in a VM on Host1, port P2 as the
input port to a VNF on the same host, port P3 as the output port on the VNF,
and port P4 as connected to VM-App2 running on its own VM.
 
The service chain is just that one VNF, with direction "entry-lport" to
VM-App2. All traffic going to VM-App2 will be steered through the service
chain going through P2 then out P3. Traffic in the reverse direction will go
from P3 to P2.
 
The setup commands for the use case are shown below:
 
<pre><code>
#
# Configure the port pair PP1
ovn-nbctl lsp-pair-add SWO VNF1-P2 VNF1-P2 PP1
#
# Configure the port chain PC1
ovn-nbctl lsp-chain-add SWO PC1
#
# Configure the port pair group PG1 and add to port chain
ovn-nbctl lsp-pair-group-add PC1 PG1
#
# Add port pair to port chain
ovn-nbctl lsp-pair-group-port-pair-add PG1 PP1
#
# Add port chain to port classifier PCC1
ovn-nbctl lsp-chain-classifier SW0 PC1 VM-App2 "entry-lport" \
          "bi-directional" PCC1 ""
</code></pre>
 
 
The packet path is as follows:

1. VM-App1 sends a packet to VM-App2 through P1. The ingress pipeline goes  
through tables 1-9, at table 10 (S_SWITCH_IN_CHAIN) the packet matches the  
destination mac address as VM-App2 inport P1 but does not match the port as  
P3 therefore it hits the lowest priority rule (priority 100) the action is to  
set the output port to P2 and output directly to the VNF.  

2. VNF1 processes the packet and it is sent out port P3 to the local OVS. The  
VNF acts as a *bump in the wire*, it does not alter the packet network header,  
though it can act on the packet payload.  

3. The ingress pipeline goes through tables 1-9, at table 10 (S_SWITCH_IN_CHAIN)  
the packet matches the destination MAC as VM-App2 and does match the input  
port as P3 therefore it hits the highest priority rule (priority 150). This is  
the last VNF in the chain (chain length is 1) the action is to set the action  
to output port P2 and next; and then forward to the next stage in the chain, 
table 11.

4. The packet in the reverse path leaves VM-APP2 with a source mac address as  
VM-App2 and an input port of P4. The ingress pipeline goes through tables 1-9,  
at table 10 (S_SWITCH_IN_CHAIN) the packet matches the source MAC as VM-App2  
inport 4 and does not match the inport as P3 therefore it hits the lowest   
priority rule (priority 100) the action is to set the output port to P3 and  
then output the packet.
 
5. VNF1 processes the packet and it is sent out P2 to the local OVS. The VNF  
acts as a *bump in the wire*, it does not alter the packet network header,  
though it can act on the packet payload.

6. The egress pipeline goes through tables 1-9, at table 10 S_SWITCH_IN_CHAIN  
the packet matches the source mac as VM-App2 and does match the port as P2  
therefore it hits the highest priority rule (priority 150). The traffic is  
coming from the virtual network but may be going to a physical network where a  
physical switch would see a packet with a mac address that does not match the  
port, this would confuse MAC learning. To address this the egress rules, send  
the packet back to the source.
 
7. This is the last VNF in the chain (chain length is 1) the action is to set  
the input port to P1 and then forward to the packet to the egress stage  
S_SWITCH_OUT_CHAIN with an action to insert into the next stage in the ingress  
chain after S_SWITCH_IN_CHAIN (table 11).

8. The packet is now coming from VM-App2 and is inserted into the ingress  
pipeline at table 11 after table 10 (S_SWITCH_IN_CHAIN) with destination port  
P4. Normal processing is now for the remain stages in the ingress pipeline.
 
 
### Case 2
 
This case has the VM-App1 and VM-App2 on different hosts (Hypervisors). This
requires the packet to traverse a (Geneve) tunnel twice, once for ingress and
once for egress.
 
The service chain is just one VNF, with direction "entry-lport" to
VM-App2. 
 
<pre><code>
+--------------------------+      +----------------+
|          Host 1          |      |     Host 2     |
| +-------+  +--------+    |      |   +--------+   |
| |       |  |        |    |      |   |        |   |
| |VM-App1|  |  VNF1  |    |      |   |VM-App2 |   |
| |       |  |        |    |      |   |        |   |
| +----+--+  +-+----+-+    |      |   +---+----+   |
|      |       |    |      |      |       |        |
|     P1      P2   P3      |      |      P4        |
|      |       |    |      |      |       |        |
|      |       |    |      |      |       |        |
|  +---+-------+----+-+    |      |+------+-------+|
|  |       OVS        |    |      ||     OVS      ||
|  +------------------+    |      |+--------------+|
|                          |      |                |
|  +------------------+    |      |+--------------+|
|  |  OVN Controller  |    |      ||OVN Controller||
|  +---------+--------+    |      |+-------+------+|
|            |             |      |        |       |
+------------+-------------+      +--------+-------+
             |                             |       
             |                             |       
             |                             |       
             +--------------+--------------+       
                            |                       
                            |                      
                            |                      
               +------------+----------+           
               |   +--------+------+   |           
               |   |   ovn-sb-db   |   |           
               |   +--------+------+   |           
               |   +--------+------+   |           
               |   |  ovn-northd   |   |           
               |   +--------+------+   |           
               |   +--------+------+   |           
               |   |   ovn-nb-db   |   |           
               |   +---------------+   |           
               |       OVN Host        |           
               +-----------------------+           
</code></pre>
 
**NOTE:** The ovn-nbctl configuration commands are identical to Case 1, the
only difference is that the application VMs are now running on two different
hosts.
 
 
<pre><code>
#
# Configure the port pair PP1
ovn-nbctl lsp-pair-add SWO VNF1-P2 VNF1-P2 PP1
#
# Configure the port chain PC1
ovn-nbctl lsp-chain-add SWO PC1
#
# Configure the port pair group PG1 and add to port chain
ovn-nbctl lsp-pair-group-add PC1 PG1
#
# Add port pair to port chain
ovn-nbctl lsp-pair-group-port-pair-add PG1 PP1
#
# Add port chain to port classifier PCC1
ovn-nbctl lsp-chain-classifier SW0 PC1 VM-APP2 "entry-lport" \
          "bi-directional" PCC1 ""
</code></pre>
 
The packet path is as follows:


1. VM-App1 sends a packet (flow) to VM-App2 through P1. The ingress pipeline  
goes through tables 1-9, at table 10 (S_SWITCH_IN_CHAIN) the packet matches  
the destination MAC as VM-App2 inport P1 but does not match the  
port as P3 therefore it hits the lowest priority rule (priority 100) the  
action is to set the output port to P2 and output directly to the VNF.
 
2. VNF1 processes the packet and it is sent out port P3 on the local OVS.  
The VNF acts as a *bump in the wire*, it does not alter the packet network  
header, though it can act on the packet payload.  
 
3. The ingress pipeline goes through tables 1-9, at table 10 (S_SWITCH_IN_CHAIN)  
the packet matches the destination MAC as VM-App2 and does match the input  
port as P3 therefore it hits the highest priority rule (priority 150). This is  
the last VNF in the chain (chain length is 1) the action is to set the action  
to output port P2 and output the packet.

5. The packet is now sent by the local OVS switch Table 32 sends the packet  
through a Geneve tunnel to Host 2.
 
6. The packet arrives on Host 2 and is delivered to P4.

7. The packet in the reverse path leaves VM-APP2 with a source MAC address as  
VM-App2 and an input port of P4. The ingress pipeline goes through tables 1-9,  
at table 10 (S_SWITCH_IN_CHAIN) the packet matches the source MAC as VM-App2  
inport 4 and does not match the inport as P3 therefore it hits the lowest  
priority rule (priority 100) the action is to set the output port to P3 and  
then forward to the next stage in the chain, table 11.

8. The packet is now sent back over the Geneve tunnel to Host 1 and delivered  
to P3.

9. VNF1 processes the packet and it is sent out P2 to the local OVS. The VNF  
acts as a *bump in the wire*, it does not alter the packet network header,  
though it can act on the packet payload.
 
10. The egress pipeline goes through tables 1-9, at table 10  
(S_SWITCH_IN_CHAIN) the packet matches the source mac as VM-App2 and does  
match the port as P2 therefore it hits the highest priority rule  
(priority 150). This is the last VNF in the chain (chain length is 1) the  
packet needs to be delivered to the final destination.
 
12. In this case the traffic is coming from the virtual network but may be  
going to a physical network where a physical switch would see a packet with a  
mac address that does not match the port it is coming from, this would confuse   
MAC learning. To address this the egress rules, send the packet back to the  
source.
 
13. In the egress direction the rule is to set the metadata registers to zero  
then forward to the packet to the egress stage S_SWITCH_OUT_CHAIN with an  
action to insert into the next stage in the ingress chain after  
S_SWITCH_IN_CHAIN (table 11). Sending the packet to the egress stage is to  
prevent the possibility of loops. The input port is set to P4 and the output  
port is unset.
 
14. The packet is sent over the Geneve tunnel to Host 2 with input port set as  
P4.
 
15. The packet is now coming from VM-App2 and is inserted into the ingress  
pipeline at table 11 after table 10 (S_SWITCH_IN_CHAIN) with input port P4.  
Normal processing is now for the remain stages in the ingress pipeline, and  
the output port will be set to P1 in the S_SWITCH_IN_L2_LKUP stage. 
 
### Case 3
 
This use case has a chain to three VNFs and the VNFs inserted before VM-App2.
The setup for Case 3 is shown below:
 
<pre><code>
+------------------------------------------------+      +----------------+
|                     Host 1                     |      |     Host 2     |
| +-------+  +--------+  +--------+  +--------+  |      |   +--------+   |
| |       |  |        |  |        |  |        |  |      |   |        |   |
| |VM-App1|  |  VNF1  |  |  VNF2  |  |  VNF3  |  |      |   |VM-App2 |   |
| |       |  |        |  |        |  |        |  |      |   |        |   |
| +---+---+  +-+----+-+  +-+----+-+  +-+----+-+  |      |   +---+----+   |
|     |        |    |      |    |      |    |    |      |       |        |
|    P1       P2   P3     P4   P5     P6   P7    |      |      P8        |
|     |        |    |      |    |      |    |    |      |       |        |
|     |        |    |      |    |      |    |    |      |       |        |
| +---+--------+----+------+----+------+----+--+ |      |+------+-------+|
| |                    OVS                     | |      ||     OVS      ||
| +--------------------------------------------+ |      |+--------------+|
|                                                |      |                |
| +--------------------------------------------+ |      |+--------------+|
| |               OVN Controller               | |      ||OVN Controller||
| +----------------------+---------------------+ |      |+-------+------+|
|                        |                       |      |        |       |
+------------------------+-----------------------+      +--------+-------+
                         |                                       |       
                         |                                       |       
                         |                                       |       
                         +-------------------+-------------------+       
                                             |                           
                                             |                           
                                             |                           
                                +------------+----------+                 
                                |   +--------+------+   |                
                                |   |   ovn-sb-db   |   |                
                                |   +--------+------+   |                
                                |   +--------+------+   |                
                                |   |  ovn-northd   |   |                
                                |   +--------+------+   |                
                                |   +--------+------+   |                
                                |   |   ovn-nb-db   |   |                
                                |   +---------------+   |                
                                |       OVN Host        |                
                                +-----------------------+                               
</pre></code>
 
 
The configuration is slightly more complicated as three port pairs need to be
defined and added to three port chain groups.
<pre><code>
#
# Configure the port pair PP1
ovn-nbctl lsp-pair-add SWO VNF1-P2 VNF1-P3 PP1
#
# Configure the port pair PP2
ovn-nbctl lsp-pair-add SWO VNF1-P4 VNF1-P5 PP2
#
# Configure the port pair PP1
ovn-nbctl lsp-pair-add SWO VNF1-P6 VNF1-P7 PP3
#
# Configure the port chain PC1
ovn-nbctl lsp-chain-add SWO PC1
#
# Configure the port pair group PG1 and add to port chain
ovn-nbctl lsp-pair-group-add PC1 PG1
#
# Add port pair to port chain
ovn-nbctl lsp-pair-group-port-pair-add PG1 PP1
#
# Configure the port pair group PG2 and add to port chain
ovn-nbctl lsp-pair-group-add PC1 PG2
#
# Add port pair 2 to port pair group 2
ovn-nbctl lsp-pair-group-port-pair-add PG2 PP2
#
# Configure the port pair group PG2 and add to port chain
ovn-nbctl lsp-pair-group-add PC1 PG3
#
# Add port pair to port chain
ovn-nbctl lsp-pair-group-port-pair-add PG3 PP3
#
# Add port chain to port classifier PCC1
ovn-nbctl lsp-chain-classifier SW0 PC1 VM-APP2 "entry-lport" \
          "bi-directional" PCC1 ""
</code></pre>
 
The packet path is as follows:

1. VM-App1 sends a packet (flow) to VM-App2 through P1. The ingress pipeline  
goes through tables 1-9, at table 10 (S_SWITCH_IN_CHAIN) the packet matches  
the destination MAC as VM-App2 inport P1 but does not match the port as P3  
therefore it hits the lowest priority rule (priority 100) the action is to  
set the output port to P2 and output directly to the VNF.

2. VNF1 processes the packet and it is sent out port P3 to the local OVS. The  
VNF acts as a *bump in the wire*, it does not alter the packet network header,  
though it can act on the packet payload.

3. The ingress pipeline goes through tables 1-9, at table 10  
(S_SWITCH_IN_CHAIN) the packet matches the destination MAC as VM-App2 and  
does match the input port as P3 therefore it hits the highest priority rule  
(priority 150). This is the first VNF in the ingress chain (chain length is 3)  
the action is to set the action to output port P2 and next; and then forward  
to the next stage in the chain, table 11.

5. The packet now moves through the chain going through the normal ingress and  
egress stages with S_SWITCH_IN_CHAIN rules setting the output port to the next  
port in the port pair chain list.
 
6. At the last stage in the chain the rule is fired to send the packet to  
VM-App2 (Port P8). The packet is now sent by the local OVS switch Table 32  
through a Geneve tunnel to Host 2.

7. The packet arrives on Host 2 and is delivered to P8.
 
8. The packet in the reverse path leaves VM-APP2 with a source MAC address as  
VM-App2 and an input port of P4. The ingress pipeline goes through tables 1-9,  
at table 10 (S_SWITCH_IN_CHAIN) the packet matches the source MAC as VM-App2  
inport 4 and does not match the inport as P3 therefore it hits the lowest  
priority rule (priority 100) the action is to set the output port to P3 and  
then forward to the next stage in the chain, table 11.

9. The packet is now sent back over the Geneve tunnel to Host 1 and delivered  
to P7.

10. As this is the first VNF in the egress chain (VNF3) (chain length is 3)  
the action is to set the action to output port P7 and next; and then forward  
to the next stage in the chain, table 11.
 
11. VNF3 processes the packet and it is sent out P6 to the local OVS. The VNF  
acts as a *bump in the wire*, it does not alter the packet network header,  
though it can act on the packet payload.
 
12. The packet now moves through the chain going through the normal ingress  
and egress stages with S_SWITCH_IN_CHAIN rules setting the output port to the  
next port in the port pair chain list.

13. At the last VNF in the egress chain (VNF1) (chain length is 3) the packet  
is delivered to the final destination.

14. In this case the traffic is coming from the virtual network but may be  
going to a physical network where a physical switch would see a packet with a  
mac address that does not match the port it is coming from, this would confuse  
MAC learning. To address this the egress rules, send the packet back to the  
source.
 
15. The egress pipeline goes through tables 1-9, at table 10  
(S_SWITCH_IN_CHAIN) the packet matches the source mac as VM-App2 and does  
match the port as P2 therefore it hits the highest priority rule (priority 150).  

16. In the egress direction the rule is to set the metadata registers to zero  
then forward to the packet to the egress stage S_SWITCH_OUT_CHAIN with an  
action to insert into the next stage in the ingress chain after  
S_SWITCH_IN_CHAIN (table 11). Sending the packet to the egress stage is to  
prevent the possibility of loops. The input port is set to P8 and the output  
port is unset.

17. The packet is sent over the Geneve tunnel to Host 2 with input port set as  
P8.

18. The packet is now coming from VM-App2 and is inserted into the ingress  
pipeline at table 11 after table 10 (S_SWITCH_IN_CHAIN) with input port P8.  
Normal processing is now for the remain stages in the ingress pipeline, and  
the output port will be set to P1 in the S_SWITCH_IN_L2_LKUP stage. 
