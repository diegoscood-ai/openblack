/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <EnumHeader.h>
#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/Engine/AnimEffectBank.h"
#include "Audio/Services/SpellSounds.h"
#include "Common/Zip.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SoundAction.h"

using namespace openblack;

namespace
{
int32_t ValueOf(const std::map<std::string, int32_t, std::less<>>& names, const std::string& name)
{
	const auto it = names.find(name);
	return it == names.end() ? -2 : it->second;
}

void AppendBlock(std::vector<uint8_t>& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.insert(out.end(), blockName.begin(), blockName.end());
	const auto size = static_cast<uint32_t>(data.size());
	const auto* sizeBytes = reinterpret_cast<const uint8_t*>(&size);
	out.insert(out.end(), sizeBytes, sizeBytes + sizeof(size));
	out.insert(out.end(), data.begin(), data.end());
}

template <typename T>
std::vector<uint8_t> Bytes(const std::vector<T>& values)
{
	std::vector<uint8_t> bytes(values.size() * sizeof(T));
	std::memcpy(bytes.data(), values.data(), bytes.size());
	return bytes;
}

/// A sample header of a sound bank: its number, override flags, loops, play mode and maximum distance; every sample
/// shares the bank's one 4-byte wave
pack::AudioBankSampleHeader SampleHeader(int32_t id, uint16_t overrides, int16_t loops, pack::AudioBankLoop mode,
                                         float maxDistance)
{
	pack::AudioBankSampleHeader header;
	std::memset(&header, 0, sizeof(header));
	header.id = id;
	header.size = 4;
	header.offset = 0;
	header.unknown10 = overrides;
	header.loop = loops;
	header.loopType = mode;
	header.maxDist = maxDistance;
	return header;
}

/// A .sad in memory with these samples, the anim effect rows (u32 rows, u32 width 6, then the rows) and their lists
std::vector<uint8_t> MakeSpellBank(const std::vector<pack::AudioBankSampleHeader>& headers,
                                   const std::vector<std::array<int32_t, 6>>& rows, const std::vector<int32_t>& lists)
{
	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	AppendBlock(file, "LHAudioWaveData", std::vector<uint8_t>(4, 0));
	std::vector<uint8_t> table(4, 0);
	const auto count = static_cast<uint16_t>(headers.size());
	std::memcpy(table.data(), &count, sizeof(count));
	const auto headerBytes = Bytes(headers);
	table.insert(table.end(), headerBytes.begin(), headerBytes.end());
	AppendBlock(file, "LHAudioBankSampleTable", table);
	std::vector<int32_t> array = {static_cast<int32_t>(rows.size()), 6};
	for (const auto& row : rows)
	{
		array.insert(array.end(), row.begin(), row.end());
	}
	AppendBlock(file, "LHAudioAnimArrayTable", Bytes(array));
	AppendBlock(file, "LHAudioWaveNumTable", Bytes(lists));
	return file;
}

/// A .zzz in memory: the text's length as a u32, then a zlib stream of one stored (uncompressed) block
std::vector<uint8_t> PackedSpellFile(std::string_view text)
{
	const auto size = static_cast<uint32_t>(text.size());
	std::vector<uint8_t> out = {static_cast<uint8_t>(size), static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size >> 16),
	                            static_cast<uint8_t>(size >> 24)};
	const auto length = static_cast<uint16_t>(text.size());
	const auto complement = static_cast<uint16_t>(~length);
	out.insert(out.end(), {0x78, 0x01, 0x01}); // the zlib header, then a final stored block
	out.push_back(static_cast<uint8_t>(length));
	out.push_back(static_cast<uint8_t>(length >> 8));
	out.push_back(static_cast<uint8_t>(complement));
	out.push_back(static_cast<uint8_t>(complement >> 8));
	out.insert(out.end(), text.begin(), text.end());
	uint32_t a = 1;
	uint32_t b = 0;
	for (const auto c : text)
	{
		a = (a + static_cast<uint8_t>(c)) % 65521;
		b = (b + a) % 65521;
	}
	const uint32_t adler = (b << 16) | a; // big-endian at the end
	for (int shift = 24; shift >= 0; shift -= 8)
	{
		out.push_back(static_cast<uint8_t>(adler >> shift));
	}
	return out;
}

} // namespace

