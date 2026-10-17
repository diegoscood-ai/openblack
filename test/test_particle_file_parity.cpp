/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The psys component's readers (ParticleFile, EnumHeader, StackedBitmap) against the readers they replaced, kept here
// unchanged as the reference: the same parsed data on synthetic files and, when the game is installed, on every spell
// file, enum header and light map the game ships.

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <EnumHeader.h>
#include <ParticleFile.h>
#include <StackedBitmap.h>
#include <gtest/gtest.h>

#include "Common/Zip.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;

namespace
{
// ---------------------------------------------------------------------------------------------------------------------
// The replaced readers, as they were

namespace reference
{
struct Value
{
	enum class Type
	{
		Bool,
		Integer,
		Float,
		String,
		Pointer,
		Array,
		Sound,
	};
	Type type {Type::Integer};
	int integer {0};
	float number {0.0f};
	std::string text;
	std::vector<int> array;
	std::vector<float> numbers;
};

struct Object
{
	std::string className;
	std::string name;
	std::map<std::string, Value, std::less<>> properties;

	[[nodiscard]] bool Bool(std::string_view key, bool fallback) const
	{
		const auto it = properties.find(key);
		return it == properties.end() ? fallback : it->second.integer != 0;
	}
	[[nodiscard]] int Int(std::string_view key, int fallback) const
	{
		const auto it = properties.find(key);
		if (it == properties.end())
		{
			return fallback;
		}
		return it->second.type == Value::Type::Float ? static_cast<int>(it->second.number) : it->second.integer;
	}
	[[nodiscard]] float Float(std::string_view key, float fallback) const
	{
		const auto it = properties.find(key);
		if (it == properties.end())
		{
			return fallback;
		}
		return it->second.type == Value::Type::Float ? it->second.number : static_cast<float>(it->second.integer);
	}
	[[nodiscard]] std::string String(std::string_view key) const
	{
		const auto it = properties.find(key);
		return it == properties.end() ? std::string() : it->second.text;
	}
	[[nodiscard]] std::vector<int> Array(std::string_view key) const
	{
		const auto it = properties.find(key);
		return it == properties.end() ? std::vector<int>() : it->second.array;
	}
};

struct File
{
	Object header;
	std::vector<Object> objects;
};

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
				std::string element;
				in >> element;
				value.numbers[k] = std::strtof(element.c_str(), nullptr);
				value.array[k] = static_cast<int>(value.numbers[k]);
			}
		}
		else if (type == "SOUND_ACTION")
		{
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

std::optional<File> Parse(std::string_view text)
{
	std::istringstream in {std::string(text)};
	File file;
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

std::vector<std::pair<std::string, int32_t>> ParseEnumHeader(std::string_view text)
{
	std::string code;
	code.reserve(text.size());
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/')
		{
			while (i < text.size() && text[i] != '\n')
			{
				++i;
			}
		}
		else if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*')
		{
			const auto end = text.find("*/", i + 2);
			i = end == std::string_view::npos ? text.size() : end + 1;
			continue;
		}
		if (i < text.size())
		{
			code.push_back(text[i]);
		}
	}
	std::vector<std::pair<std::string, int32_t>> result;
	const auto keyword = code.find("enum");
	const auto open = keyword == std::string::npos ? std::string::npos : code.find('{', keyword);
	const auto close = open == std::string::npos ? std::string::npos : code.find('}', open);
	if (close == std::string::npos)
	{
		return result;
	}
	const std::string_view body(code.data() + open + 1, close - open - 1);
	const auto trim = [](std::string_view s) {
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0)
		{
			s.remove_prefix(1);
		}
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0)
		{
			s.remove_suffix(1);
		}
		return s;
	};
	int32_t next = 0;
	size_t start = 0;
	while (start <= body.size())
	{
		auto end = body.find(',', start);
		if (end == std::string_view::npos)
		{
			end = body.size();
		}
		std::string_view name = trim(body.substr(start, end - start));
		if (const auto equals = name.find('='); equals != std::string_view::npos)
		{
			const std::string number(trim(name.substr(equals + 1)));
			next = static_cast<int32_t>(std::strtol(number.c_str(), nullptr, 0));
			name = trim(name.substr(0, equals));
		}
		if (!name.empty())
		{
			result.emplace_back(std::string(name), next++);
		}
		start = end + 1;
	}
	return result;
}

