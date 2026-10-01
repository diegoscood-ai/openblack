/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define SOL_ALL_SAFETIES_ON 1
#include "LuaHost.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "Mods/Api.h"
#include "Mods/Mod.h"
#include "Mods/ModLog.h"
#include "Mods/ModRegistry.h"

namespace openblack::mods::lua
{
namespace
{
constexpr int k_MaxErrors = 10;

/// One mod's script: its sandbox, its event functions and its modules
struct Script
{
	Mod* mod {nullptr};
	sol::environment env;
	std::map<std::string, std::vector<sol::protected_function>, std::less<>> handlers;
	std::map<std::string, sol::object, std::less<>> modules; ///< require cache
	int errors {0};
};

struct Host
{
	std::unique_ptr<sol::state> lua;
	std::vector<std::unique_ptr<Script>> scripts;
	/// tables offered by Lua mods (api::Interface::table points at the sol::table here)
	std::map<std::string, sol::table, std::less<>> interfaces;
};

Host& Get()
{
	static Host host;
	return host;
}

void Fail(Script& script, std::string_view where, std::string_view what)
{
	++script.errors;
	log::Error(script.mod->GetInfo().id, fmt::format("Lua {}: {}", where, what));
	if (script.errors == k_MaxErrors)
	{
		script.handlers.clear();
		log::Error(script.mod->GetInfo().id, fmt::format("{} Lua errors: its event functions are dropped", k_MaxErrors));
	}
}

template <typename... Args>
void Fire(std::string_view event, Args&&... args)
{
	for (auto& script : Get().scripts)
	{
		const auto it = script->handlers.find(event);
		if (it == script->handlers.end())
		{
			continue;
		}
		// copied: a handler may add or drop handlers
		const auto handlers = it->second;
		for (const auto& handler : handlers)
		{
			const sol::protected_function_result result = handler(args...);
			if (!result.valid())
			{
				const sol::error error = result;
				Fail(*script, event, error.what());
				if (script->errors >= k_MaxErrors)
				{
					break;
				}
			}
		}
	}
}

double ToNumber(const sol::object& value, bool& ok)
{
	ok = true;
	if (value.is<bool>())
	{
		return value.as<bool>() ? 1.0 : 0.0;
	}
	if (value.is<double>())
	{
		return value.as<double>();
	}
	ok = false;
	return 0.0;
}

sol::table EnumTable(sol::state& lua, std::string_view which)
{
	auto table = lua.create_table();
	for (const auto& [name, value] : api::Enumeration(which))
	{
		table[name] = value;
	}
	return table;
}

/// The sandbox: only what cannot reach the disk, the OS or the Lua internals
void FillSandbox(sol::state& lua, Script& script)
{
	auto& env = script.env;
	auto globals = lua.globals();
	for (const char* name :
	     {"assert", "error", "ipairs", "next", "pairs", "pcall", "select", "tonumber", "tostring", "type", "xpcall",
	      "rawequal", "rawget", "rawset", "rawlen", "setmetatable", "getmetatable", "string", "table", "math", "utf8",
	      "coroutine", "_VERSION"})
	{
		env[name] = globals[name];
	}
	auto os = lua.create_table();
	os["time"] = globals["os"]["time"];
	os["clock"] = globals["os"]["clock"];
	os["date"] = globals["os"]["date"];
	env["os"] = os;
	env["_G"] = env;

	Script* self = &script;
	env["print"] = [self](sol::variadic_args args, sol::this_state state) {
		std::string text;
		sol::state_view view(state);
		const sol::protected_function toString = view["tostring"];
		for (auto arg : args)
		{
			const sol::protected_function_result result = toString(arg);
			text += (text.empty() ? "" : "\t") + (result.valid() ? result.get<std::string>() : std::string("?"));
		}
		log::Info(self->mod->GetInfo().id, text);
	};
	// require "a.b" loads scripts/a/b.lua of the mod, once, in the same sandbox
	env["require"] = [self](const std::string& name, sol::this_state state) -> sol::object {
		if (const auto it = self->modules.find(name); it != self->modules.end())
		{
			return it->second;
		}
		std::string relative = name;
		std::ranges::replace(relative, '.', '/');
		const auto path = self->mod->GetRoot() / "scripts" / (relative + ".lua");
		sol::state_view view(state);
		sol::load_result chunk = view.load_file(path.string());
		if (!chunk.valid())
		{
			const sol::error error = chunk;
			throw sol::error(fmt::format("require '{}': {}", name, error.what()));
		}
		sol::protected_function function = chunk;
		sol::set_environment(self->env, function);
		const sol::protected_function_result result = function(name);
		if (!result.valid())
		{
			const sol::error error = result;
			throw sol::error(fmt::format("require '{}': {}", name, error.what()));
		}
		sol::object value = result.get_type() == sol::type::none || result.get_type() == sol::type::lua_nil
		                        ? sol::make_object(view, true)
		                        : result.get<sol::object>();
		self->modules[name] = value;
		return value;
	};
}

/// The `ob` table: the translation of Mods/Api.h
sol::table MakeOb(sol::state& lua, Script& script)
{
	Script* self = &script;
	Mod& mod = *script.mod;
	auto ob = lua.create_table();

	auto log = lua.create_table();
	log["info"] = [&mod](const std::string& text) { api::Log(mod, 0, text); };
	log["warn"] = [&mod](const std::string& text) { api::Log(mod, 1, text); };
	log["error"] = [&mod](const std::string& text) { api::Log(mod, 2, text); };
	ob["log"] = log;

	auto modTable = lua.create_table();
	modTable["id"] = mod.GetInfo().id;
	modTable["name"] = mod.GetInfo().name;
	modTable["version"] = mod.GetInfo().version.ToString();
	modTable["folder"] = mod.GetRoot().generic_string();
	modTable["option"] = [&mod](const std::string& option) { return api::Option(mod, option); };
	ob["mod"] = modTable;

	auto switchTable = lua.create_table();
	switchTable["get"] = [](const std::string& name) -> sol::optional<double> {
		const auto value = api::GetSwitch(name);
		return value ? sol::optional<double>(*value) : sol::nullopt;
	};
	switchTable["set"] = [&mod](const std::string& name, sol::object value) {
		bool ok = false;
		const double number = ToNumber(value, ok);
		if (!ok)
		{
			throw sol::error(fmt::format("ob.switch.set('{}'): the value must be a number or true/false", name));
		}
		return api::SetSwitch(mod, name, number);
	};
	switchTable["list"] = [](sol::this_state state) {
		sol::state_view view(state);
		auto list = view.create_table();
		for (const auto& info : api::Switches())
		{
			auto entry = view.create_table();
			entry["name"] = info.name;
			entry["type"] = info.type;
			entry["when"] = info.when;
			entry["description"] = info.description;
			entry["min"] = info.min;
			entry["max"] = info.max;
			list.add(entry);
		}
		return list;
	};
	ob["switch"] = switchTable;

	ob["on"] = [self](const std::string& event, sol::protected_function function) {
		static const std::array<std::string_view, 3> k_Events = {"turn", "frame", "land_loaded"};
		if (std::ranges::find(k_Events, event) == k_Events.end())
		{
			throw sol::error(fmt::format("ob.on: there is no event '{}' (turn, frame, land_loaded)", event));
		}
		self->handlers[event].push_back(std::move(function));
	};

	auto interfaces = lua.create_table();
	interfaces["provide"] = [&mod](const std::string& name, sol::table table) {
		auto& host = Get();
		host.interfaces[name] = table;
		api::Provide(mod, {name, mod.GetInfo().id, "lua", &host.interfaces[name], 0});
	};
	interfaces["get"] = [](const std::string& name) -> sol::optional<sol::table> {
		const auto* found = api::FindInterface(name, "lua");
		if (found == nullptr)
		{
			return sol::nullopt;
		}
		return *static_cast<const sol::table*>(found->table);
	};
	ob["interfaces"] = interfaces;

	ob["enum"] = [](const std::string& which, sol::this_state state) {
		sol::state_view view(state);
		auto table = view.create_table();
		for (const auto& [name, value] : api::Enumeration(which))
		{
			table[name] = value;
		}
		return table;
	};
	// ob.enums.meshes, ob.enums.magic...: made when first read
	auto enums = lua.create_table();
	auto enumsMeta = lua.create_table();
	enumsMeta[sol::meta_function::index] = [](sol::table /*self*/, const std::string& which, sol::this_state state) {
		sol::state_view view(state);
		return EnumTable(static_cast<sol::state&>(*Get().lua), which);
	};
	enums[sol::metatable_key] = enumsMeta;
	ob["enums"] = enums;

	auto game = lua.create_table();
	game["turn"] = []() { return api::Turn(); };
	game["hour"] = []() { return api::Hour(); };
	game["ground_height"] = [](float x, float z) -> sol::optional<float> {
		const auto height = api::GroundHeight(x, z);
		return height ? sol::optional<float>(*height) : sol::nullopt;
	};
	game["camera"] = []() {
		const auto position = api::CameraPosition();
		const auto focus = api::CameraFocus();
		return std::make_tuple(position.x, position.y, position.z, focus.x, focus.y, focus.z);
	};
	game["set_camera"] = [](float x, float y, float z, float fx, float fy, float fz) {
		api::SetCamera({x, y, z}, {fx, fy, fz});
	};
	game["cast_miracle"] = [](const std::string& magic, float x, float z, sol::optional<float> radius,
	                          sol::optional<float> seconds) {
		const auto height = api::GroundHeight(x, z);
		if (!height)
		{
			return false;
		}
		return api::CastMiracle(magic, {x, *height, z}, radius.value_or(10.0f), seconds.value_or(-1.0f));
	};
	ob["game"] = game;

	ob["api_version"] = "1.0.0";
	return ob;
}

std::unique_ptr<Script> MakeScript(sol::state& lua, Mod& mod)
{
	auto script = std::make_unique<Script>();
	script->mod = &mod;
	script->env = sol::environment(lua, sol::create);
	FillSandbox(lua, *script);
	script->env["ob"] = MakeOb(lua, *script);
	return script;
}

sol::state& State()
{
	auto& host = Get();
	if (!host.lua)
	{
		host.lua = std::make_unique<sol::state>();
		host.lua->open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::utf8,
		                         sol::lib::coroutine, sol::lib::os);
	}
	return *host.lua;
}
} // namespace

