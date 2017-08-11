
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
#define _GNU_SOURCE
#include <sched.h>

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>



#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <net/if.h>
#include <net/ethernet.h>

#include "vnfapp.h"

bool set_promiscous_mode(int fd, char *intf_name);
bool get_interface_status(int fd, char *intf_name);
int set_pmap(intf_config_t *config, uint8_t **read_ring, uint8_t **write_ring);
void *read_write_one(intf_config_t *f_config);
void *read_write_two(intf_config_t *f_config, intf_config_t *s_config);
int set_socket_non_blocking(int fd);
int get_mtu_size(int fd, char *name);

void vnfapp(arg_config_t *arg_config){
	
	int tstatus, ec;
    intf_config_t f_config, s_config;
    struct sockaddr_ll saddr;
    struct ifreq ifr; 
    int mtu_size;
    socklen_t bufSize;
#ifdef DEBUG
    unsigned int rcvBufferSize;
    unsigned int sndBufferSize;
#endif

    if ( (strcmp(arg_config->first, "") == 0)  || (strcmp(arg_config->second, "") == 0)  || (strcmp(arg_config->first,arg_config->second) == 0) ){
        if  (strcmp(arg_config->first, "") != 0){
            memset(&f_config,0,sizeof(f_config));
            strncpy(f_config.name,arg_config->first,IFNAMSIZ-1);
            f_config.max_ring_frames = arg_config->max_ring_frames;
            f_config.max_ring_blocks = arg_config->max_ring_blocks;
            f_config.max_frame_size = arg_config->max_frame_size;
            f_config.mtu_size = 1514;
            f_config.single = true;
        } else if (strcmp(arg_config->first, "") != 0){
            memset(&s_config,0,sizeof(s_config));
            strncpy(s_config.name,arg_config->second,IFNAMSIZ-1);
            s_config.max_ring_frames = arg_config->max_ring_frames;
            s_config.max_ring_blocks = arg_config->max_ring_blocks;
            s_config.max_frame_size = arg_config->max_frame_size;
            s_config.mtu_size = 1514;
        } else {
            printf("Interface not set\n");
            exit(-1);
        }

    } else {
        memset(&f_config,0,sizeof(f_config));
        strncpy(f_config.name,arg_config->first,IFNAMSIZ-1);
        f_config.max_ring_frames = arg_config->max_ring_frames;
        f_config.max_ring_blocks = arg_config->max_ring_blocks;
        f_config.max_frame_size = arg_config->max_frame_size;
        f_config.mtu_size = 1514;
        f_config.single = false;

        memset(&s_config,0,sizeof(s_config));
        strncpy(s_config.name,arg_config->second,IFNAMSIZ-1);
        s_config.max_ring_frames = arg_config->max_ring_frames;
        s_config.max_ring_blocks = arg_config->max_ring_blocks;
        s_config.max_frame_size = arg_config->max_frame_size;
        s_config.mtu_size = 1514;
        f_config.single = false;
    }
    if (f_config.single == true) {
        printf("Initializing Single Interface VNF APP for interface: %s\n", arg_config->first);
    } else {
        printf("Initializing Dual Interface VNF APP for interfaces: %s and %s\n", arg_config->first,arg_config->second);
    }
	/*
	* Create sockets
	*/
    int n = 1;
    f_config.fd = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
	if (f_config.fd == -1){
		perror("Opening first socket");
		exit(-1);
	}
    mtu_size = get_mtu_size(f_config.fd, f_config.name);
    if (mtu_size == -1 ){
        printf("ERROR: Getting MTU size for: %s\n", f_config.name);
        exit(-1);
    } else {
        mtu_size = mtu_size + sizeof(struct ethhdr);
        f_config.mtu_size = mtu_size;
    }
    if (setsockopt(f_config.fd, SOL_SOCKET, SO_BROADCAST, &n, sizeof n) < 0) {
        perror("SO_BROADCAST");
        exit(-1);
    }
    /*
    * Configure interfaces for promiscous mode
    */

    if (set_promiscous_mode(f_config.fd,f_config.name) == true){
        if (get_interface_status(f_config.fd,f_config.name) ==true) {
            printf("Interface: %s is in promiscous mode\n",f_config.name);
        }
    } else {
        printf("ERROR:Setting promiscous (read) mode on: %s\n", f_config.name);
        exit(-1);
    }
    tstatus = set_socket_non_blocking (f_config.fd);
    if (tstatus == -1) {
        perror("Setting non-blocking on first interface");
        exit(-1);
    }
    tstatus = set_pmap(&f_config, &(f_config.r_ring), &(f_config.w_ring));
    if (tstatus == -1){
        printf("ERROR: Configuring first pmap on: %s\n", f_config.name);
        exit(-1);
    }
#ifdef DEBUG
    bufSize = sizeof(rcvBufferSize);
    getsockopt(f_config.fd, SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &bufSize);
    printf("initial socket receive buf %d\n", rcvBufferSize);
#endif
    bufSize = arg_config->max_ring_frames * arg_config->max_frame_size;
    if (setsockopt(f_config.fd, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize)) == -1) {
        perror("SO_RCVBUF");
        exit(-1);
    }
