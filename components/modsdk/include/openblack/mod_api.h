/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

/*
 * openblack mod API for native mods (DLL / .so): the only header a native mod needs. Plain C, so a mod built with any
 * compiler or language that can make a C library works with any openblack of the same API major version.
 *
 * A native mod is a mod folder whose mod.json has "entry": {"native": {"windows": "bin/<name>.dll", "linux":
 * "bin/<name>.so"}}. openblack loads the library of each active mod, in load order, and calls:
 *
 *   ob_mod_query()            no side effects: the API version it was built for and its id (checked first)
 *   ob_mod_load(host, self)   start: keep `host` and `self`, register events, offer or take interfaces; 0 = fine
 *   ob_mod_unload()           optional: openblack closes
 *
 * Rules:
 * - Everything happens on the game thread. A mod may run threads of its own but must not call `host` from them.
 * - Nothing of C++ crosses: no exceptions out of the mod's functions (catch them inside), no STL types.
 * - Strings given to the mod are UTF-8 and valid only during the call; strings asked from openblack go into a buffer
 *   of the mod (the function returns the length it needs, without the terminating 0).
 * - New functions are only ever added at the end of ob_host_api: a mod checks `host->struct_size` before calling one
 *   newer than its API version (OB_HOST_HAS(host, member)).
 * - Library mods: a mod offers a table of functions to others with provide_interface("name.v1", &table, sizeof table);
 *   the table starts with its own uint32_t size and only grows at the end, as ob_host_api. Other mods (which list the
 *   library in "dependencies", so they load after it) get it with get_interface. A new incompatible version is a new
 *   name ("name.v2").
 *
 * Documentation: docs/bw1-notes/mod-library.md ("Mods nativos"), examples in mods/examples/.
 */

#ifndef OPENBLACK_MOD_API_H
#define OPENBLACK_MOD_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The API major version this header describes. openblack loads mods of its own major version only. */
#define OB_MOD_API_VERSION 1

#if defined(_WIN32)
#define OB_MOD_EXPORT __declspec(dllexport)
#else
#define OB_MOD_EXPORT __attribute__((visibility("default")))
#endif

/* Does this host have that member (an older openblack has fewer functions)? */
#define OB_HOST_HAS(host, member) ((host)->struct_size >= offsetof(ob_host_api, member) + sizeof((host)->member))

/* The mod, as openblack knows it (opaque: only passed back) */
typedef struct ob_mod ob_mod;

typedef struct ob_vec3
{
	float x, y, z;
} ob_vec3;

/* log levels */
#define OB_LOG_INFO 0
#define OB_LOG_WARNING 1
#define OB_LOG_ERROR 2

/* events (on_event): `value` is the turn number for OB_EVENT_TURN, the seconds since the last frame for
 * OB_EVENT_FRAME, 0 for OB_EVENT_LAND_LOADED (its name with host->land_name) */
#define OB_EVENT_TURN 1
#define OB_EVENT_FRAME 2
#define OB_EVENT_LAND_LOADED 3
typedef void (*ob_event_fn)(void* user, int32_t event, double value);

typedef struct ob_host_api
{
	uint32_t struct_size; /* sizeof(ob_host_api) of the openblack running */
	uint32_t api_version; /* OB_MOD_API_VERSION of the openblack running */

	/* ---- the mod itself */
	void (*log)(ob_mod* self, int32_t level, const char* text);
	/* the current choice of one of its options; returns the length (0 and "" if there is no such option) */
	size_t (*get_option)(ob_mod* self, const char* option, char* buffer, size_t capacity);
	/* an engine switch while the mod is active (see the switches list in the wiki); 1 = set, 0 = no such switch */
	int32_t (*set_switch)(ob_mod* self, const char* name, double value);
	/* 1 and the value if there is such a switch */
	int32_t (*get_switch)(const char* name, double* value);
	/* an event function; 1 = registered */
	int32_t (*on_event)(ob_mod* self, int32_t event, ob_event_fn function, void* user);

	/* ---- interfaces between mods */
	/* offers `table` (it must stay valid while the mod is loaded) under `name`; 1 = done */
	int32_t (*provide_interface)(ob_mod* self, const char* name, const void* table, size_t size);
	/* the table offered under `name` by another native mod, if it is at least `min_size` bytes; NULL if none */
	const void* (*get_interface)(const char* name, size_t min_size);

	/* ---- enumerations: "meshes", "magic", "object_tables", "object_fields", "switches" */
	/* item `index` of the enumeration: 1 and its name and value, 0 past the end */
	int32_t (*enumeration)(const char* which, size_t index, char* name, size_t capacity, int64_t* value);

	/* ---- the game */
	uint32_t (*game_turn)(void);
	float (*game_hour)(void);
	/* 1 and the landscape height at x, z; 0 when no land is loaded */
	int32_t (*ground_height)(float x, float z, float* height);
	void (*camera)(ob_vec3* position, ob_vec3* focus);
	void (*set_camera)(ob_vec3 position, ob_vec3 focus);
	/* a miracle cast on the ground at x, z by the neutral player, as the script's SPELL_AT_POS (with its checks).
	 * `magic` is its info.dat name ("FIREBALL") or number; seconds < 0 = the player's cast time. 1 = cast */
	int32_t (*cast_miracle)(const char* magic, float x, float z, float radius, float seconds);
	/* the name of the land loaded last ("Land1"); returns the length */
	size_t (*land_name)(char* buffer, size_t capacity);
} ob_host_api;

typedef struct ob_mod_info
{
	uint32_t struct_size;  /* sizeof(ob_mod_info) */
	uint32_t api_version;  /* OB_MOD_API_VERSION it was built with */
	const char* id;        /* must be the id of its mod.json */
	const char* version;   /* its version, e.g. "1.0.0" */
} ob_mod_info;

/* what a native mod exports */
typedef const ob_mod_info* (*ob_mod_query_fn)(void);
typedef int32_t (*ob_mod_load_fn)(const ob_host_api* host, ob_mod* self);
typedef void (*ob_mod_unload_fn)(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENBLACK_MOD_API_H */
