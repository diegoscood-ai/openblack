/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flock miracles (M4c): SpellFlock's spawn interpolation, fn_00723570's fan and destination, the counts, the
// SpellWolf corridor (fn_00420F50 / IsPosOnCorridor 0x420E10), the SpellDove / SpellWolf fade (Magic/Spells/SpellFlock)
// and UR_Flocking's formulas (PSys/Rules/Flock).

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "InfoConstants.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellFlock.h"
#include "PSys/Rules/Flock.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
constexpr int32_t Cell(int cell, int low = 0)
{
	return static_cast<int32_t>((static_cast<uint32_t>(cell) << 16) | static_cast<uint32_t>(low));
}
bool InMap(glm::ivec2 p)
{
	return (static_cast<uint32_t>(p.x) >> 16) < 512 && (static_cast<uint32_t>(p.y) >> 16) < 512;
}
} // namespace

/// GetNumberToCreate 0x723B80 / 0x724250: fistp rounds to nearest (even)
TEST(SpellFlock, numberToCreate)
{
	EXPECT_EQ(spell_flock::NumberToCreate(12, 1.0f), 12);
	EXPECT_EQ(spell_flock::NumberToCreate(14, 1.0f), 14);
	EXPECT_EQ(spell_flock::NumberToCreate(12, 1.25f), 15);
	EXPECT_EQ(spell_flock::NumberToCreate(14, 1.25f), 18); // 17.5 -> 18
	EXPECT_EQ(spell_flock::NumberToCreate(12, 0.625f), 8); // 7.5 -> 8
	EXPECT_EQ(spell_flock::NumberToCreate(12, 0.375f), 4); // 4.5 -> 4
}

/// GetParticleType 0x723A30 and the bird of 0x723CFE: evil only strictly below alignmentSwitch
TEST(SpellFlock, goodOrEvil)
{
	EXPECT_TRUE(spell_flock::IsEvil(-0.21f, -0.2f));
	EXPECT_FALSE(spell_flock::IsEvil(-0.2f, -0.2f));
	EXPECT_FALSE(spell_flock::IsEvil(0.0f, -0.2f));
	EXPECT_EQ(spell_flock::FlyingAnimal(-1.0f, -0.2f), AnimalInfo::SpellBat);
	EXPECT_EQ(spell_flock::FlyingAnimal(0.5f, -0.2f), AnimalInfo::SpellDove);
}

/// fn_00723570: the direction, the side and the fan of up to +-2 rad
TEST(SpellFlock, fan)
{
	// the AI (a script cast from 30 m above the target): castPos - handPos has no XZ part -> (1, 0)
	const auto d = spell_flock::Direction(false, glm::vec3(0.0f), glm::vec3(100.0f, 0.0f, 200.0f), glm::vec3(100.0f, 30.0f, 200.0f));
	EXPECT_FLOAT_EQ(d.x, 1.0f);
	EXPECT_FLOAT_EQ(d.y, 0.0f);
	const auto human = spell_flock::Direction(true, glm::vec3(0.0f, -1.0f, 3.0f), glm::vec3(0.0f), glm::vec3(0.0f));
	EXPECT_FLOAT_EQ(human.x, 0.0f);
	EXPECT_FLOAT_EQ(human.y, 3.0f);
	// the AI alternates: even +1, odd -1
	EXPECT_FLOAT_EQ(spell_flock::Side(false, d, glm::vec2(0.0f), glm::vec2(0.0f), 1), -1.0f);
	EXPECT_FLOAT_EQ(spell_flock::Side(false, d, glm::vec2(0.0f), glm::vec2(0.0f), 2), 1.0f);
	// a human: the side of the spawn point relative to the cast point and the direction (v.z d.x - v.x d.z)
	EXPECT_FLOAT_EQ(spell_flock::Side(true, glm::vec2(1.0f, 0.0f), glm::vec2(10.0f, 15.0f), glm::vec2(10.0f, 10.0f), 1), 1.0f);
	EXPECT_FLOAT_EQ(spell_flock::Side(true, glm::vec2(1.0f, 0.0f), glm::vec2(10.0f, 5.0f), glm::vec2(10.0f, 10.0f), 2), -1.0f);
	// straight ahead (|cross| <= 0.1): parity
	EXPECT_FLOAT_EQ(spell_flock::Side(true, glm::vec2(1.0f, 0.0f), glm::vec2(20.0f, 10.0f), glm::vec2(10.0f, 10.0f), 3), -1.0f);
	// the angle: 2 x created x side / N, so the 12th of 12 is at +-2 rad
	EXPECT_FLOAT_EQ(spell_flock::Angle(12, 1.0f, 12), 2.0f);
	EXPECT_FLOAT_EQ(spell_flock::Angle(3, -1.0f, 12), -0.5f);
	EXPECT_FLOAT_EQ(spell_flock::Angle(1, 1.0f, 0), 0.0f);
	const auto r = spell_flock::Rotate(glm::vec2(1.0f, 0.0f), 2.0f);
	EXPECT_NEAR(r.x, std::cos(2.0f), 1e-6f);
	EXPECT_NEAR(r.y, std::sin(2.0f), 1e-6f);
	const auto q = spell_flock::Rotate(glm::vec2(0.0f, 1.0f), 0.5f);
	EXPECT_NEAR(q.x, -std::sin(0.5f), 1e-6f);
	EXPECT_NEAR(q.y, std::cos(0.5f), 1e-6f);
}

