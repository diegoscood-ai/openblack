/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Replacements.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <variant>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "ModLog.h"
#include "ModRegistry.h"

namespace openblack::mods::replace
{
namespace
{
using Json = nlohmann::json;

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

/// A replacement and the mod that made it
struct Entry
{
	std::filesystem::path file;
	std::string mod;
};

struct State
{
	std::map<size_t, Entry> meshes;
	std::map<uint32_t, Entry> packTextures;
	std::map<std::string, Entry> rawTextures; ///< lower-case stem
	std::map<std::string, std::string> rawNames; ///< lower-case stem -> as the mod wrote it
	std::vector<std::filesystem::path> folders;
	/// "objects" of each active mod, in load order
	std::vector<std::pair<std::string, Json>> objects;
};

State& Get()
{
	static State state;
	return state;
}

template <typename Key>
void Put(std::map<Key, Entry>& map, const Key& key, Entry entry, std::string_view what)
{
	if (const auto it = map.find(key); it != map.end() && it->second.mod != entry.mod)
	{
		log::Warning(entry.mod, fmt::format("{} was replaced by {} too: this mod comes later and wins", what, it->second.mod));
	}
	map[key] = std::move(entry);
}

std::optional<size_t> MeshIndex(std::string_view name)
{
	if (name.starts_with('#'))
	{
		size_t index = 0;
		const auto [end, error] = std::from_chars(name.data() + 1, name.data() + name.size(), index);
		if (error == std::errc() && index < k_MeshNames.size())
		{
			return index;
		}
		return std::nullopt;
	}
	const auto lower = Lower(name);
	for (size_t i = 0; i < k_MeshNames.size(); ++i)
	{
		if (Lower(k_MeshNames[i]) == lower)
		{
			return i;
		}
	}
	return std::nullopt;
}

std::optional<uint32_t> HexId(std::string_view text)
{
	if (text.starts_with("0x") || text.starts_with("0X"))
	{
		text.remove_prefix(2);
	}
	uint32_t id = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id, 16);
	if (error != std::errc() || end != text.data() + text.size())
	{
		return std::nullopt;
	}
	return id;
}

std::filesystem::path InMod(const Mod& mod, const std::string& relative)
{
	return mod.GetRoot() / std::filesystem::path(reinterpret_cast<const char8_t*>(relative.c_str()));
}

void ReadMod(const Mod& mod, const Json& replace)
{
	auto& state = Get();
	const auto& id = mod.GetInfo().id;
	std::error_code error;
	const auto existing = [&](const std::string& relative, std::string_view what) -> std::optional<std::filesystem::path> {
		auto path = InMod(mod, relative);
		if (!std::filesystem::exists(path, error))
		{
			log::Error(id, fmt::format("{}: there is no file {}", what, log::Utf8(path)));
			return std::nullopt;
		}
		return path;
	};

	if (const auto it = replace.find("meshes"); it != replace.end() && it->is_object())
	{
		for (const auto& [name, file] : it->items())
		{
			const auto index = MeshIndex(name);
			if (!index)
			{
				log::Error(id, fmt::format("replace.meshes: there is no mesh '{}' (names of k_MeshNames, or #index)", name));
				continue;
			}
			if (!file.is_string())
			{
				continue;
			}
			if (auto path = existing(file.get<std::string>(), fmt::format("mesh {}", name)))
			{
				Put(state.meshes, *index, {std::move(*path), id}, fmt::format("mesh {}", k_MeshNames[*index]));
			}
		}
	}
	if (const auto it = replace.find("textures"); it != replace.end() && it->is_object())
	{
		for (const auto& [key, file] : it->items())
		{
			if (!file.is_string())
			{
				continue;
			}
			if (key.starts_with("pack:"))
			{
				const auto textureId = HexId(std::string_view(key).substr(5));
				if (!textureId)
				{
					log::Error(id, fmt::format("replace.textures: '{}' needs a hex texture id (pack:47)", key));
					continue;
				}
				if (auto path = existing(file.get<std::string>(), key))
				{
					Put(state.packTextures, *textureId, {std::move(*path), id}, key);
				}
			}
			else if (key.starts_with("raw:"))
			{
				const auto stem = key.substr(4);
				if (auto path = existing(file.get<std::string>(), key))
				{
					Put(state.rawTextures, Lower(stem), {std::move(*path), id}, key);
					state.rawNames[Lower(stem)] = stem;
				}
			}
			else
			{
				log::Error(id, fmt::format("replace.textures: '{}' must start with pack: or raw:", key));
			}
		}
	}
	if (const auto it = replace.find("objects"); it != replace.end())
	{
		if (it->is_object())
		{
			if (!it->empty())
			{
				state.objects.emplace_back(id, *it);
			}
		}
		else if (it->is_string())
		{
			if (const auto path = existing(it->get<std::string>(), "replace.objects"))
			{
				std::ifstream file(*path, std::ios::binary);
				std::ostringstream text;
				text << file.rdbuf();
				auto objects = Json::parse(text.str(), nullptr, false, true);
				if (objects.is_object())
				{
					state.objects.emplace_back(id, std::move(objects));
				}
				else
				{
					log::Error(id, fmt::format("{} is not a JSON object", path->generic_string()));
				}
			}
		}
	}
}

// ---- info.dat objects

/// The GObjectInfo fields every object has (InfoConstants.h), by their C++ name
using ObjectField = std::variant<float GObjectInfo::*, uint32_t GObjectInfo::*>;
const std::vector<std::pair<std::string_view, ObjectField>>& CommonFields()
{
	static const std::vector<std::pair<std::string_view, ObjectField>> k_Fields = {
	    {"foodValue", &GObjectInfo::foodValue},
	    {"woodValue", &GObjectInfo::woodValue},
	    {"weight", &GObjectInfo::weight},
	    {"heatCapacity", &GObjectInfo::heatCapacity},
	    {"combustionTemperature", &GObjectInfo::combustionTemperature},
	    {"burningPriority", &GObjectInfo::burningPriority},
	    {"defenceEffectBurn", &GObjectInfo::defenceEffectBurn},
	    {"defenceEffectCrush", &GObjectInfo::defenceEffectCrush},
	    {"defenceEffectHit", &GObjectInfo::defenceEffectHit},
	    {"defenceEffectHeal", &GObjectInfo::defenceEffectHeal},
	    {"defenceEffectFlyAway", &GObjectInfo::defenceEffectFlyAway},
	    {"defenceMultiplierBurn", &GObjectInfo::defenceMultiplierBurn},
	    {"defenceMultiplierCrush", &GObjectInfo::defenceMultiplierCrush},
	    {"defenceMultiplierHit", &GObjectInfo::defenceMultiplierHit},
	    {"defenceMultiplierHeal", &GObjectInfo::defenceMultiplierHeal},
	    {"defenceMultiplierFlyAway", &GObjectInfo::defenceMultiplierFlyAway},
	    {"canCreatureUseForBuilding", &GObjectInfo::canCreatureUseForBuilding},
	    {"canCreatureInteractWithMe", &GObjectInfo::canCreatureInteractWithMe},
	    {"canCreatureAttackMe", &GObjectInfo::canCreatureAttackMe},
	    {"canCreaturePlayWithMe", &GObjectInfo::canCreaturePlayWithMe},
	    {"villagerInteractDesire", &GObjectInfo::villagerInteractDesire},
	    {"sacrificeValue", &GObjectInfo::sacrificeValue},
	    {"impressiveValue", &GObjectInfo::impressiveValue},
	    {"aggressorValue", &GObjectInfo::aggressorValue},
	    {"villagerImpressiveValue", &GObjectInfo::villagerImpressiveValue},
	    {"artifactMultiplier", &GObjectInfo::artifactMultiplier},
	    {"drawImportance", &GObjectInfo::drawImportance},
	    {"computerAttackDesire", &GObjectInfo::computerAttackDesire},
	};
	return k_Fields;
}

constexpr std::array<std::string_view, 9> k_MeshFields = {"meshId", "normal", "growing", "burning", "high",
                                                          "std",    "low",    "startScale", "finalScale"};

std::optional<MeshId> MeshValue(const Json& value)
{
	if (value.is_number_integer())
	{
		const auto index = value.get<int64_t>();
		if (index >= 0 && static_cast<size_t>(index) < k_MeshNames.size())
		{
			return static_cast<MeshId>(index);
		}
		return std::nullopt;
	}
	if (value.is_string())
	{
		if (const auto index = MeshIndex(value.get<std::string>()))
		{
			return static_cast<MeshId>(*index);
		}
	}
	return std::nullopt;
}

/// One field of one object: a common one, or a mesh / scale field of the tables that have it
template <typename T>
bool SetField(T& info, std::string_view field, const Json& value, std::string& error)
{
	for (const auto& [name, member] : CommonFields())
	{
		if (name != field)
		{
			continue;
		}
		if (!value.is_number() && !value.is_boolean())
		{
			error = fmt::format("{} needs a number", field);
			return false;
		}
		const double number = value.is_boolean() ? (value.get<bool>() ? 1.0 : 0.0) : value.get<double>();
		bool ok = true;
		std::visit(
		    [&info, number, &ok, &error, field](auto pointer) {
			    using Field = std::remove_reference_t<decltype(static_cast<GObjectInfo&>(info).*pointer)>;
			    if constexpr (std::is_unsigned_v<Field>)
			    {
				    if (number < 0.0 || number > 4294967295.0)
				    {
					    error = fmt::format("{} must be 0 or more", field);
					    ok = false;
					    return;
				    }
			    }
			    static_cast<GObjectInfo&>(info).*pointer = static_cast<Field>(number);
		    },
		    member);
		return ok;
	}
	const auto mesh = [&](MeshId& target) {
		if (const auto id = MeshValue(value))
		{
			target = *id;
			return true;
		}
		error = fmt::format("{}: there is no mesh {}", field, value.dump());
		return false;
	};
	if constexpr (requires { info.meshId; })
	{
		if (field == "meshId")
		{
			return mesh(info.meshId);
		}
	}
	if constexpr (requires { info.normal; })
	{
		if (field == "normal")
		{
			return mesh(info.normal);
		}
	}
	if constexpr (requires { info.growing; })
	{
		if (field == "growing")
		{
			return mesh(info.growing);
		}
	}
	if constexpr (requires { info.burning; })
	{
		if (field == "burning")
		{
			return mesh(info.burning);
		}
	}
	if constexpr (requires { info.high; info.low; })
	{
		if (field == "high")
		{
			return mesh(info.high);
		}
		if (field == "std")
		{
			return mesh(info.std);
		}
		if (field == "low")
		{
			return mesh(info.low);
		}
	}
	if constexpr (requires { info.startScale; info.finalScale; })
	{
		if ((field == "startScale" || field == "finalScale") && value.is_number())
		{
			(field == "startScale" ? info.startScale : info.finalScale) = value.get<float>();
			return true;
		}
	}
	error = fmt::format("objects of this table have no field '{}'", field);
	return false;
}

template <typename T, size_t N>
size_t PatchTable(std::array<T, N>& table, std::string_view tableName, const Json& entries, const std::string& mod)
{
	size_t changed = 0;
	for (const auto& [name, fields] : entries.items())
	{
		if (!fields.is_object())
		{
			continue;
		}
		const auto lower = Lower(name);
		size_t matches = 0;
		for (auto& info : table)
		{
			bool match = Lower(info.debugString.data()) == lower;
			// abodes also by "<TRIBE>_<name>", as the scripts name them (GAbodeInfo::GetInfoFromText 0x405A70): the plain
			// name is the abode of every tribe
			if constexpr (requires { info.tribeType; })
			{
				if (!match && info.tribeType != Tribe::NONE && static_cast<size_t>(info.tribeType) < k_TribeStrs.size())
				{
					match = Lower(fmt::format("{}_{}", k_TribeStrs.at(static_cast<size_t>(info.tribeType)),
					                          info.debugString.data())) == lower;
				}
			}
			if (!match)
			{
				continue;
			}
			++matches;
			for (const auto& [field, value] : fields.items())
			{
				std::string error;
				if (SetField(info, field, value, error))
				{
					++changed;
				}
				else if (matches == 1)
				{
					log::Error(mod, fmt::format("objects.{}.{}: {}", tableName, name, error));
				}
			}
		}
		if (matches == 0)
		{
			log::Error(mod, fmt::format("objects.{}: there is no object '{}' (its debug name in info.dat)", tableName, name));
		}
	}
	return changed;
}

struct Table
{
	std::string_view name;
	size_t (*patch)(InfoConstants&, std::string_view, const Json&, const std::string&);
};

#define OPENBLACK_OBJECT_TABLE(member)                                                                                       \
	Table                                                                                                                    \
	{                                                                                                                        \
		#member, [](InfoConstants& info, std::string_view name, const Json& entries, const std::string& mod) {               \
			return PatchTable(info.member, name, entries, mod);                                                             \
		}                                                                                                                    \
	}