void Start(ModRegistry& registry)
{
	Stop();
	auto& lua = State();
	for (auto* mod : registry.GetLoadOrder())
	{
		const auto& entry = mod->GetInfo().luaEntry;
		if (entry.empty() || !registry.IsActive(*mod))
		{
			continue;
		}
		auto script = MakeScript(lua, *mod);
		sol::load_result chunk = lua.load_file(entry.string());
		if (!chunk.valid())
		{
			const sol::error error = chunk;
			log::Error(mod->GetInfo().id, fmt::format("Lua {}: {}", entry.filename().string(), error.what()));
			continue;
		}
		sol::protected_function function = chunk;
		sol::set_environment(script->env, function);
		const sol::protected_function_result result = function();
		if (!result.valid())
		{
			const sol::error error = result;
			log::Error(mod->GetInfo().id, fmt::format("Lua {}: {}", entry.filename().string(), error.what()));
			api::Withdraw(*mod);
			continue;
		}
		log::Info(mod->GetInfo().id, fmt::format("Lua {} running", entry.filename().string()));
		Get().scripts.push_back(std::move(script));
	}
}

void Stop()
{
	auto& host = Get();
	for (const auto& script : host.scripts)
	{
		api::Withdraw(*script->mod);
	}
	host.scripts.clear();
	host.interfaces.clear();
	host.lua.reset();
}

void OnTurn(uint32_t turn)
{
	Fire("turn", turn);
}

void OnFrame(float seconds)
{
	Fire("frame", seconds);
}

void OnLandLoaded(std::string_view land)
{
	Fire("land_loaded", std::string(land));
}

size_t Running()
{
	return Get().scripts.size();
}

std::string RunForTest(std::string_view modId, std::string_view code)
{
	auto& host = Get();
	auto it = std::ranges::find_if(host.scripts, [modId](const auto& script) { return script->mod->GetInfo().id == modId; });
	if (it == host.scripts.end())
	{
		return fmt::format("mod '{}' has no running script", modId);
	}
	auto& lua = State();
	sol::load_result chunk = lua.load(code);
	if (!chunk.valid())
	{
		const sol::error error = chunk;
		return error.what();
	}
	sol::protected_function function = chunk;
	sol::set_environment((*it)->env, function);
	const sol::protected_function_result result = function();
	if (!result.valid())
	{
		const sol::error error = result;
		return error.what();
	}
	return {};
}

} // namespace openblack::mods::lua