/// fn_00723570's destination: the 10 m cell moves by d (ftol of (cell x 10 + d) / 10), the sub-cell part stays
TEST(SpellFlock, destination)
{
	const glm::ivec2 spawn(Cell(100, 0x8000), Cell(200, 0x1234));
	const auto t = spell_flock::DestinationAt(spawn, glm::vec2(1.0f, 0.0f), 800.0f);
	EXPECT_EQ(t.x, Cell(180, 0x8000));
	EXPECT_EQ(t.y, Cell(200, 0x1234));
	// any length of d: it is set to the distance
	const auto u = spell_flock::DestinationAt(spawn, glm::vec2(0.0f, -3.0f), 800.0f);
	EXPECT_EQ(u.x, Cell(100, 0x8000));
	EXPECT_EQ(u.y, Cell(120, 0x1234));
	// truncation towards 0: (50 - 55) / 10 = -0.5 -> cell 0
	const auto v = spell_flock::DestinationAt(glm::ivec2(Cell(5), Cell(5)), glm::vec2(-1.0f, 0.0f), 55.0f);
	EXPECT_EQ(v.x, Cell(0));
	// off the map: halved until it fits (800, 400, 200, 100 miss; 50 lands on cell 0)
	glm::ivec2 out;
	EXPECT_TRUE(spell_flock::Destination(glm::ivec2(Cell(5), Cell(5)), glm::vec2(-1.0f, 0.0f), 800.0f, InMap, out));
	EXPECT_EQ(out.x, Cell(0));
	EXPECT_EQ(out.y, Cell(5));
	// the last try is at 12.5 m (the next, 6.25, is not over 10): from cell 0 going -x that still misses
	EXPECT_FALSE(spell_flock::Destination(glm::ivec2(Cell(0), Cell(5)), glm::vec2(-1.0f, 0.0f), 800.0f, InMap, out));
	EXPECT_EQ(out.x, Cell(0xFFFF)); // ftol(-1.25) = -1 as the word
}

/// The spawn loop's interpolation along the hand's path (fistp: to nearest)
TEST(SpellFlock, spawnPoint)
{
	const glm::ivec2 from(1000, 2000);
	const glm::ivec2 to(1010, 1990);
	EXPECT_EQ(spell_flock::SpawnPoint(from, to, 0.0f), from);
	EXPECT_EQ(spell_flock::SpawnPoint(from, to, 1.0f), to);
	EXPECT_EQ(spell_flock::SpawnPoint(from, to, 0.25f), glm::ivec2(1002, 1998)); // 2.5 -> 2, -2.5 -> -2
	EXPECT_EQ(spell_flock::SpawnPoint(from, to, 0.35f), glm::ivec2(1004, 1996)); // 3.5 -> 4
	// MapCoords: ftol(x x 6553.6) and back x 10 / 65536
	const auto m = spell_flock::ToMapCoords(glm::vec2(1005.0f, 2.5f));
	EXPECT_EQ(m.x, Cell(100, 0x8000));
	EXPECT_EQ(m.y, 16384);
	const auto back = spell_flock::ToMetres(m);
	EXPECT_FLOAT_EQ(back.x, 1005.0f);
	EXPECT_FLOAT_EQ(back.y, 2.5f);
}

