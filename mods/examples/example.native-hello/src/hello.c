/* Native hello: the smallest useful native mod (openblack mod API 1, plain C). */

#include <stdio.h>
#include <stdlib.h>

#include <openblack/mod_api.h>

static const ob_host_api* g_host = NULL;
static ob_mod* g_self = NULL;
static int g_every = 100;

static void OnEvent(void* user, int32_t event, double value)
{
	(void)user;
	char text[256];
	if (event == OB_EVENT_LAND_LOADED)
	{
		char land[64];
		g_host->land_name(land, sizeof land);
		snprintf(text, sizeof text, "land %s loaded", land);
		g_host->log(g_self, OB_LOG_INFO, text);
		return;
	}
	if (event == OB_EVENT_TURN && (int)value % g_every == 0)
	{
		ob_vec3 position;
		ob_vec3 focus;
		float height = 0.0f;
		g_host->camera(&position, &focus);
		if (g_host->ground_height(focus.x, focus.z, &height))
		{
			snprintf(text, sizeof text, "turn %d, hour %.2f, ground under the camera's focus at %.1f m", (int)value,
			         g_host->game_hour(), height);
			g_host->log(g_self, OB_LOG_INFO, text);
		}
	}
}

OB_MOD_EXPORT const ob_mod_info* ob_mod_query(void)
{
	static const ob_mod_info k_Info = {sizeof(ob_mod_info), OB_MOD_API_VERSION, "example.native-hello", "1.0.0"};
	return &k_Info;
}

OB_MOD_EXPORT int32_t ob_mod_load(const ob_host_api* host, ob_mod* self)
{
	g_host = host;
	g_self = self;
	char every[32];
	host->get_option(self, "every", every, sizeof every); /* "100 turns" */
	g_every = atoi(every) > 0 ? atoi(every) : 100;
	host->on_event(self, OB_EVENT_LAND_LOADED, OnEvent, NULL);
	host->on_event(self, OB_EVENT_TURN, OnEvent, NULL);

	/* the enumerations: how many meshes openblack knows by name */
	size_t meshes = 0;
	while (host->enumeration("meshes", meshes, NULL, 0, NULL))
	{
		++meshes;
	}
	char text[128];
	snprintf(text, sizeof text, "hello from C: mod API %u, %zu mesh names, logging every %d turns", host->api_version,
	         meshes, g_every);
	host->log(self, OB_LOG_INFO, text);
	return 0;
}

OB_MOD_EXPORT void ob_mod_unload(void)
{
	if (g_host != NULL)
	{
		g_host->log(g_self, OB_LOG_INFO, "goodbye from C");
	}
}