struct Bitmap
{
	int pitch {0};
	int frames {0};
	int channels {0};
	std::vector<uint8_t> data;
};

std::optional<Bitmap> LoadBitmap(const std::vector<uint8_t>& bytes, int pitch, int bpp, int framesInFile, int framesInUse)
{
	if (pitch <= 0 || bpp <= 0 || framesInFile <= 0)
	{
		return std::nullopt;
	}
	const auto frameSize = static_cast<size_t>(pitch) * static_cast<size_t>(pitch) * static_cast<size_t>(bpp);
	if (bytes.size() != frameSize * static_cast<size_t>(framesInFile))
	{
		return std::nullopt;
	}
	Bitmap bitmap {.pitch = pitch, .frames = std::max(std::min(framesInUse, framesInFile), 0), .channels = bpp};
	bitmap.data.resize(frameSize * static_cast<size_t>(bitmap.frames));
	const auto perRow = std::max(1, static_cast<int>(std::sqrt(static_cast<float>(framesInFile))));
	size_t out = 0;
	for (int frame = 0; frame < bitmap.frames; ++frame)
	{
		const int column = frame % perRow;
		const int row = frame / perRow;
		for (int y = 0; y < pitch; ++y)
		{
			for (int x = 0; x < pitch; ++x)
			{
				const auto source = ((static_cast<size_t>(row) * static_cast<size_t>(pitch) + static_cast<size_t>(y)) *
				                         static_cast<size_t>(perRow) +
				                     static_cast<size_t>(column)) *
				                        static_cast<size_t>(pitch) +
				                    static_cast<size_t>(x);
				for (int c = 0; c < bpp; ++c)
				{
					const auto at = source * static_cast<size_t>(bpp) + static_cast<size_t>(c);
					bitmap.data[out++] = at < bytes.size() ? bytes[at] : 0;
				}
			}
		}
	}
	return bitmap;
}
} // namespace reference

// ---------------------------------------------------------------------------------------------------------------------
// The comparisons

bool SameFloat(float a, float b)
{
	return std::memcmp(&a, &b, sizeof(float)) == 0;
}

psys::Property::Type TypeOf(reference::Value::Type type)
{
	using Old = reference::Value::Type;
	using New = psys::Property::Type;
	switch (type)
	{
	case Old::Bool:
		return New::Bool;
	case Old::Integer:
		return New::Integer;
	case Old::Float:
		return New::Float;
	case Old::String:
		return New::String;
	case Old::Pointer:
		return New::Reference;
	case Old::Array:
		return New::Array;
	case Old::Sound:
		return New::SoundAction;
	}
	return New::Integer;
}

/// Every property and every accessor the effects read gives the same value
void ExpectSameObject(const reference::Object& old, const psys::ParticleObject& now, const std::string& where)
{
	EXPECT_EQ(old.className, now.className) << where;
	EXPECT_EQ(old.name, now.name) << where;
	ASSERT_EQ(old.properties.size(), now.properties.size()) << where;
	for (const auto& [key, value] : old.properties)
	{
		const auto at = where + " " + old.name + "." + key;
		const auto it = now.properties.find(key);
		ASSERT_NE(it, now.properties.end()) << at;
		const auto& property = it->second;
		EXPECT_EQ(TypeOf(value.type), property.type) << at;
		EXPECT_EQ(value.integer, property.integer) << at;
		EXPECT_TRUE(SameFloat(value.number, property.number)) << at;
		EXPECT_EQ(value.text, property.text) << at;
		ASSERT_EQ(value.numbers.size(), property.numbers.size()) << at;
		for (size_t i = 0; i < value.numbers.size(); ++i)
		{
			EXPECT_TRUE(SameFloat(value.numbers[i], property.numbers[i])) << at << "[" << i << "]";
		}
		if (value.type == reference::Value::Type::Sound)
		{
			const auto sound = now.Sound(key);
			EXPECT_EQ(value.text, sound.sound) << at;
			ASSERT_EQ(value.array.size(), 4u) << at;
			EXPECT_EQ(value.array[0] != 0, sound.looping) << at;
			EXPECT_EQ(value.array[1] != 0, sound.onlyOne) << at;
			EXPECT_EQ(value.array[2] != 0, sound.softRelease) << at;
			EXPECT_EQ(value.array[3] != 0, sound.useSurface) << at;
		}
		else
		{
			EXPECT_EQ(old.Array(key), now.IntArray(key)) << at;
		}
		EXPECT_EQ(old.Int(key, -7), now.Int(key, -7)) << at;
		EXPECT_TRUE(SameFloat(old.Float(key, -7.0f), now.Float(key, -7.0f))) << at;
		EXPECT_EQ(old.String(key), now.String(key)) << at;
		// the effects read Bool only from BOOL and INTEGER properties, where both read the whole number
		if (value.type != reference::Value::Type::Float)
		{
			EXPECT_EQ(old.Bool(key, true), now.Bool(key, true)) << at;
			EXPECT_EQ(old.Bool(key, false), now.Bool(key, false)) << at;
		}
	}
}