/// fn_00420F50 and SpellWolf::IsPosOnCorridor 0x420E10
TEST(SpellFlock, wolfCorridor)
{
	SpellFlockAnimal wolf;
	spell_flock::SetupCorridor(wolf, glm::vec2(0.0f, 0.0f), glm::vec2(100.0f, 0.0f), 45.0f);
	EXPECT_FLOAT_EQ(wolf.normal.x, 0.0f);
	EXPECT_FLOAT_EQ(wolf.normal.y, -1.0f);
	EXPECT_FLOAT_EQ(wolf.offset, 0.0f);
	EXPECT_FLOAT_EQ(wolf.halfWidth, 45.0f);
	const glm::vec2 at(20.0f, 0.0f);
	EXPECT_TRUE(spell_flock::IsPosOnCorridor(wolf, at, glm::vec2(50.0f, 30.0f)));
	EXPECT_TRUE(spell_flock::IsPosOnCorridor(wolf, at, glm::vec2(500.0f, -45.0f))); // on the edge
	EXPECT_FALSE(spell_flock::IsPosOnCorridor(wolf, at, glm::vec2(50.0f, 50.0f)));
	// behind the wolf's cell corner (20, 0) by more than the half width
	EXPECT_TRUE(spell_flock::IsPosOnCorridor(wolf, at, glm::vec2(-20.0f, 0.0f)));
	EXPECT_FALSE(spell_flock::IsPosOnCorridor(wolf, at, glm::vec2(-30.0f, 0.0f)));
	// the corner, not the wolf: at (29, 0) it is still cell 2
	EXPECT_TRUE(spell_flock::IsPosOnCorridor(wolf, glm::vec2(29.0f, 0.0f), glm::vec2(-24.0f, 0.0f)));
	// an oblique one: start (10, 10) -> (40, 50), normal (40, -30) / 50
	SpellFlockAnimal oblique;
	spell_flock::SetupCorridor(oblique, glm::vec2(10.0f, 10.0f), glm::vec2(40.0f, 50.0f), 5.0f);
	EXPECT_FLOAT_EQ(oblique.normal.x, 0.8f);
	EXPECT_FLOAT_EQ(oblique.normal.y, -0.6f);
	EXPECT_FLOAT_EQ(oblique.offset, -(10.0f * 0.8f - 10.0f * 0.6f));
	// start == destination: the normal (1, 0)
	SpellFlockAnimal still;
	spell_flock::SetupCorridor(still, glm::vec2(5.0f, 5.0f), glm::vec2(5.0f, 5.0f), 45.0f);
	EXPECT_FLOAT_EQ(still.normal.x, 1.0f);
	EXPECT_FLOAT_EQ(still.normal.y, 0.0f);
}

/// SpellDove / SpellWolf::SetDying: the alpha 255 -> 0 over 20 turns (2 s), then ToBeDeleted
TEST(SpellFlock, fade)
{
	SpellFlockAnimal dove;
	dove.fade.SetPosition(spell_flock::k_FullAlpha);
	EXPECT_FALSE(spell_flock::ProcessFade(dove)); // not fading: stays 255
	EXPECT_FLOAT_EQ(dove.fade.value, 255.0f);
	spell_flock::StartFade(dove);
	float previous = dove.fade.value;
	int turns = 0;
	bool gone = false;
	while (!gone && turns < 30)
	{
		gone = spell_flock::ProcessFade(dove);
		++turns;
		EXPECT_LE(dove.fade.value, previous + 1e-3f);
		previous = dove.fade.value;
		if (turns == 10)
		{
			EXPECT_NEAR(dove.fade.value, 79.6875f, 0.5f); // the quartic (no acceleration at the end): 5/16 at half time
		}
	}
	EXPECT_TRUE(gone);
	EXPECT_GE(turns, 20);
	EXPECT_LE(turns, 21);
	// a second SetDying does not restart it
	SpellFlockAnimal wolf;
	wolf.fade.SetPosition(spell_flock::k_FullAlpha);
	spell_flock::StartFade(wolf);
	(void)spell_flock::ProcessFade(wolf);
	const float after = wolf.fade.value;
	spell_flock::StartFade(wolf);
	EXPECT_FLOAT_EQ(wolf.fade.value, after);
	EXPECT_FLOAT_EQ(wolf.fade.duration, 2.0f);
}