TEST(SpellSounds, parseEnumHeader)
{
	// the layout of Data\SoundAction.h: tabs, names with a value, then bare names counting on from the one before
	const auto names = psys::ParseEnumHeader("// header\n#ifndef X\nenum\tLHSoundAction\n{\n\tA = 1, // one\n\tB\t= 2,\n"
	                                         "\tD = 112,\n\tE,\n\tF,\n};\n#endif\n");
	ASSERT_EQ(names.size(), 5u);
	EXPECT_EQ(ValueOf(names, "A"), 1);
	EXPECT_EQ(ValueOf(names, "B"), 2);
	EXPECT_EQ(ValueOf(names, "C"), -2);
	EXPECT_EQ(ValueOf(names, "D"), 112);
	EXPECT_EQ(ValueOf(names, "E"), 113);
	EXPECT_EQ(ValueOf(names, "F"), 114);
}

TEST(SpellSounds, readSoundActionFlags)
{
	const auto file = particles::File::Parse("BEGINPROPERTIES\nENDPROPERTIES\nBEGINCLASS CreateRuleAnAtom C0\nBEGINPROPERTIES\n"
	                                         "PROPERTY SoundOfCreate SOUND_ACTION NO_SOUND LOOPING 1 ONLYONE 1 SOFTRELEASE 1 "
	                                         "USESURFACE 1\nPROPERTY Other SOUND_ACTION NO_SOUND LOOPING 0 ONLYONE 1 "
	                                         "SOFTRELEASE 0 USESURFACE 1\nENDPROPERTIES\nENDCLASS\n",
	                                         "test");
	ASSERT_TRUE(file.has_value());
	const auto& object = file->objects.at(0);
	const auto a = particles::ReadSoundAction(object, "SoundOfCreate");
	EXPECT_EQ(a.action, -1);
	EXPECT_EQ(a.flags,
	          particles::SoundAction::k_Looping | particles::SoundAction::k_SoftRelease | particles::SoundAction::k_UseSurface);
	const auto b = particles::ReadSoundAction(object, "Other");
	EXPECT_EQ(b.flags, particles::SoundAction::k_UseSurface); // ONLYONE is not kept
	// a missing property: the SoundAction defaults
	const auto none = particles::ReadSoundAction(object, "Missing");
	EXPECT_EQ(none.action, -1);
	EXPECT_EQ(none.surface, 1);
	EXPECT_EQ(none.size, 2);
	EXPECT_EQ(none.alignment, 2);
	EXPECT_EQ(none.fadeStep, 0);
	EXPECT_EQ(none.flags, 0);
}

TEST(SpellSounds, sizeClasses)
{
	using namespace audio::spell_sounds;
	EXPECT_EQ(SizeFromRadius(199.0f, 200.0f, 500.0f), 3);
	EXPECT_EQ(SizeFromRadius(200.0f, 200.0f, 500.0f), 2);
	EXPECT_EQ(SizeFromRadius(499.0f, 200.0f, 500.0f), 2);
	EXPECT_EQ(SizeFromRadius(500.0f, 200.0f, 500.0f), 1);
	// the original compares with the doubles 0.6 / 0.3: 0.6f = 0.60000002 and 0.3f = 0.30000001 are above them, the
	// floats just below are not
	EXPECT_EQ(SizeFromThrow(0.61f), 1);
	EXPECT_EQ(SizeFromThrow(0.6f), 1);
	EXPECT_EQ(SizeFromThrow(std::nextafter(0.6f, 0.0f)), 2);
	EXPECT_EQ(SizeFromThrow(0.31f), 2);
	EXPECT_EQ(SizeFromThrow(0.3f), 2);
	EXPECT_EQ(SizeFromThrow(std::nextafter(0.3f, 0.0f)), 3);
	EXPECT_EQ(SizeFromThrow(0.0f), 3);
	EXPECT_EQ(SizeFromImpactSpeed(9.0f, 10.0f, 20.0f), 3);
	EXPECT_EQ(SizeFromImpactSpeed(10.0f, 10.0f, 20.0f), 2);
	EXPECT_EQ(SizeFromImpactSpeed(20.0f, 10.0f, 20.0f), 1);
}