void ExpectSameParse(std::string_view text, const std::string& where)
{
	const auto old = reference::Parse(text);
	const auto now = psys::ParticleFile::Parse(text);
	ASSERT_EQ(old.has_value(), now.has_value()) << where;
	if (!old.has_value())
	{
		return;
	}
	ExpectSameObject(old->header, now->header, where);
	ASSERT_EQ(old->objects.size(), now->objects.size()) << where;
	for (size_t i = 0; i < old->objects.size(); ++i)
	{
		ExpectSameObject(old->objects[i], now->objects[i], where);
	}
}

void ExpectSameBitmap(const std::vector<uint8_t>& bytes, int pitch, int channels, int framesInFile, int framesInUse)
{
	const auto old = reference::LoadBitmap(bytes, pitch, channels, framesInFile, framesInUse);
	const auto now = psys::LoadStackedBitmap(bytes, pitch, channels, framesInFile, framesInUse);
	ASSERT_EQ(old.has_value(), now.has_value());
	if (!old.has_value())
	{
		return;
	}
	EXPECT_EQ(old->pitch, now->pitch);
	EXPECT_EQ(old->frames, now->frames);
	EXPECT_EQ(old->channels, now->channels);
	EXPECT_EQ(old->data, now->data);
}

std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::optional<std::filesystem::path> GameData()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return std::filesystem::path(game) / "Data";
}

/// One property of every type and every layout the game's files use
constexpr std::string_view k_Sample = "BEGINPROPERTIES\r\n"
                                      "PROPERTY DeleteOnCloseDown BOOL 1\r\n"
                                      "PROPERTY Hierarchies ARRAY SIZE 3 1 0 1 \r\n"
                                      "PROPERTY InitiallyCreated ARRAY SIZE 0 \r\n"
                                      "PROPERTY MaxSpellAge FLOAT -1\r\n"
                                      "ENDPROPERTIES\r\n"
                                      "BEGINCLASS DiskEmitter Emitter0\r\n"
                                      "BEGINPROPERTIES\r\n"
                                      "PROPERTY Condition PERSIS_PNTR NULL_STRING\r\n"
                                      "PROPERTY EmissionFreq FLOAT 2.5\r\n"
                                      "PROPERTY Maximum FLOAT 1e+006\r\n"
                                      "PROPERTY Tiny FLOAT -3.5e-005\r\n"
                                      "PROPERTY Group INTEGER -12\r\n"
                                      "PROPERTY PCreator PERSIS_PNTR Sprite0\r\n"
                                      "PROPERTY KeyPoints ARRAY SIZE 4 0 0.5 1.25 -2 \r\n"
                                      "PROPERTY MeshEnum ENUM MSH_INVALID\r\n"
                                      "PROPERTY Texture STRING NULL_STRING\r\n"
                                      "PROPERTY SoundEmission SOUND_ACTION SOUND_FIZZ LOOPING 1 ONLYONE 0 SOFTRELEASE 1 "
                                      "USESURFACE 0\r\n"
                                      "PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 1 SOFTRELEASE 0 "
                                      "USESURFACE 1\r\n"
                                      "PROPERTY Group INTEGER 3\r\n"
                                      "ENDPROPERTIES\r\n"
                                      "ENDCLASS\r\n"
                                      "BEGINCLASS ParticleSpriteCreator Sprite0\r\n"
                                      "BEGINPROPERTIES\r\n"
                                      "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet3.raw\r\n"
                                      "PROPERTY UseAdditiveAlpha BOOL 0\r\n"
                                      "ENDPROPERTIES\r\n"
                                      "ENDCLASS\r\n"
                                      "BEGINCLASS ParticleSpriteCreator Sprite0\r\n"
                                      "BEGINPROPERTIES\r\n"
                                      "ENDPROPERTIES\r\n"
                                      "ENDCLASS\r\n";
} // namespace