/// With OPENBLACK_GAME_PATH set to the install: the FLYING_FLOCK / GROUND_FLOCK rows (resources.md §0.2)
TEST(SpellFlock, realInfoDat)
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
	const auto* flying = magic::GetMagicInfoAs<GMagicFlockFlyingInfo>(*info, MagicType::FlockFlying);
	ASSERT_NE(flying, nullptr);
	EXPECT_EQ(flying->numberToCreate, 12u);
	EXPECT_FLOAT_EQ(flying->alignmentSwitch, -0.2f);
	EXPECT_FLOAT_EQ(flying->distanceToTravel, 800.0f);
	const auto* ground = magic::GetMagicInfoAs<GMagicFlockGroundInfo>(*info, MagicType::FlockGround);
	ASSERT_NE(ground, nullptr);
	EXPECT_EQ(ground->numberToCreate, 14u);
	EXPECT_FLOAT_EQ(ground->alignmentSwitch, -0.2f);
	EXPECT_FLOAT_EQ(ground->distanceToTravel, 800.0f);
	EXPECT_FLOAT_EQ(ground->huntingRadius, 45.0f);
	const auto& flyingEffect = magic::GetMagicEffectInfo(*info, MagicType::FlockFlying);
	EXPECT_FLOAT_EQ(flyingEffect.costPerGameTurn, 40.0f);
	EXPECT_FLOAT_EQ(flyingEffect.costPerShieldCollide, 50.0f);
	EXPECT_FLOAT_EQ(flyingEffect.timerWhenPlayerCasting, 25.0f);
	EXPECT_EQ(flyingEffect.reactionType, Reaction::ReactToImpressiveSpell);
	const auto& groundEffect = magic::GetMagicEffectInfo(*info, MagicType::FlockGround);
	EXPECT_FLOAT_EQ(groundEffect.costPerGameTurn, 35.0f);
	EXPECT_FLOAT_EQ(groundEffect.timerWhenPlayerCasting, 60.0f);
	// the birds fly to T at the GAnimalInfo altitudeNormal (+0x278) of SpellDove / SpellBat
	EXPECT_FLOAT_EQ(info->animal[static_cast<size_t>(AnimalInfo::SpellDove)].altitudeNormal, 45.0f);
	EXPECT_FLOAT_EQ(info->animal[static_cast<size_t>(AnimalInfo::SpellBat)].altitudeNormal, 45.0f);
}

/// UR_Flocking's fn_00683520: the distance law
TEST(UrFlocking, accn)
{
	EXPECT_FLOAT_EQ(psys::flocking::Accn(5.0f, 0, false, 0.4f), 1.0f);
	EXPECT_FLOAT_EQ(psys::flocking::Accn(5.0f, 1, false, 0.4f), 0.5f);  // 1 / (5 x 0.4)
	EXPECT_FLOAT_EQ(psys::flocking::Accn(5.0f, 2, false, 0.4f), 0.25f); // 1 / (5 x 0.4)^2
	EXPECT_FLOAT_EQ(psys::flocking::Accn(5.0f, 1, true, 0.4f), 2.0f);
	EXPECT_FLOAT_EQ(psys::flocking::Accn(5.0f, 2, true, 0.4f), 4.0f);
	// at least 0.01 m
	EXPECT_FLOAT_EQ(psys::flocking::Accn(0.0f, 1, true, 1.0f), 0.01f);
	EXPECT_FLOAT_EQ(psys::flocking::Accn(-3.0f, 1, false, 1.0f), 100.0f);
}

/// UR_Flocking::UpdateBanking 0x684160: local +Z along the velocity, the pitch reduced, the bank from the turn
TEST(UrFlocking, banking)
{
	// straight along +x: no pitch, no bank; local z -> +x, local y up
	auto m = psys::flocking::BankingRotation(glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f), 1.0f, 10.0f);
	EXPECT_NEAR(m[2].x, 1.0f, 1e-5f);
	EXPECT_NEAR(m[2].y, 0.0f, 1e-5f);
	EXPECT_NEAR(m[2].z, 0.0f, 1e-5f);
	EXPECT_NEAR(m[1].y, 1.0f, 1e-5f);
	// along +z: the identity
	m = psys::flocking::BankingRotation(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f), 1.0f, 10.0f);
	EXPECT_NEAR(m[0].x, 1.0f, 1e-5f);
	EXPECT_NEAR(m[1].y, 1.0f, 1e-5f);
	EXPECT_NEAR(m[2].z, 1.0f, 1e-5f);
	// climbing at 45 degrees with ReducePitchBy 1: local z along the velocity; with 0.5 only 22.5 degrees
	m = psys::flocking::BankingRotation(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f), 1.0f, 10.0f);
	EXPECT_NEAR(m[2].x, std::sqrt(0.5f), 1e-5f);
	EXPECT_NEAR(m[2].y, std::sqrt(0.5f), 1e-5f);
	m = psys::flocking::BankingRotation(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f), 0.5f, 10.0f);
	EXPECT_NEAR(m[2].y, std::sin(0.125f * 3.14159265f), 1e-5f);
	// turning (a.z v.x - a.x v.z = 1 at |v.xz| = 1, gravity 1): bank 45 degrees, local y = (0, cos, -sin)
	m = psys::flocking::BankingRotation(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 1.0f, 1.0f);
	EXPECT_NEAR(m[1].x, 0.0f, 1e-5f);
	EXPECT_NEAR(m[1].y, std::sqrt(0.5f), 1e-5f);
	EXPECT_NEAR(m[1].z, -std::sqrt(0.5f), 1e-5f);
	// no horizontal speed: no bank
	m = psys::flocking::BankingRotation(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(5.0f, 0.0f, 5.0f), 1.0f, 1.0f);
	EXPECT_NEAR(glm::length(m[1]), 1.0f, 1e-5f);
}
