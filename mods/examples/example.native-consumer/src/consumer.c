/* A mod that uses a library mod: example.native-library's "example.counter.v1". */

#include <stdio.h>

#include <example_counter_v1.h>
#include <openblack/mod_api.h>

static const ob_host_api* g_host = NULL;
static ob_mod* g_self = NULL;
static const example_counter_v1* g_counter = NULL;

static void OnLandLoaded(void* user, int32_t event, double value)
{
	(void)user;
	(void)event;
	(void)value;
	char land[64];
	char text[160];
	g_host->land_name(land, sizeof land);
	const int32_t number = g_counter->next("lands"); /* first: C does not fix the order of a call's arguments */
	snprintf(text, sizeof text, "land %s is load number %d (counted by the library, %d counters)", land, number,
	         g_counter->count());
	g_host->log(g_self, OB_LOG_INFO, text);
}

OB_MOD_EXPORT const ob_mod_info* ob_mod_query(void)
{
	static const ob_mod_info k_Info = {sizeof(ob_mod_info), OB_MOD_API_VERSION, "example.native-consumer", "1.0.0"};
	return &k_Info;
}

OB_MOD_EXPORT int32_t ob_mod_load(const ob_host_api* host, ob_mod* self)
{
	g_host = host;
	g_self = self;
	g_counter = (const example_counter_v1*)host->get_interface(EXAMPLE_COUNTER_V1, sizeof(example_counter_v1));
	if (g_counter == NULL)
	{
		host->log(self, OB_LOG_ERROR, "the interface " EXAMPLE_COUNTER_V1 " is not there");
		return 1;
	}
	host->on_event(self, OB_EVENT_LAND_LOADED, OnLandLoaded, NULL);
	host->log(self, OB_LOG_INFO, "using " EXAMPLE_COUNTER_V1);
	return 0;
}