TEST(ParticleFileParity, syntheticFile)
{
	ExpectSameParse(k_Sample, "sample");
	// LF line ends and tabs, as a hand-edited loose .txt may have
	std::string unix(k_Sample);
	std::erase(unix, '\r');
	std::ranges::replace(unix, ' ', '\t');
	ExpectSameParse(unix, "sample, LF and tabs");
}

TEST(ParticleFileParity, edgeCases)
{
	for (const std::string_view text : {
	         std::string_view {""},
	         std::string_view {"hello"},
	         std::string_view {"BEGINPROPERTIES PROPERTY A COLOUR 1 ENDPROPERTIES"},
	         std::string_view {"BEGINPROPERTIES ENDPROPERTIES BEGINCLASS A B BEGINPROPERTIES ENDPROPERTIES"},
	         std::string_view {"BEGINPROPERTIES ENDPROPERTIES BEGINCLASS A B BEGINPROPERTIES ENDPROPERTIES ENDCLASS 1"},
	         std::string_view {"BEGINPROPERTIES PROPERTY A INTEGER x ENDPROPERTIES"},
	         std::string_view {"BEGINPROPERTIES PROPERTY A ARRAY SIZE 3 1 2"},
	         std::string_view {"BEGINPROPERTIES ENDPROPERTIES"},
	     })
	{
		ExpectSameParse(text, std::string(text));
	}
}

TEST(ParticleFileParity, syntheticEnumHeader)
{
	// the layouts of Data\AllMeshes.h and Data\SoundAction.h: aligned initialisers with comments, then bare names
	constexpr std::string_view k_Header = "#ifndef X\n#define X\n// 3 Meshes.\n\nenum MESH_LIST\n{\n"
	                                      "    MSH_INVALID    =   -1,\t//\t\n    MSH_DUMMY      =    0,\t//\t\n"
	                                      "    MSH_A_BAT_1    =    1,\t//\t\n};\n\nenum\tLHSoundAction\n{\n"
	                                      "\tSOUND_ACTION_BREATHE_IN\t\t\t= 1,\n\tSOUND_ACTION_FOOT_STAMP\t\t\t= 5,\n"
	                                      "\tSOUND_ACTION_BEEN_STOMPED,\n\tSOUND_ACTION_SWIM,\n};\n#endif\n";
	const auto old = reference::ParseEnumHeader(k_Header);
	const auto now = psys::ParseEnumHeader(k_Header);
	ASSERT_EQ(old.size(), 3u);
	for (const auto& [name, value] : old)
	{
		ASSERT_TRUE(now.contains(name)) << name;
		EXPECT_EQ(now.at(name), value) << name;
	}
	// the component reads every enum of the header, not only the first
	EXPECT_EQ(now.at("SOUND_ACTION_BEEN_STOMPED"), 6);
	EXPECT_EQ(now.at("SOUND_ACTION_SWIM"), 7);
}

TEST(ParticleFileParity, syntheticBitmaps)
{
	std::vector<uint8_t> bytes(5 * 3 * 3 * 3);
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		bytes[i] = static_cast<uint8_t>(i * 7 + 1);
	}
	// 5 frames in a grid of 2 to a row: the last frames run past the file's end and read as zeros
	for (int framesInUse = -1; framesInUse <= 6; ++framesInUse)
	{
		ExpectSameBitmap(bytes, 3, 3, 5, framesInUse);
	}
	ExpectSameBitmap(bytes, 5, 3, 9, 9);
	ExpectSameBitmap(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 45), 3, 1, 5, 5);
	ExpectSameBitmap(bytes, 0, 3, 5, 5);
	ExpectSameBitmap(bytes, 3, 0, 5, 5);
	ExpectSameBitmap(bytes, 3, 3, 0, 5);
	ExpectSameBitmap(bytes, 3, 3, 4, 4);
}

TEST(ParticleFileParity, particleTypeNames)
{
	// the table's names, which only the debug windows show; the files are pinned in test_magic_tables
	EXPECT_EQ(particles::ParticleTypeName(ParticleType::Leaves), "Leaves");
	EXPECT_EQ(particles::ParticleTypeName(ParticleType::Heal), "Heal");
	EXPECT_TRUE(particles::ParticleTypeName(static_cast<ParticleType>(particles::k_ParticleTypeCount)).empty());
	for (size_t i = 0; i < particles::k_ParticleTypeCount; ++i)
	{
		EXPECT_FALSE(particles::ParticleTypeName(static_cast<ParticleType>(i)).empty()) << i;
	}
}

