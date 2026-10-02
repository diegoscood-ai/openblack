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
#include <fstream>
#include <map>
#include <sstream>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "Mods/Api.h"
#include "Mods/Manifest.h"
#include "Mods/Mod.h"
#include "Mods/ModLog.h"
#include "Mods/ModRegistry.h"

namespace openblack::mods::lua
{
namespace
{
// Host rules, not from the original game: after this many errors a script's event functions are dropped, and a call
// into a script may run this many blocks of 1000 Lua instructions (about 20 million) before it is stopped
constexpr int k_MaxErrors = 10;
constexpr int64_t k_InstructionBlocks = 20000;
int64_t g_budget = k_InstructionBlocks;

/// The count hook: every 1000 instructions; out of budget, the running call fails (the game goes on)
void BudgetHook(lua_State* state, lua_Debug* /*debug*/)
{
	if (--g_budget < 0)
	{
		g_budget = k_InstructionBlocks; // so the error handler itself can run
		luaL_error(state, "the script ran too long (more than %d million instructions in one call)",
		           static_cast<int>(k_InstructionBlocks / 1000));
	}
}

/// A fresh budget before each call from openblack into a script
void Budget()
{
	g_budget = k_InstructionBlocks;
}

/// A script file as text (read with the wide path, so any folder name works); precompiled Lua (bytecode, which Lua
/// does not verify) is refused
std::optional<std::string> ReadScript(const std::filesystem::path& path, std::string& error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		error = "cannot open " + log::Utf8(path.filename());
		return std::nullopt;
	}
	std::ostringstream text;
	text << file.rdbuf();
	auto code = text.str();
	if (code.starts_with("\x1bLua"))
	{
		error = log::Utf8(path.filename()) + " is precompiled Lua: only source scripts are run";
		return std::nullopt;
	}
	return code;
}

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
	ModRegistry* registry {nullptr};
	std::vector<std::unique_ptr<Script>> scripts;
	/// scripts whose main chunk failed: no events, but kept until Stop, as functions they made may still be held by
	/// other mods
	std::vector<std::unique_ptr<Script>> failed;
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
	auto& host = Get();
	for (auto& script : host.scripts)
	{
		// a mod switched off (or blocked) in the window gets no events until it is on again
		if (host.registry != nullptr && !host.registry->IsActive(*script->mod))
		{
			continue;
		}
		const auto it = script->handlers.find(event);
		if (it == script->handlers.end())
		{
			continue;
		}
		// copied: a handler may add or drop handlers
		const auto handlers = it->second;
		for (const auto& handler : handlers)
		{
			Budget();
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
	for (const char* name : {"assert", "error", "ipairs", "next", "pairs", "pcall", "select", "tonumber", "tostring",
	                         "type", "xpcall", "rawequal", "rawget", "rawset", "rawlen", "setmetatable", "getmetatable",
	                         "_VERSION"})
	{
		env[name] = globals[name];
	}
	// the libraries as copies, so a mod that changes string.format changes only its own (the strings' metatable is
	// still the shared string library: a mod must not change it, the wiki says so)
	for (const char* name : {"string", "table", "math", "utf8", "coroutine"})
	{
		auto copy = lua.create_table();
		for (const auto& [key, value] : globals[name].get<sol::table>())
		{
			copy[key] = value;
		}
		env[name] = copy;
	}
	env["string"]["dump"] = sol::lua_nil;
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
		// only scripts/ of this mod: "a.b" is scripts/a/b.lua; no paths, drives or ".."
		if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name.find("..") != std::string::npos ||
		    name.front() == '.' || name.back() == '.')
		{
			throw sol::error(fmt::format("require '{}': a module is a name like \"util\" or \"lib.math\"", name));
		}
		std::string relative = name;
		std::ranges::replace(relative, '.', '/');
		const auto path = self->mod->GetRoot() / "scripts" / (relative + ".lua");
		sol::state_view view(state);
		std::string readError;
		const auto code = ReadScript(path, readError);
		if (!code)
		{
			throw sol::error(fmt::format("require '{}': {}", name, readError));
		}
		sol::load_result chunk = view.load(*code, "@" + relative + ".lua", sol::load_mode::text);
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
	modTable["folder"] = log::Utf8(mod.GetRoot());
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
	// radius 10 m when not given: the default of the hook OPENBLACK_TEST_SPELL (MagicDebugHooks.cpp), not original
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

	ob["api_version"] = k_ApiVersion.ToString();
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
		lua_sethook(host.lua->lua_state(), BudgetHook, LUA_MASKCOUNT, 1000);
	}
	return *host.lua;
}
} // namespace

void Start(ModRegistry& registry)
{
	Stop();
	auto& lua = State();
	Get().registry = &registry;
	for (auto* mod : registry.GetLoadOrder())
	{
		const auto& entry = mod->GetInfo().luaEntry;
		if (entry.empty() || !registry.IsActive(*mod))
		{
			continue;
		}
		auto script = MakeScript(lua, *mod);
		const auto fileName = log::Utf8(entry.filename());
		std::string readError;
		const auto code = ReadScript(entry, readError);
		if (!code)
		{
			log::Error(mod->GetInfo().id, fmt::format("Lua: {}", readError));
			continue;
		}
		sol::load_result chunk = lua.load(*code, "@" + fileName, sol::load_mode::text);
		if (!chunk.valid())
		{
			const sol::error error = chunk;
			log::Error(mod->GetInfo().id, fmt::format("Lua {}: {}", fileName, error.what()));
			continue;
		}
		sol::protected_function function = chunk;
		sol::set_environment(script->env, function);
		Budget();
		const sol::protected_function_result result = function();
		if (!result.valid())
		{
			const sol::error error = result;
			log::Error(mod->GetInfo().id, fmt::format("Lua {}: {}", fileName, error.what()));
			api::Withdraw(*mod);
			script->handlers.clear();
			Get().failed.push_back(std::move(script));
			continue;
		}
		log::Info(mod->GetInfo().id, fmt::format("Lua {} running", fileName));
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
	host.failed.clear();
	host.interfaces.clear();
	host.lua.reset();
	host.registry = nullptr;
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
	sol::load_result chunk = lua.load(code, "=test", sol::load_mode::text);
	if (!chunk.valid())
	{
		const sol::error error = chunk;
		return error.what();
	}
	sol::protected_function function = chunk;
	sol::set_environment((*it)->env, function);
	Budget();
	const sol::protected_function_result result = function();
	if (!result.valid())
	{
		const sol::error error = result;
		return error.what();
	}
	return {};
}

} // namespace openblack::mods::lua