#ifdef DEBUG
    bufSize = sizeof(rcvBufferSize);
    getsockopt(f_config.fd, SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &bufSize);
    printf("after set socket receive buf %d\n", rcvBufferSize);
#endif

#ifdef DEBUG
    bufSize = sizeof(sndBufferSize);
    getsockopt(f_config.fd, SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &bufSize);
    printf("initial socket send buf %d\n", sndBufferSize);
#endif
    //n = pmmap_tx_buf_num*PAN_PACKET_MMAP_FRAME_SIZE; // To improve performance
    bufSize= arg_config->max_ring_frames * arg_config->max_frame_size;
    if (setsockopt(f_config.fd, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize)) == -1) {
        perror("SO_SNDBUF");
        exit(-1);
    }
#ifdef DEBUG
    bufSize = sizeof(sndBufferSize);
    getsockopt(f_config.fd, SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &bufSize);
    printf("after set socket send buf %d\n", sndBufferSize);
#endif
    /* convert interface name to index (in ifr.ifr_ifindex) */
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name, f_config.name, sizeof(ifr.ifr_name));
    ec = ioctl(f_config.fd, SIOCGIFINDEX, &ifr);
    if (ec < 0) {
        printf("Error: failed to find interface %s\n",f_config.name);
        exit(-1);
    }
    /* Bind the interface */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sll_family = PF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = ifr.ifr_ifindex;
    if (bind(f_config.fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("bind failed for first read socket\n");
        exit(-1);
    }
    if (f_config.single == false) {
        s_config.fd = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
        if (s_config.fd == -1){
    		perror("Opening second socket");
    		exit(-1);
    	}
        mtu_size = get_mtu_size(s_config.fd, s_config.name);
        if (mtu_size == -1 ){
            printf("ERROR: Getting MTU size for: %s\n", s_config.name);
            exit(-1);
        } else {
            mtu_size = mtu_size + sizeof(struct ethhdr);
            s_config.mtu_size = mtu_size;
        }
        if (setsockopt(s_config.fd, SOL_SOCKET, SO_BROADCAST, &n, sizeof n) < 0) {
            perror("SO_BROADCAST");
            exit(-1);
        }
        if (set_promiscous_mode(s_config.fd,s_config.name) == true){
            if (get_interface_status(s_config.fd,s_config.name) ==true) {
                printf("Interface: %s is in promiscous mode\n",s_config.name);
            }
        } else {
            printf("ERROR:Setting promiscous (write) mode on: %s\n", s_config.name);
            exit(-1);
        }
        tstatus = set_socket_non_blocking (s_config.fd);
        if (tstatus == -1) {
            perror("Setting non-blocking on second interface");
            exit(-1);
        }
        tstatus = set_pmap(&s_config, &(s_config.r_ring), &(s_config.w_ring));
        if (tstatus == -1){
            printf("ERROR: Configuring second pmap on: %s\n", s_config.name);
            exit(-1);
        } 
#ifdef DEBUG
        bufSize = sizeof(rcvBufferSize);
        getsockopt(s_config.fd, SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &bufSize);
        printf("initial socket receive buf %d\n", rcvBufferSize);
#endif
        n = arg_config->max_ring_frames * arg_config->max_frame_size;
        if (setsockopt(s_config.fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) == -1) {
            perror("SO_RCVBUF");
            exit(-1);
        }
#ifdef DEBUG
        bufSize = sizeof(rcvBufferSize);
        getsockopt(s_config.fd, SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &bufSize);
        printf("after set socket receive buf %d\n", rcvBufferSize);
#endif

#ifdef DEBUG
        bufSize = sizeof(sndBufferSize);
        getsockopt(s_config.fd, SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &bufSize);
        printf("initial socket send buf %d\n", sndBufferSize);
#endif
    //n = pmmap_tx_buf_num*PAN_PACKET_MMAP_FRAME_SIZE; // To improve performance
        n = arg_config->max_ring_frames * arg_config->max_frame_size;
        if (setsockopt(s_config.fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n)) == -1) {
            perror("SO_SNDBUF");
            exit(-1);
        }
#ifdef DEBUG
        bufSize = sizeof(sndBufferSize);
        getsockopt(s_config.fd, SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &bufSize);
        printf("after set socket send buf %d\n", sndBufferSize);
#endif
        /* convert interface name to index (in ifr.ifr_ifindex) */
        memset(&ifr,0,sizeof(ifr));
        strncpy(ifr.ifr_name, s_config.name, sizeof(ifr.ifr_name));
        ec = ioctl(s_config.fd, SIOCGIFINDEX, &ifr);
        if (ec < 0) {
            printf("Error: failed to find interface %s\n", s_config.name);
            exit(-1);
        }
        /* 
        * Bind the interfaces
        */
        memset(&saddr, 0, sizeof(saddr));
        saddr.sll_family = PF_PACKET;
        saddr.sll_protocol = htons(ETH_P_ALL);
        saddr.sll_ifindex = ifr.ifr_ifindex;
        if (bind(s_config.fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
            perror("bind failed for second read socket\n");
            exit(-1);
        }
    }
	/*
	* Read from interface and write to other interface
	*/
    if (f_config.single == false) {
        read_write_two(&f_config, &s_config);
    } else {
        read_write_one(&f_config);
    }
}