const std::array<Table, 10>& Tables()
{
	static const std::array<Table, 10> k_Tables = {
	    OPENBLACK_OBJECT_TABLE(feature),      OPENBLACK_OBJECT_TABLE(abode),      OPENBLACK_OBJECT_TABLE(mobileStatic),
	    OPENBLACK_OBJECT_TABLE(mobileObject), OPENBLACK_OBJECT_TABLE(pot),        OPENBLACK_OBJECT_TABLE(tree),
	    OPENBLACK_OBJECT_TABLE(animatedStatic), OPENBLACK_OBJECT_TABLE(animal),   OPENBLACK_OBJECT_TABLE(bigForest),
	    OPENBLACK_OBJECT_TABLE(fieldType),
	};
	return k_Tables;
}
#undef OPENBLACK_OBJECT_TABLE
} // namespace

void Collect(const ModRegistry& registry)
{
	Clear();
	std::error_code error;
	for (const auto* mod : registry.GetLoadOrder())
	{
		if (!registry.IsActive(*mod))
		{
			continue;
		}
		if (mod->GetInfo().kind == Mod::Kind::Package && std::filesystem::is_directory(mod->GetRoot() / "replace", error))
		{
			Get().folders.push_back(mod->GetRoot() / "replace");
		}
		if (mod->GetInfo().replaceJson.empty())
		{
			continue;
		}
		const auto replace = Json::parse(mod->GetInfo().replaceJson, nullptr, false);
		if (replace.is_object())
		{
			ReadMod(*mod, replace);
		}
	}
	const auto& state = Get();
	if (!state.meshes.empty() || !state.packTextures.empty() || !state.rawTextures.empty() || !state.objects.empty() ||
	    !state.folders.empty())
	{
		log::Info("", fmt::format("replacements: {} meshes, {} pack textures, {} raw textures, objects from {} mods, {} "
		                          "replace/ folders",
		                          state.meshes.size(), state.packTextures.size(), state.rawTextures.size(),
		                          state.objects.size(), state.folders.size()));
	}
}

