/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
* Utility functions for NFV Application
*/
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
//
#include <arpa/inet.h>
#include <linux/if_packet.h>
//
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/epoll.h>
//
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ip_icmp.h>

#include "vnfapp.h"

#define MAX_BUF 65536
#define MAX_EVENTS 5

uint16_t display_ethernet(uint8_t *buf);
uint16_t display_ip(uint8_t *buf);
void display_icmp(uint8_t *buf);


int set_socket_non_blocking (int sfd) {
  int flags, s;

  flags = fcntl (sfd, F_GETFL, 0);
  if (flags == -1)
    {
      perror ("fcntl");
      return -1;
    }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if (s == -1)
    {
      perror ("fcntl");
      return -1;
    }

  return 0;
}

bool set_promiscous_mode(int fd, char *intf_name){
	bool status = true;
	int rtn;
	struct ifreq ifr;

	
	memset(&ifr, 0, sizeof(ifr));
	/* Set interface to promiscuous mode - do we need to do this every time? */
	strncpy(ifr.ifr_name, intf_name, IFNAMSIZ-1);
	rtn = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (rtn == -1){
		perror("SIOCGIFFLAGS");
		exit(-1);
	}
	ifr.ifr_flags |= IFF_PROMISC;
	rtn = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if (rtn == -1){
		perror("SIOCSIFFLAGS");
		exit(-1);
	}


	return status;
}
/*
* Get status of an interface
*/
bool get_interface_status(int fd, char * intf_name){
	bool status = true;
    struct ifreq ifr;
 	int rtn;
    memset(&ifr, 0, sizeof(ifr));

    /* set the name of the interface we wish to check */
    strncpy(ifr.ifr_name, intf_name, IFNAMSIZ-1);
    /* grab flags associated with this interface */
    rtn = ioctl(fd, SIOCGIFFLAGS, &ifr);
    if (rtn == -1){
		perror("SIOCSIFFLAGS");
		exit(-1);
	}
    if (ifr.ifr_flags & IFF_PROMISC) {
       status = true;
    } else {
		perror("Get Interface status:");
        printf("%s is NOT in promiscuous mode\n",
               ifr.ifr_name);
        status = false;
    }

	return status;
}
/*
* Configure ring buffer for socket
*/
int set_pmap(intf_config_t *vnf_config, uint8_t **read_ring, uint8_t **write_ring){
 	struct tpacket_req treq_rx, treq_tx;
 	int status = 0;
 	unsigned long memlen;
 	int v = TPACKET_V2;

 	memset(&treq_rx, 0, sizeof(treq_rx));
 	treq_rx.tp_block_size =  vnf_config->max_ring_frames * vnf_config->max_frame_size;
	treq_rx.tp_block_nr   =  vnf_config->max_ring_blocks;
	treq_rx.tp_frame_size =  vnf_config->max_frame_size;
	treq_rx.tp_frame_nr   =  vnf_config->max_ring_frames * vnf_config->max_ring_blocks;

	memset(&treq_tx, 0, sizeof(treq_tx));
	treq_tx.tp_block_size = vnf_config->max_ring_frames * vnf_config->max_frame_size;
	treq_tx.tp_block_nr   = vnf_config->max_ring_blocks;
	treq_tx.tp_frame_size = vnf_config->max_frame_size;
	treq_tx.tp_frame_nr   = vnf_config->max_ring_frames * vnf_config->max_ring_blocks;

	if (setsockopt(vnf_config->fd , SOL_PACKET , PACKET_VERSION , &v , sizeof(v)) == -1){
		perror("PACKET_VERSION");
		close(vnf_config->fd);
		return -1;
	}
	if (setsockopt(vnf_config->fd , SOL_PACKET , PACKET_RX_RING , (void*)&treq_rx , sizeof(treq_rx)) == -1){
		perror("PACKET_RX_RING");
		exit (-1);
	}
   if (setsockopt(vnf_config->fd, SOL_PACKET , PACKET_TX_RING , (void*)&treq_tx , sizeof(treq_tx)) == -1){
		perror("PACKET_TX_RING");
		exit(-1);
	}
	memlen = treq_rx.tp_block_size * treq_rx.tp_block_nr;

  	*read_ring = mmap(NULL, 2 * memlen, PROT_READ | PROT_WRITE, MAP_SHARED, vnf_config->fd, 0);
  	if (*read_ring == MAP_FAILED) {
  		perror("mmap");
   		printf("Error: mmap failed: %d\n", errno);
   	    exit(-1);
   	}
   	*write_ring = *read_ring+memlen;

	return status;
}
int get_mtu_size(int fd, char *name){

	struct ifreq ifr;
	strcpy(ifr.ifr_name, name);
	if (!ioctl(fd, SIOCGIFMTU, &ifr)) {
   		return ifr.ifr_mtu;// Contains current mtu value
	} 

	return -1;
}
bool is_power_two(int n)
{
  /*
  * Zero is not a power of 2
  */
  if (n == 0)
    return false;
  while (n != 1)
  {
  	/*
  	* If remainder not power of 2
  	*/
    if (n%2 != 0)
      return false;
    n = n/2;
  }
  return true;
}
/*
* Utilities to print out network headers
*/
uint16_t display_ethernet(uint8_t *buffer){

	struct ethhdr *eth = (struct ethhdr *)(buffer);
	printf("\nEthernet Header\n");
	printf("\t|-Source Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5]);
	printf("\t|-Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
	printf("\t|-Protocol : 0x%04x\n",ntohs(eth->h_proto));
	return ntohs(eth->h_proto);
}

uint16_t display_ip(uint8_t *buffer){

	struct sockaddr_storage source,dest; 
	struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));
	memset(&source, 0, sizeof(source));
	((struct sockaddr_in *)&source)->sin_addr.s_addr = ip->saddr;
	memset(&dest, 0, sizeof(dest));
	((struct sockaddr_in *)&dest)->sin_addr.s_addr = ip->daddr;

	printf("\n\nIPV4 Header\n");
	printf("\t|-Version : %u\n",(unsigned int)ip->version);
	printf("\t|-Internet Header Length : %u DWORDS or %u Bytes\n",(unsigned int)ip->ihl,((unsigned int)(ip->ihl))*4); 
	printf("\t|-Type Of Service : %d\n",(unsigned int)ip->tos); 
	printf("\t|-Total Length : %d Bytes\n",ntohs(ip->tot_len)); 
	printf("\t|-Identification : %d\n",ntohs(ip->id)); 
	printf("\t|-Time To Live : %d\n",(unsigned int)ip->ttl); 
	printf("\t|-Protocol : %d\n",(unsigned int)ip->protocol);
	printf("\t|-Header Checksum : %d\n",ntohs(ip->check)); 
	printf("\t|-Source IP : %s\n", inet_ntoa(((struct sockaddr_in *)&source)->sin_addr) ) ;
	printf("\t|-Destination IP : %s\n",inet_ntoa(((struct sockaddr_in *)&dest)->sin_addr) );

	return ip->protocol;
}

