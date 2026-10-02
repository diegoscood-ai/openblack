/* The public header of the library mod example.native-library: what it offers other native mods under the interface
 * name "example.counter.v1". A mod that uses it includes this header, lists "example.native-library" in its mod.json
 * "dependencies" (so it loads after it) and calls host->get_interface(EXAMPLE_COUNTER_V1, sizeof(example_counter_v1)).
 *
 * Rules of a library interface (as ob_host_api): the table starts with its size, functions are only added at the end,
 * an incompatible change is a new name (".v2"). */

#ifndef EXAMPLE_COUNTER_V1_H
#define EXAMPLE_COUNTER_V1_H

#include <stdint.h>

#define EXAMPLE_COUNTER_V1 "example.counter.v1"

typedef struct example_counter_v1
{
	uint32_t size; /* sizeof(example_counter_v1) of the library loaded */
	/* the next number of the counter called `name` (each name counts on its own, from 1) */
	int32_t (*next)(const char* name);
	/* how many counters there are */
	int32_t (*count)(void);
} example_counter_v1;

#endif
