/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PSysFile.h"

#include <cstdlib>
#include <cstring>

#include <mutex>
#include <sstream>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "Common/Zip.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack::psys;

bool Object::Bool(std::string_view key, bool fallback) const
{
	const auto it = properties.find(key);
	return it == properties.end() ? fallback : it->second.integer != 0;
}

int Object::Int(std::string_view key, int fallback) const
{
	const auto it = properties.find(key);
	if (it == properties.end())
	{
		return fallback;
	}
	return it->second.type == Value::Type::Float ? static_cast<int>(it->second.number) : it->second.integer;
}

float Object::Float(std::string_view key, float fallback) const
{
	const auto it = properties.find(key);
	if (it == properties.end())
	{
		return fallback;
	}
	return it->second.type == Value::Type::Float ? it->second.number : static_cast<float>(it->second.integer);
}

std::string Object::String(std::string_view key) const
{
	const auto it = properties.find(key);
	return it == properties.end() ? std::string() : it->second.text;
}

std::vector<int> Object::Array(std::string_view key) const
{
	const auto it = properties.find(key);
	return it == properties.end() ? std::vector<int>() : it->second.array;
}

const Object* File::Find(std::string_view objectName) const
{
	if (objectName.empty())
	{
		return nullptr;
	}
	// duplicate names (12 files have some): the first one, like a name lookup would find (inf)
	for (const auto& object : objects)
	{
		if (object.name == objectName)
		{
			return &object;
		}
	}
	return nullptr;
}

namespace
{
bool ReadProperties(std::istringstream& in, Object& object)
{
	std::string token;
	while (in >> token)
	{
		if (token == "ENDPROPERTIES")
		{
			return true;
		}
		if (token != "PROPERTY")
		{
			return false;
		}
		std::string name;
		std::string type;
		in >> name >> type;
		Value value;
		if (type == "BOOL" || type == "INTEGER")
		{
			value.type = type == "BOOL" ? Value::Type::Bool : Value::Type::Integer;
			in >> value.integer;
		}
		else if (type == "FLOAT")
		{
			value.type = Value::Type::Float;
			std::string number;
			in >> number;
			value.number = std::strtof(number.c_str(), nullptr);
		}
		else if (type == "STRING" || type == "ENUM" || type == "PERSIS_PNTR")
		{
			value.type = type == "PERSIS_PNTR" ? Value::Type::Pointer : Value::Type::String;
			in >> value.text;
			if (value.text == "NULL_STRING")
			{
				value.text.clear();
			}
		}
		else if (type == "ARRAY")
		{
			value.type = Value::Type::Array;
			std::string size;
			int count = 0;
			in >> size >> count;
			value.array.resize(static_cast<size_t>(std::max(count, 0)));
			value.numbers.resize(value.array.size());
			for (size_t k = 0; k < value.array.size(); ++k)
			{
				// one token each: some arrays hold floats (KeyPoints "0 0 0.2 1 ..."), where an int read would stop
				std::string element;
				in >> element;
				value.numbers[k] = std::strtof(element.c_str(), nullptr);
				value.array[k] = static_cast<int>(value.numbers[k]);
			}
		}
		else if (type == "SOUND_ACTION")
		{
			// <SOUND> LOOPING b ONLYONE b SOFTRELEASE b USESURFACE b: the name, then the four values in that order
			// (SoundActionProperty::ReadProperty 0x585A70; psys::ReadSoundAction turns them into a PSysSoundAction)
			value.type = Value::Type::Sound;
			in >> value.text;
			std::string key;
			value.array.assign(4, 0);
			for (auto& flag : value.array)
			{
				in >> key >> flag;
			}
		}
		else
		{
			return false;
		}
		object.properties.insert_or_assign(std::move(name), std::move(value));
	}
	return false;
}
} // namespace

std::optional<File> File::Parse(std::string_view text, std::string name)
{
	std::istringstream in {std::string(text)};
	File file;
	file.name = std::move(name);
	std::string token;
	if (!(in >> token) || token != "BEGINPROPERTIES" || !ReadProperties(in, file.header))
	{
		return std::nullopt;
	}
	while (in >> token)
	{
		if (token != "BEGINCLASS")
		{
			return std::nullopt;
		}
		Object object;
		std::string begin;
		in >> object.className >> object.name >> begin;
		if (begin != "BEGINPROPERTIES" || !ReadProperties(in, object) || !(in >> token) || token != "ENDCLASS")
		{
			return std::nullopt;
		}
		file.objects.push_back(std::move(object));
	}
	return file;
}

std::shared_ptr<const File> File::Load(const std::string& name)
{
	static std::mutex mutex;
	static std::unordered_map<std::string, std::shared_ptr<const File>> cache; // PSysFileData cache 0xD4EBC0
	const std::lock_guard lock(mutex);
	if (const auto it = cache.find(name); it != cache.end())
	{
		return it->second;
	}
	std::shared_ptr<const File> result;
	auto& fileSystem = Locator::filesystem::value();
	const auto directory = fileSystem.GetPath<filesystem::Path::Data>() / "Spells" / "ZSpellFiles";
	try
	{
		std::string text;
		if (const auto loose = directory / (name + ".txt"); fileSystem.Exists(loose))
		{
			const auto bytes = fileSystem.ReadAll(loose);
			text.assign(bytes.begin(), bytes.end());
		}
		else
		{
			const auto bytes = fileSystem.ReadAll(directory / (name + "_txt.zzz"));
			if (bytes.size() > 4)
			{
				uint32_t size = 0;
				std::memcpy(&size, bytes.data(), sizeof(size));
				const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
				text.assign(inflated.begin(), inflated.end());
			}
		}
		if (auto parsed = Parse(text, name); parsed.has_value())
		{
			result = std::make_shared<const File>(std::move(*parsed));
		}
		else
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: {} is not a spell file", name);
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: cannot load {}: {}", name, e.what());
	}
	cache.emplace(name, result);
	return result;
}