void Clear()
{
	Get() = {};
}

std::optional<std::filesystem::path> Mesh(size_t index)
{
	const auto& meshes = Get().meshes;
	const auto it = meshes.find(index);
	return it != meshes.end() ? std::optional(it->second.file) : std::nullopt;
}

std::optional<std::filesystem::path> PackTexture(uint32_t id)
{
	const auto& textures = Get().packTextures;
	const auto it = textures.find(id);
	return it != textures.end() ? std::optional(it->second.file) : std::nullopt;
}

std::optional<std::filesystem::path> RawTexture(std::string_view stem)
{
	const auto& textures = Get().rawTextures;
	const auto it = textures.find(Lower(stem));
	return it != textures.end() ? std::optional(it->second.file) : std::nullopt;
}

std::vector<std::string> RawTextureNames()
{
	std::vector<std::string> names;
	for (const auto& [lower, name] : Get().rawNames)
	{
		names.push_back(name);
	}
	return names;
}

const std::vector<std::filesystem::path>& Folders()
{
	return Get().folders;
}

bool HasObjectPatches()
{
	return !Get().objects.empty();
}

size_t PatchObjects(InfoConstants& info)
{
	size_t changed = 0;
	for (const auto& [mod, objects] : Get().objects)
	{
		for (const auto& [tableName, entries] : objects.items())
		{
			const auto table = std::ranges::find(Tables(), tableName, &Table::name);
			if (table == Tables().end())
			{
				log::Error(mod, fmt::format("objects: there is no table '{}'", tableName));
				continue;
			}
			if (entries.is_object())
			{
				changed += table->patch(info, table->name, entries, mod);
			}
		}
	}
	if (changed > 0)
	{
		log::Info("", fmt::format("info.dat: {} object fields changed by mods", changed));
	}
	return changed;
}

std::vector<std::string_view> ObjectTables()
{
	std::vector<std::string_view> names;
	for (const auto& table : Tables())
	{
		names.push_back(table.name);
	}
	return names;
}

std::vector<std::string_view> ObjectFields()
{
	std::vector<std::string_view> names;
	for (const auto& [name, member] : CommonFields())
	{
		names.push_back(name);
	}
	names.insert(names.end(), k_MeshFields.begin(), k_MeshFields.end());
	return names;
}

} // namespace openblack::mods::replace
