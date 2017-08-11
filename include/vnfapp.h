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
#ifndef VNFAPP_H 
#define VNFAPP_H

typedef struct _intf_config {
	int fd;
	uint8_t *r_ring;
	uint8_t *w_ring;
	char name[IFNAMSIZ];
	unsigned long max_ring_frames;
  unsigned long max_ring_blocks;
  unsigned long max_frame_size;
  unsigned int mtu_size;
  bool single;
} intf_config_t;

typedef struct _arg_config {
  char first[IFNAMSIZ];
  char second[IFNAMSIZ];
  unsigned long max_ring_frames;
  unsigned long max_ring_blocks;
  unsigned long max_frame_size;
} arg_config_t;

/*
* Defaults for mmap buffers
*/
#define MAX_RING_FRAMES 32
#define MAX_RING_BLOCKS 2
#define MAX_FRAME_SIZE  4096

#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif


#endif /* VNFAPP_H */