void display_icmp(uint8_t *buffer)
{
    unsigned short iphdrlen;
     
    struct iphdr *iph = (struct iphdr*)(buffer + sizeof(struct ethhdr));
    iphdrlen = iph->ihl*4;
    struct icmphdr *icmph = (struct icmphdr *)(buffer + iphdrlen + sizeof(struct ethhdr));
         
         
    printf("\nICMP Header\n");
    printf("\t|Type : %u\t",(uint8_t)(icmph->type));
    printf("\t\t|Type Name:");
    if (icmph->type == ICMP_ECHOREPLY) {
    		printf("\t\tICMP ECHOREPLY\n");
    		printf("\t\tID: %" PRIu16 "\n",(uint16_t)ntohs(icmph->un.echo.id));
    		printf("\t\tSequence number: %" PRIu16 "\n",(uint16_t)ntohs(icmph->un.echo.sequence));
    	};
    if (icmph->type == ICMP_DEST_UNREACH) printf("\t\tICMP DEST_UNREACH\n");
    if (icmph->type == ICMP_SOURCE_QUENCH) printf("\t\tICMP SOURCE_QUENCH\n");
    if (icmph->type == ICMP_ECHO) {
		printf("\t\tICMP ECHO\n");
		printf("\t\tID: %" PRIu16 "\n",(uint16_t)ntohs(icmph->un.echo.id));
    	printf("\t\tSequence number: %" PRIu16 "\n",(uint16_t)ntohs(icmph->un.echo.sequence));
    }
    if (icmph->type == ICMP_TIME_EXCEEDED) printf("\t\tICMP TIME_EXCEEDED\n");
    if (icmph->type == ICMP_PARAMETERPROB) printf("\t\tICMP PARAMETERPROB\n");
    if (icmph->type == ICMP_TIMESTAMP) printf("\t\tICMP TIMESTAMP\n");
    if (icmph->type == ICMP_TIMESTAMPREPLY) printf("\t\tICMP TIMESTAMPREPLY\n");
    if (icmph->type == ICMP_INFO_REPLY) printf("\t\tICMP INFO_REPLY\n");
    if (icmph->type == ICMP_ADDRESS) printf("\t\tICMP ADDRESS\n");
    if (icmph->type == ICMP_ADDRESSREPLY) printf("\t\tICMP ADDRESSREPLY\n");
    if (icmph->type ==  NR_ICMP_TYPES) printf("\t\tNR_ICMP_TYPES\n");

    printf("\n\t|-Code : %d\n",(uint8_t)(icmph->code));
    printf("\t|-Checksum : %d\n",ntohs(icmph->checksum));
    //printf("\t|-ID       : %d\n",ntohs(icmph->id));
    //printf("\t|-Sequence : %d\n",ntohs(icmph->sequence));
    //fprintf(logfile,"Data Payload\n");  
    //PrintData(Buffer + iphdrlen + sizeof icmph , (Size - sizeof icmph - iph->ihl * 4));
}