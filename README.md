# Sample VNF

This is a very simple virtusl network function (VNF) that is intended to be a test vehicle for Service Function Chaining.
The VNF is a simple "bump in the wire" implementation. The interfaces are in promiscous mode and simple transmit all packets
received with no changes.

The implementation uses packet mmap to maximize performance but the code has not been optimized.

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

There is debug built into the code - compile withj -DDEBUG and that should help.

# Containers
This VNF has been published to docker as a container, to get the container search the docker hub.

<pre><code>
$ sudo docker search doonhammer/centos:vnf
NAME                DESCRIPTION   STARS     OFFICIAL   AUTOMATED
doonhammer/centos   Demo VNF      0  
</code></pre>

The OVS/OVN CNI plugin is designed to have a logical switch for each K8S Node. Each POD running on the Node has a logical port to the logical switch. Therefore if a firewall is deployed per logical switch it can be configured to protect pods as they are deployed on the the node.
Deploying a PANOS DP Container into the path of traffic requires that a DP Container be deployed with each K8S POD that requires firewall services.  This process requires automation to operate at scale. The proposed approach is to define a DP container as a Kubernetes DaemonSet. To insert a daemon set it needs to be defined on the master. The sample code below shows a yaml file using the sample VNF that has been pushed to the docker hub.

<pre><code>
apiVersion: extensions/v1beta1
kind: DaemonSet
metadata:
  name: firewall
  namespace: kube-system
  labels:
    vnfType: firewall
spec:
  template:
    metadata:
      labels:
        name: firewall
    spec:
      containers:
      - name: firewall
        image: doonhammer/centos:vnf
        imagePullPolicy: Always
        securityContext:
          capabilities:
            add: ["ALL"]
        resources:
          limits:
            memory: 500Mi
          requests:
            cpu: 1000m
            memory: 500Mi
        volumeMounts:
        - name: varlog
          mountPath: /var/log
        - name: varlibdockercontainers
          mountPath: /var/lib/docker/containers
          readOnly: true
      terminationGracePeriodSeconds: 30
      volumes:
      - name: varlog
        hostPath:
          path: /var/log
      - name: varlibdockercontainers
        hostPath:
          path: /var/lib/docker/containers
</code></pre>
 
If the file is saved as firewall_vnf.yaml it is run on the master by:

<pre><code>
kubectl create -f firewall_vnf.yaml
</code></pre>
Now any node that is created with the tag vnfType: firewall will have a DP container launched and attached to the logical switch via a logical port. The configuration required when starting a new node is shown below:

<pre><code>
./kubelet --api-servers=http://$MASTER_IP:8080 --v=2 --address=0.0.0.0 \
--node-labels=vnfType=firewall \
--allow-privileged=true \
--enable-server=true --network-plugin=cni \
--cni-conf-dir=/etc/cni/net.d \
--cni-bin-dir="/opt/cni/bin/" 2>&1 0<&- &>/dev/null &
</code></pre>

The two key lines are "--node-labels" which tags the node to launch a daemon set and "--allow-privleged" which is required for the VNF to access RAW Sockets and packet mmap.
This approach ensures that any node that requires a PAN-OS DP Container is brought up with a container installed and running before any pods are deployed. Once the node is up and running with the firewall now pods can be added and protected by the firewall.

This configures K8S to automatically deploy a specific VNF(s) anytime a node is created. The traffic to or from a pod on the node can be steered through the VNF using the following script

<pre><code>
	
#!/bin/sh
#
#
# CLI Help function
#
show_help() {
cat << EOF
Usage: ${0##*/} [-h] [-s logical-switch] [-v logical VNF port] [-a logical qpplication port] [-d display current state] [-e echo the porposed commands] [-f dump flows after commands are applied] [-c clear configuration]
 
     -h          display this help and exit
     -s          logical switch
     -v          vnf logical port
     -a          application logical port
     -d          display current chain classifier and exit
     -e          echo the commands to be applied and exit without applying
     -f          dump sb flows
     -c          clear configuration
EOF
}
#
# Parse Command Line Arguments
#
while getopts "hcdefs:v:a:" opt; do
    case "$opt" in
        h)
            show_help
            exit 0
            ;;
        s) opt_ls=$OPTARG
           ;;
        v) opt_lsp_vnf=$OPTARG
           ;;
        a) opt_lsp_app=$OPTARG
           ;;
        d) opt_d=true
           ;;
        e) opt_e=true
           ;;
        f) opt_f=true
           ;;
        c) opt_c=true
           ;;
        '?')
            show_help
            exit 1
            ;;
    esac
done

if [ ! -z "$opt_d" ]; then
        printf "\nCurrent system state\n\n"
        ovn-nbctl lsp-chain-show
        ovn-nbctl lsp-chain-classifier-show
        exit 1
fi

if [ ! -z "$opt_c" ]; then
        printf "\nClearing existing configuration\n"
        ovn-nbctl --if-exists lsp-chain-classifier-del pcc1
        ovn-nbctl --if-exists lsp-pair-group-del ppg1
        ovn-nbctl --if-exists lsp-pair-del pp1
        ovn-nbctl --if-exists lsp-chain-del pc1
        exit 1
fi

if [ -z "$opt_ls" ]; then
        printf "\nERROR: Logical switch must be defined\n"
        exit 1
fi

if [ -z "$opt_lsp_vnf" ]; then
        printf "\nERROR: Logical switch port for VNF must be defined\n"
        exit 1
fi

if [ -z "$opt_lsp_app" ]; then
        printf "\nERROR: Logical switch port for application must be defined\n"
        exit 1
fi

if [ ! -z "$opt_e" ]; then
        printf "\nEcho candidate config\n"
        printf "\novn-nbctl lsp-pair-add  $opt_ls $opt_lsp_vnf $opt_lsp_vnf pp1"
        printf "\novn-nbctl lsp-chain-add $opt_ls pc1"
        printf "\novn-nbctl lsp-pair-group-add pc1 ppg1"
        printf "\novn-nbctl lsp-pair-group-add-port-pair ppg1 pp1"
        printf "\novn-nbctl lsp-chain-classifier-add $opt_ls pc1 $opt_lsp_app \"exit-lport\" \"bi-directional\" pcc1 \"\"\n"
        exit 1
fi
#
# Default install new rules
#
# Remove all existing Configuration
ovn-nbctl --if-exists lsp-chain-classifier-del pcc1
ovn-nbctl --if-exists lsp-pair-group-del ppg1
ovn-nbctl --if-exists lsp-pair-del pp1
ovn-nbctl --if-exists lsp-chain-del pc1
#
# Configure Chain
#
#
# Configure Service chain
#
ovn-nbctl lsp-pair-add  $opt_ls $opt_lsp_vnf $opt_lsp_vnf pp1
ovn-nbctl lsp-chain-add $opt_ls pc1
ovn-nbctl lsp-pair-group-add pc1 ppg1
ovn-nbctl lsp-pair-group-add-port-pair ppg1 pp1
ovn-nbctl lsp-chain-classifier-add $opt_ls pc1 $opt_lsp_app "exit-lport" "bi-directional" pcc1 ""
#
if [ ! -z "$opt_f" ]; then
        ovn-sbctl dump-flows
fi

	
</code></pre>
# ToDo List

- [ ] Performance tuning