TEST(SpellSounds, aDelayedSoundWaitsItsDistanceAtTheSpeedOfSound)
{
	EXPECT_FLOAT_EQ(audio::spell_sounds::DelayFor(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(audio::spell_sounds::DelayFor(347.0f), 1.0f);
	EXPECT_FLOAT_EQ(audio::spell_sounds::DelayFor(694.0f), 2.0f);
}

TEST(SpellSounds, alignmentAttributeOfTheEffectsPlayer)
{
	using audio::spell_sounds::AlignmentAttribute;
	// The original's table over the discrete alignment 0..6: 1, 1, 2, 2, 2, 3, 3; out of it the action keeps its own
	EXPECT_EQ(AlignmentAttribute(0, 2), 1);
	EXPECT_EQ(AlignmentAttribute(1, 2), 1);
	EXPECT_EQ(AlignmentAttribute(2, 9), 2);
	EXPECT_EQ(AlignmentAttribute(3, 9), 2);
	EXPECT_EQ(AlignmentAttribute(4, 9), 2);
	EXPECT_EQ(AlignmentAttribute(5, 2), 3);
	EXPECT_EQ(AlignmentAttribute(6, 2), 3);
	EXPECT_EQ(AlignmentAttribute(7, 2), 2);
	EXPECT_EQ(AlignmentAttribute(-1, 9), 9);
	EXPECT_EQ(AlignmentAttribute(std::numeric_limits<int32_t>::min(), 2), 2);
}

TEST(SpellSounds, findAttribRow)
{
	constexpr int32_t w = audio::AnimEffectBank::k_Wildcard;
	audio::AnimEffectBank bank;
	bank.rows = {{w, w, w, w, 52, 0}, {1, w, w, w, 52, 2}, {1, w, w, 7, 52, 5}, {w, w, w, 7, 52, 7}};
	bank.waves = {1, 10, 2, 11, 12, 1, 13, 1, 14};
	EXPECT_EQ(bank.FindList({3, 2, 1, 1, 52}), std::vector<int32_t>({10}));
	EXPECT_EQ(bank.FindList({1, 2, 1, 1, 52}), std::vector<int32_t>({11, 12}));
	EXPECT_EQ(bank.FindList({1, 2, 1, 7, 52}), std::vector<int32_t>({13})); // the most exact row
	EXPECT_EQ(bank.FindList({2, 2, 1, 7, 52}), std::vector<int32_t>({14}));
	EXPECT_TRUE(bank.FindList({1, 2, 1, 1, 40}).empty());
}

/// With OPENBLACK_GAME_PATH set to the install: Data\SoundAction.h, spells.sad and SF_TeleportVortex
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(SpellSounds, realData)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const std::filesystem::path root(game);
	std::ifstream header(root / "Data" / "SoundAction.h", std::ios::binary);
	ASSERT_TRUE(header.is_open());
	const std::string text((std::istreambuf_iterator<char>(header)), std::istreambuf_iterator<char>());
	const auto names = psys::ParseEnumHeader(text);
	EXPECT_EQ(ValueOf(names, "SOUND_SPELL_TELEPORT_POOL"), 76);
	EXPECT_EQ(ValueOf(names, "SOUND_SPELL_HAND_LIGHTNING_LEVEL_3"), 113);
	EXPECT_EQ(ValueOf(names, "SOUND_SPELL_CREATURE_SPELL_CAST"), 117);
	EXPECT_EQ(ValueOf(names, "SOUND_SPELL_WATER"), 141);
	EXPECT_EQ(ValueOf(names, "SOUND_ACTION_VORTEX"), 156);

	pack::PackFile pack;
	ASSERT_EQ(pack.Open(root / "Audio" / "Sfx" / "Game" / "spells.sad"), pack::PackResult::Success);
	audio::AnimEffectBank bank;
	bank.name = "spells.sad";
	bank.Load(pack);
	EXPECT_EQ(bank.rows.size(), 75u);
	// the default slots {size 2, alignment 2, 1, surface 1}: the "*" rows
	EXPECT_EQ(bank.FindList({2, 2, 1, 1, 76}), std::vector<int32_t>({39}));
	EXPECT_EQ(bank.FindList({2, 2, 1, 1, 48}), std::vector<int32_t>({1}));
	EXPECT_EQ(bank.FindList({1, 2, 1, 3, 52}), std::vector<int32_t>({28, 29}));
	EXPECT_EQ(bank.FindList({3, 2, 1, 7, 52}), std::vector<int32_t>({23}));
	EXPECT_EQ(bank.FindList({3, 2, 1, 1, 43}), std::vector<int32_t>({16}));
	EXPECT_TRUE(bank.FindList({2, 2, 1, 1, 57}).empty()); // CAST_FAILURE has no row
	const auto* pool = bank.FindSample(39);
	ASSERT_NE(pool, nullptr);
	EXPECT_EQ(pool->loops, -1);
	EXPECT_EQ(pool->playMode, 2);
	EXPECT_FLOAT_EQ(pool->maxDistance, 170.0f);

	std::ifstream zzz(root / "Data" / "Spells" / "ZSpellFiles" / "SF_TeleportVortex_txt.zzz", std::ios::binary);
	ASSERT_TRUE(zzz.is_open());
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(zzz)), std::istreambuf_iterator<char>());
	ASSERT_GT(bytes.size(), 4u);
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), 4);
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	const auto file = particles::File::Parse(std::string(inflated.begin(), inflated.end()), "SF_TeleportVortex");
	ASSERT_TRUE(file.has_value());
	const auto* create = file->Find("CreateRuleAnAtom0");
	ASSERT_NE(create, nullptr);
	EXPECT_EQ(create->String("SoundOfCreate"), "SOUND_SPELL_TELEPORT_POOL");
	EXPECT_EQ(particles::ReadSoundAction(*create, "SoundOfCreate").flags,
	          particles::SoundAction::k_Looping | particles::SoundAction::k_SoftRelease);
}

