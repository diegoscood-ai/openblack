/* A library mod: it changes nothing itself and offers functions to other mods ("example.counter.v1"). */

#include <string.h>

#include <example_counter_v1.h>
#include <openblack/mod_api.h>

#define MAX_COUNTERS 32

static struct
{
	char name[32];
	int32_t value;
} g_counters[MAX_COUNTERS];
static int32_t g_used = 0;

static int32_t Next(const char* name)
{
	for (int32_t i = 0; i < g_used; ++i)
	{
		if (strncmp(g_counters[i].name, name, sizeof g_counters[i].name - 1) == 0)
		{
			return ++g_counters[i].value;
		}
	}
	if (g_used == MAX_COUNTERS)
	{
		return -1;
	}
	strncpy(g_counters[g_used].name, name, sizeof g_counters[g_used].name - 1);
	g_counters[g_used].value = 1;
	return g_counters[g_used++].value;
}

static int32_t Count(void)
{
	return g_used;
}

/* static: it must stay valid while the library is loaded */
static const example_counter_v1 k_Interface = {sizeof(example_counter_v1), Next, Count};

OB_MOD_EXPORT const ob_mod_info* ob_mod_query(void)
{
	static const ob_mod_info k_Info = {sizeof(ob_mod_info), OB_MOD_API_VERSION, "example.native-library", "1.0.0"};
	return &k_Info;
}

OB_MOD_EXPORT int32_t ob_mod_load(const ob_host_api* host, ob_mod* self)
{
	if (!host->provide_interface(self, EXAMPLE_COUNTER_V1, &k_Interface, sizeof k_Interface))
	{
		return 1;
	}
	host->log(self, OB_LOG_INFO, "offering " EXAMPLE_COUNTER_V1);
	return 0;
}
