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
/*! Sample Bump in the Wire "BITW" NFV program
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <netinet/in.h>

#include "vnfapp.h"
/*
 * Declare functions
 */
int read_config(char*, arg_config_t*);
double get_clk(void);
void *vnfapp(arg_config_t *arg);
bool validate_mmap(arg_config_t *config);
bool is_power_two(int n);
/**
 * Print configuration (Debugging utility)
 */
void print_config(arg_config_t *config){
    printf("\n---- VNF Test Utility ----\n");
    printf("First interface: %s\n", config->first);
    printf("Second interface: %s\n", config->second);
    printf("Max Ring Frames: %lu\n", config->max_ring_frames);
    printf("Max Ring Blocks: %lu\n",config->max_ring_blocks);
    printf("Max Frame Size: %lu\n",config->max_frame_size);
    printf("----------------------------------------\n");
}
/*
 * Main routine
 */
int main(int argc, char ** argv){
    /*
     * Command Line Arguments
     */
    int c;
    int status;
    char *arg_config=NULL;
    char arg_first[IFNAMSIZ];
    char arg_second[IFNAMSIZ];
    unsigned long max_ring_frames;
    unsigned long max_ring_blocks;
    unsigned long max_frame_size;
    char *str_part;
    bool valid;
    /*
    * Set defaults
    */
    arg_first[0]='\0';
    arg_second[0] = '\0';
    max_ring_frames = MAX_RING_FRAMES;
    max_ring_blocks = MAX_RING_BLOCKS;
    max_frame_size = getpagesize();
    arg_config_t config_info;

    static struct option longopts[] = {
        {"first", required_argument,0,'f'},
        {"second", required_argument,0,'s'},
        {"ring",required_argument,0,'r'},
        {"number",required_argument,0,'n'},
        {"length",required_argument,0,'l'},
        {"help",no_argument,0,'h'},
    };
    printf("Input: %s\n", argv[0]);
    /*
     * Loop over input
     */
    while (( c = getopt_long(argc,argv, "f:s:r:n:l:h",longopts,NULL))!=-1){
        switch(c) {
            case 'f':
                strncpy(arg_first,optarg,IFNAMSIZ-1);
                break;
            case 's':
                strncpy(arg_second, optarg,IFNAMSIZ-1);
                break;
            case 'r':
                max_ring_frames = strtoul(optarg, &str_part,10);
                break;
            case 'n':
                max_ring_blocks = strtoul(optarg, &str_part,10);
                break;
            case 'l':
                max_frame_size = strtoul(optarg, &str_part,10);
                break;
            case 'h':
                printf("Command line arguments: \n");
                printf("-f, --first     First interface \n");
                printf("-s, --second    Second interface \n");
                printf("-r, --ring      Number of blocks of frame size \n");
                printf("-n, --number    Number of rings  \n");
                printf("-l, --length    Length of a frame \n");
                printf("-h, --help:     Command line help \n");
                exit(1);
            default:
                printf("Ignoring unrecognized command line option:%d\n ",c);
                break;
        }
    }
    /*
     * Intialize confguration structure
     */
    if (arg_config != NULL){
      status = read_config(arg_config, &config_info);
        if (status){
            printf("Error reading config file: %s\n",arg_config);
            exit(1);
        }
    } else {
        strncpy(config_info.first,arg_first,IFNAMSIZ-1);
        strncpy(config_info.second,arg_second,IFNAMSIZ-1);
        //if (arg_interface != NULL){
        //      strncpy(config_info.interface,arg_interface,IFNAMSIZ-1);
        //}
        config_info.max_ring_frames = max_ring_frames;
        config_info.max_ring_blocks = max_ring_blocks;
        config_info.max_frame_size = max_frame_size;
    }
    /*
    * TODO - validate mmap parameters
    */
    valid = validate_mmap(&config_info);
    if (valid == false){
        printf("Error: Invalid mmap parameters\n");
        exit(-1);
    }
    /*
     *  run application
     */
    print_config(&config_info);
    vnfapp(&config_info);

    printf("Exiting normally\n");
    return 1;
}
/*
* Placeholder to add configuration file
*/
int read_config(char *file_name, arg_config_t *config){
    /*
     * Read configuration file and set params
     */
    char *first_interface = "em2";
    char *second_interface = "em3";
    config->max_ring_frames = MAX_RING_FRAMES;
    config->max_ring_blocks = MAX_RING_BLOCKS;
    config->max_frame_size = getpagesize();
    strncpy(config->first,first_interface,IFNAMSIZ-1);
    strncpy(config->second, second_interface,IFNAMSIZ-1);

    return 0;
}

bool validate_mmap(arg_config_t *config){
    bool status = true;
    unsigned long nframes,nblocks,frame_size, page_size;
    /*
    * System page size
    */
    page_size = getpagesize();
    /*
    * Values set by default or CLI
    */
    nframes = config->max_ring_frames;
    nblocks = config->max_ring_blocks;    
    frame_size = config->max_frame_size;
    if (!(frame_size <= page_size && is_power_two(frame_size))){
        printf("ERROR: Max frame size: %lu is not a power of 2 or is greater than max page size: %lu\n", frame_size,page_size);
        return false;
    }
    if (!is_power_two(nframes)){
        printf("ERROR: Max ring frames: %lu is not a power of 2.\n", nframes);
        return false;
    }
   if (!is_power_two(nblocks) || (nblocks == 1)){
        printf("ERROR: Max ring blocks: %lu is not a power of 2.\n", nblocks);
        return false;
    }
    /*
    *  Validate values (if we do not valaidate mmap will fail).
    */
    return status;
}
 