/// With OPENBLACK_GAME_PATH set to the install: every Data\Spells\ZSpellFiles\*_txt.zzz parses to the same data
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(ParticleFileParity, realSpellFiles)
{
	const auto data = GameData();
	if (!data.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto directory = *data / "Spells" / "ZSpellFiles";
	ASSERT_TRUE(std::filesystem::is_directory(directory));
	size_t count = 0;
	for (const auto& entry : std::filesystem::directory_iterator(directory))
	{
		const auto name = entry.path().filename().string();
		const auto bytes = ReadFile(entry.path());
		std::string text;
		if (name.ends_with("_txt.zzz"))
		{
			const auto compressed = psys::SplitCompressed(bytes);
			ASSERT_TRUE(compressed.has_value()) << name;
			uint32_t size = 0;
			std::memcpy(&size, bytes.data(), sizeof(size));
			EXPECT_EQ(compressed->textSize, size) << name;
			const auto inflated = zip::Inflate(std::vector<uint8_t>(compressed->deflated.begin(), compressed->deflated.end()),
			                                   compressed->textSize);
			text.assign(inflated.begin(), inflated.end());
		}
		else if (name.ends_with(".txt"))
		{
			text.assign(bytes.begin(), bytes.end());
		}
		else
		{
			continue;
		}
		ASSERT_TRUE(psys::ParticleFile::Parse(text).has_value()) << name;
		ExpectSameParse(text, name);
		++count;
	}
	EXPECT_GT(count, 100u);
}

/// With OPENBLACK_GAME_PATH set to the install: the names and values of every Data\*.h enum header
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(ParticleFileParity, realEnumHeaders)
{
	const auto data = GameData();
	if (!data.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto* header : {"AllMeshes.h", "SoundAction.h", "SoundObject.h", "LHAction.h"})
	{
		const auto bytes = ReadFile(*data / header);
		ASSERT_FALSE(bytes.empty()) << header;
		const std::string text(bytes.begin(), bytes.end());
		const auto old = reference::ParseEnumHeader(text);
		const auto now = psys::ParseEnumHeader(text);
		ASSERT_FALSE(old.empty()) << header;
		std::map<int32_t, std::string> oldFirstByValue;
		for (const auto& [name, value] : old)
		{
			oldFirstByValue.try_emplace(value, name);
			ASSERT_TRUE(now.contains(name)) << header << " " << name;
			EXPECT_EQ(now.at(name), value) << header << " " << name;
		}
		if (std::string_view(header) == "SoundAction.h")
		{
			// the names read by value (the sound logs): one name to each value, so name order finds the same one
			EXPECT_EQ(old.size(), now.size());
			std::map<int32_t, std::string> nowFirstByValue;
			for (const auto& [name, value] : now)
			{
				nowFirstByValue.try_emplace(value, name);
			}
			EXPECT_EQ(oldFirstByValue, nowFirstByValue);
		}
	}
}

/// With OPENBLACK_GAME_PATH set to the install: every Data\Spells\LightMaps\*.raw in every layout its size allows
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(ParticleFileParity, realLightMaps)
{
	const auto data = GameData();
	if (!data.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto directory = *data / "Spells" / "LightMaps";
	ASSERT_TRUE(std::filesystem::is_directory(directory));
	size_t layouts = 0;
	for (const auto& entry : std::filesystem::directory_iterator(directory))
	{
		auto extension = entry.path().extension().string();
		std::ranges::transform(extension, extension.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });
		if (extension != ".raw")
		{
			continue;
		}
		const auto bytes = ReadFile(entry.path());
		for (const int channels : {1, 3})
		{
			for (int pitch = 1; pitch <= 64; ++pitch)
			{
				for (int frames = 1; frames <= 32; ++frames)
				{
					if (bytes.size() != static_cast<size_t>(pitch * pitch * channels * frames))
					{
						continue;
					}
					SCOPED_TRACE(entry.path().filename().string() + " pitch " + std::to_string(pitch));
					ExpectSameBitmap(bytes, pitch, channels, frames, frames);
					ExpectSameBitmap(bytes, pitch, channels, frames, frames - 1);
					++layouts;
				}
			}
		}
	}
	EXPECT_GT(layouts, 0u);
}