TEST(SpellSounds, realDataSynthetic)
{
	// The same path as above on files built here: a sound action header, a spells bank and a packed spell file
	const auto names = psys::ParseEnumHeader("enum\tLHSoundAction\n{\n\tNO_SOUND = 0,\n\tSOUND_TEST_CAST = 48,\n"
	                                         "\tSOUND_TEST_POOL = 76,\n\tSOUND_TEST_BOLT,\n};\n");
	const int32_t cast = ValueOf(names, "SOUND_TEST_CAST");
	const int32_t pool = ValueOf(names, "SOUND_TEST_POOL");
	const int32_t bolt = ValueOf(names, "SOUND_TEST_BOLT");
	ASSERT_EQ(cast, 48);
	ASSERT_EQ(pool, 76);
	ASSERT_EQ(bolt, 77);

	// sample 9 has both override bits (loops, play mode); sample 4 has those fields without their bits, so the defaults
	// stay; the maximum distance is always the record's
	const std::vector<pack::AudioBankSampleHeader> headers = {
	    SampleHeader(4, 0, 5, pack::AudioBankLoop::Once, 30.0f),
	    SampleHeader(9, 0x40 | 0x400, -1, pack::AudioBankLoop::Once, 55.5f),
	    SampleHeader(6, 0, 0, pack::AudioBankLoop::None, 0.0f),
	    SampleHeader(7, 0, 0, pack::AudioBankLoop::None, 0.0f),
	    SampleHeader(8, 0, 0, pack::AudioBankLoop::None, 0.0f),
	};
	// the lists: {9} at 0, {4} at 2, {6, 7} at 4, {8} at 7
	const std::vector<int32_t> lists = {1, 9, 1, 4, 2, 6, 7, 1, 8};
	constexpr int32_t w = audio::AnimEffectBank::k_Wildcard;
	const std::vector<std::array<int32_t, 6>> rows = {
	    {w, w, w, w, pool, 0}, {w, w, w, w, cast, 2}, {1, w, w, 3, bolt, 4}, {3, w, w, 7, bolt, 7}};
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(MakeSpellBank(headers, rows, lists)), pack::PackResult::Success);
	audio::AnimEffectBank bank;
	bank.name = "spells.sad";
	bank.Load(pack);
	EXPECT_EQ(bank.rows.size(), rows.size());
	// the default slots {size 2, alignment 2, 1, surface 1}: the "*" rows
	EXPECT_EQ(bank.FindList({2, 2, 1, 1, pool}), std::vector<int32_t>({9}));
	EXPECT_EQ(bank.FindList({2, 2, 1, 1, cast}), std::vector<int32_t>({4}));
	EXPECT_EQ(bank.FindList({1, 2, 1, 3, bolt}), std::vector<int32_t>({6, 7}));
	EXPECT_EQ(bank.FindList({3, 2, 1, 7, bolt}), std::vector<int32_t>({8}));
	EXPECT_TRUE(bank.FindList({2, 2, 1, 1, 57}).empty()); // an action without a row
	const auto* looping = bank.FindSample(9);
	ASSERT_NE(looping, nullptr);
	EXPECT_EQ(looping->loops, -1);
	EXPECT_EQ(looping->playMode, 2);
	EXPECT_FLOAT_EQ(looping->maxDistance, 55.5f);
	const auto* plain = bank.FindSample(4);
	ASSERT_NE(plain, nullptr);
	EXPECT_EQ(plain->loops, 0);
	EXPECT_EQ(plain->playMode, 3);
	EXPECT_FLOAT_EQ(plain->maxDistance, 30.0f);
	EXPECT_EQ(bank.FindSample(5), nullptr);

	const auto bytes = PackedSpellFile("BEGINPROPERTIES\nENDPROPERTIES\nBEGINCLASS CreateRuleAnAtom CreateRuleAnAtom0\n"
	                                   "BEGINPROPERTIES\nPROPERTY SoundOfCreate SOUND_ACTION SOUND_TEST_POOL LOOPING 1 "
	                                   "ONLYONE 0 SOFTRELEASE 1 USESURFACE 0\nENDPROPERTIES\nENDCLASS\n");
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), 4);
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	const auto file = particles::File::Parse(std::string(inflated.begin(), inflated.end()), "SF_Test");
	ASSERT_TRUE(file.has_value());
	const auto* create = file->Find("CreateRuleAnAtom0");
	ASSERT_NE(create, nullptr);
	EXPECT_EQ(create->String("SoundOfCreate"), "SOUND_TEST_POOL");
	// the switches its flags are made of; readSoundActionFlags pins the flags, and here a sound name other than
	// NO_SOUND would make ReadSoundAction look the name up in the game's own header
	const auto sound = create->Sound("SoundOfCreate");
	EXPECT_TRUE(sound.looping);
	EXPECT_TRUE(sound.softRelease);
	EXPECT_FALSE(sound.useSurface);
}
