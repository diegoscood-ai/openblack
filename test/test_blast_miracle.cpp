/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>
#include <cstring>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "Enums.h"
#include "InfoConstants.h"
#include "Magic/MagicTables.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;

namespace
{
/// The blast and its two power-ups
constexpr std::array k_BlastTypes {MagicType::ExplosionOne, MagicType::ExplosionOnePuOne, MagicType::ExplosionOnePuTwo};

/// The effect the hand shows while it holds a seed of that magic: the magic's in-hand particle type, and whether it
/// plays a particle file at all
void ExpectNoInHandEffect(const InfoConstants& info)
{
	for (const auto type : k_BlastTypes)
	{
		const auto inHand = magic::GetMagicInfo(info, type).particleTypeInHand;
		EXPECT_EQ(inHand, ParticleType::None) << static_cast<int>(type);
		EXPECT_TRUE(particles::ParticleTypeFile(inHand).empty()) << static_cast<int>(type);
	}
}
} // namespace

// The game's tables give none of the blast's three levels an in-hand effect (particle type 0), and the original's hand
// starts no effect for type 0, so the hand shows only the seed itself
TEST(BlastMiracle, realInfoDatTheHeldSeedHasNoInHandEffect)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	// the rows are the blast's: its own magic types, played with its three particle files
	EXPECT_EQ(magic::GetMagicInfo(*info, MagicType::ExplosionOne).magicType, MagicType::ExplosionOne);
	EXPECT_EQ(magic::GetMagicInfo(*info, MagicType::ExplosionOne).particleType, ParticleType::ExplosionOne);
	EXPECT_EQ(magic::GetMagicInfo(*info, MagicType::ExplosionOnePuOne).particleType, ParticleType::ExplosionOnePuOne);
	EXPECT_EQ(magic::GetMagicInfo(*info, MagicType::ExplosionOnePuTwo).particleType, ParticleType::ExplosionOnePuTwo);
	ExpectNoInHandEffect(*info);
}

// The same lookups on zeroed tables: particle type 0 names no file, so no in-hand effect is started
TEST(BlastMiracle, realInfoDatTheHeldSeedHasNoInHandEffectSynthetic)
{
	const auto info = std::make_unique<InfoConstants>();
	ExpectNoInHandEffect(*info);
	// a type that has a file names it
	EXPECT_FALSE(particles::ParticleTypeFile(ParticleType::ExplosionOne).empty());
}
