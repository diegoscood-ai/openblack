/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NativeHost.h"

#include <cstring>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <SDL_loadso.h>
#include <fmt/format.h>
#include <openblack/mod_api.h>

#include "Mods/Api.h"
#include "Mods/Mod.h"
#include "Mods/ModLog.h"
#include "Mods/ModRegistry.h"

// The opaque ob_mod of the C API: openblack's side of one loaded library
struct ob_mod
{
	openblack::mods::Mod* mod {nullptr};
	void* library {nullptr};
	ob_mod_unload_fn unload {nullptr};
	struct Handler
	{
		int32_t event;
		ob_event_fn function;
		void* user;
	};
	std::vector<Handler> handlers;
};

namespace openblack::mods::native
{
namespace
{
struct Host
{
	std::vector<std::unique_ptr<ob_mod>> mods;
	std::string land;
};

Host& Get()
{
	static Host host;
	return host;
}

size_t CopyOut(std::string_view text, char* buffer, size_t capacity)
{
	if (buffer != nullptr && capacity > 0)
	{
		const size_t count = std::min(text.size(), capacity - 1);
		std::memcpy(buffer, text.data(), count);
		buffer[count] = '\0';
	}
	return text.size();
}

// ---- the host functions: each one a translation of Mods/Api.h, never throwing

void HostLog(ob_mod* self, int32_t level, const char* text)
{
	if (self != nullptr && text != nullptr)
	{
		api::Log(*self->mod, level, text);
	}
}

size_t HostGetOption(ob_mod* self, const char* option, char* buffer, size_t capacity)
{
	if (self == nullptr || option == nullptr)
	{
		return CopyOut("", buffer, capacity);
	}
	return CopyOut(api::Option(*self->mod, option), buffer, capacity);
}

int32_t HostSetSwitch(ob_mod* self, const char* name, double value)
{
	return self != nullptr && name != nullptr && api::SetSwitch(*self->mod, name, value) ? 1 : 0;
}

int32_t HostGetSwitch(const char* name, double* value)
{
	if (name == nullptr)
	{
		return 0;
	}
	const auto found = api::GetSwitch(name);
	if (found && value != nullptr)
	{
		*value = *found;
	}
	return found ? 1 : 0;
}

int32_t HostOnEvent(ob_mod* self, int32_t event, ob_event_fn function, void* user)
{
	if (self == nullptr || function == nullptr || event < OB_EVENT_TURN || event > OB_EVENT_LAND_LOADED)
	{
		return 0;
	}
	self->handlers.push_back({event, function, user});
	return 1;
}

int32_t HostProvideInterface(ob_mod* self, const char* name, const void* table, size_t size)
{
	if (self == nullptr || name == nullptr || table == nullptr)
	{
		return 0;
	}
	api::Provide(*self->mod, {name, self->mod->GetInfo().id, "native", table, size});
	return 1;
}

const void* HostGetInterface(const char* name, size_t minSize)
{
	if (name == nullptr)
	{
		return nullptr;
	}
	const auto* found = api::FindInterface(name, "native");
	return found != nullptr && found->size >= minSize ? found->table : nullptr;
}

int32_t HostEnumeration(const char* which, size_t index, char* name, size_t capacity, int64_t* value)
{
	if (which == nullptr)
	{
		return 0;
	}
	const auto items = api::Enumeration(which);
	if (index >= items.size())
	{
		return 0;
	}
	CopyOut(items[index].first, name, capacity);
	if (value != nullptr)
	{
		*value = items[index].second;
	}
	return 1;
}

uint32_t HostGameTurn()
{
	return api::Turn();
}

float HostGameHour()
{
	return api::Hour();
}

int32_t HostGroundHeight(float x, float z, float* height)
{
	const auto found = api::GroundHeight(x, z);
	if (found && height != nullptr)
	{
		*height = *found;
	}
	return found ? 1 : 0;
}

void HostCamera(ob_vec3* position, ob_vec3* focus)
{
	const auto origin = api::CameraPosition();
	const auto target = api::CameraFocus();
	if (position != nullptr)
	{
		*position = {origin.x, origin.y, origin.z};
	}
	if (focus != nullptr)
	{
		*focus = {target.x, target.y, target.z};
	}
}

void HostSetCamera(ob_vec3 position, ob_vec3 focus)
{
	api::SetCamera({position.x, position.y, position.z}, {focus.x, focus.y, focus.z});
}

int32_t HostCastMiracle(const char* magic, float x, float z, float radius, float seconds)
{
	if (magic == nullptr)
	{
		return 0;
	}
	const auto height = api::GroundHeight(x, z);
	return height && api::CastMiracle(magic, {x, *height, z}, radius, seconds) ? 1 : 0;
}

size_t HostLandName(char* buffer, size_t capacity)
{
	return CopyOut(Get().land, buffer, capacity);
}

const ob_host_api& HostApi()
{
	static const ob_host_api k_Api = {
	    sizeof(ob_host_api),
	    OB_MOD_API_VERSION,
	    HostLog,
	    HostGetOption,
	    HostSetSwitch,
	    HostGetSwitch,
	    HostOnEvent,
	    HostProvideInterface,
	    HostGetInterface,
	    HostEnumeration,
	    HostGameTurn,
	    HostGameHour,
	    HostGroundHeight,
	    HostCamera,
	    HostSetCamera,
	    HostCastMiracle,
	    HostLandName,
	};
	return k_Api;
}

void Fire(int32_t event, double value)
{
	for (const auto& mod : Get().mods)
	{
		for (const auto& handler : mod->handlers)
		{
			if (handler.event == event)
			{
				handler.function(handler.user, event, value);
			}
		}
	}
}
} // namespace

void Start(ModRegistry& registry)
{
	Stop();
	std::error_code error;
	for (auto* mod : registry.GetLoadOrder())
	{
		const auto& entry = mod->GetInfo().nativeEntry;
		const auto& id = mod->GetInfo().id;
		if (entry.empty() || !registry.IsActive(*mod))
		{
			continue;
		}
		if (!std::filesystem::exists(entry, error))
		{
			log::Error(id, fmt::format("native library {} not found", entry.generic_string()));
			continue;
		}
		const auto utf8 = entry.u8string();
		void* library = SDL_LoadObject(reinterpret_cast<const char*>(utf8.c_str()));
		if (library == nullptr)
		{
			log::Error(id, fmt::format("native library {}: {}", entry.filename().string(), SDL_GetError()));
			continue;
		}
		const auto query = reinterpret_cast<ob_mod_query_fn>(SDL_LoadFunction(library, "ob_mod_query"));
		const auto load = reinterpret_cast<ob_mod_load_fn>(SDL_LoadFunction(library, "ob_mod_load"));
		const auto unload = reinterpret_cast<ob_mod_unload_fn>(SDL_LoadFunction(library, "ob_mod_unload"));
		const ob_mod_info* info = query != nullptr ? query() : nullptr;
		std::string problem;
		if (query == nullptr || load == nullptr)
		{
			problem = "it exports no ob_mod_query / ob_mod_load";
		}
		else if (info == nullptr || info->struct_size < sizeof(ob_mod_info))
		{
			problem = "ob_mod_query gave no ob_mod_info";
		}
		else if (info->api_version != OB_MOD_API_VERSION)
		{
			problem = fmt::format("built for mod API {}, this openblack has {}", info->api_version, OB_MOD_API_VERSION);
		}
		else if (info->id == nullptr || id != info->id)
		{
			problem = fmt::format("its library says it is '{}', its mod.json '{}'", info->id != nullptr ? info->id : "", id);
		}
		if (!problem.empty())
		{
			log::Error(id, fmt::format("native library {} not loaded: {}", entry.filename().string(), problem));
			SDL_UnloadObject(library);
			continue;
		}
		auto self = std::make_unique<ob_mod>();
		self->mod = mod;
		self->library = library;
		self->unload = unload;
		const auto result = load(&HostApi(), self.get());
		if (result != 0)
		{
			log::Error(id, fmt::format("ob_mod_load returned {}: not loaded", result));
			api::Withdraw(*mod);
			SDL_UnloadObject(library);
			continue;
		}
		log::Info(id, fmt::format("native library {} {} loaded", entry.filename().string(),
		                          info->version != nullptr ? info->version : ""));
		Get().mods.push_back(std::move(self));
	}
}

void Stop()
{
	auto& mods = Get().mods;
	for (auto it = mods.rbegin(); it != mods.rend(); ++it)
	{
		if ((*it)->unload != nullptr)
		{
			(*it)->unload();
		}
		api::Withdraw(*(*it)->mod);
		SDL_UnloadObject((*it)->library);
	}
	mods.clear();
}

void OnTurn(uint32_t turn)
{
	Fire(OB_EVENT_TURN, static_cast<double>(turn));
}

void OnFrame(float seconds)
{
	Fire(OB_EVENT_FRAME, static_cast<double>(seconds));
}

void OnLandLoaded(std::string_view land)
{
	Get().land = std::string(land);
	Fire(OB_EVENT_LAND_LOADED, 0.0);
}

size_t Loaded()
{
	return Get().mods.size();
}

} // namespace openblack::mods::native
