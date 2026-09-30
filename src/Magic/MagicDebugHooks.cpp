/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Environment-variable test hooks of the miracles (documented in docs/bw1-notes/openblack-internals.md):
//   OPENBLACK_TEST_SPELL="<MAGIC>,x,z[,radius[,duration[,player[,curl]]]]"  a cast as the script's SPELL_AT_POS
//   OPENBLACK_TEST_SEED="<SEED>[,pu]"                                 a charged seed into the hand (one-shot path)
//   OPENBLACK_TEST_ONESHOT="<SEED>,x,z[,pu[,tap]]"                    a one-shot orb on the land (tap: into the hand)
// <MAGIC> is a MAGIC_TYPE number or the effect's info.dat name (FIRE, HEAL, STORM_PU2...); <SEED> a SPELL_SEED_TYPE
// number or the seed's name (FIRE, HEAL, STORM...). Each runs once, the first turn the land exists (or the game turn
// OPENBLACK_TEST_MAGIC_TURN=<n>).

#include "MagicLoop.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "CastRules.h"
#include "Core/OneOffSpellSeed.h"
#include "Core/Spell.h"
#include "Core/SpellCreator.h"
#include "HealDebugHooks.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MagicTables.h"
#include "Script/CHLSpells.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
bool g_Done = false;

bool IsNumber(const char* text)
{
	char* end = nullptr;
	std::strtol(text, &end, 10);
	return end != text && *end == '\0';
}

int MagicFromText(const char* text)
{
	if (IsNumber(text))
	{
		return std::atoi(text);
	}
	return GetInfoFromText(Locator::infoConstants::value(), text);
}

int SeedFromText(const char* text)
{
	if (IsNumber(text))
	{
		return std::atoi(text);
	}
	return GetSpellSeedFromText(Locator::infoConstants::value(), text);
}

float Ground(float x, float z)
{
	return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
}

void TestSpell(const char* value)
{
	char name[64] = {};
	float x = 0.0f;
	float z = 0.0f;
	float radius = 10.0f;
	float duration = -2.0f;
	int player = -1;
	float curl = 0.0f; // SPELL_AT_POS's curl (PSysProcessInfo +0x34: the shields' spin)
	if (std::sscanf(value, "%63[^,],%f,%f,%f,%f,%d,%f", name, &x, &z, &radius, &duration, &player, &curl) < 3)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Magic test: OPENBLACK_TEST_SPELL=\"{}\" not understood", value);
		return;
	}
	const int magic = MagicFromText(name);
	if (magic <= 0 || magic >= static_cast<int>(k_MagicTypeCount))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Magic test: no magic type {}", name);
		return;
	}
	const auto type = static_cast<MagicType>(magic);
	// no duration given: the player's timer (timerWhenPlayerCasting), as a hand cast would last
	if (duration == -2.0f)
	{
		duration = GetTimerWhenPlayerCasting(Locator::infoConstants::value(), type);
	}
	// the neutral player refills its spells (the script's creator); a player number casts as that player
	ecs::components::SpellCreator creator;
	if (player >= 0 && player < static_cast<int>(PlayerNames::_COUNT))
	{
		creator = creator::OfPlayer(static_cast<PlayerNames>(player));
	}
	const glm::vec3 target(x, Ground(x, z), z);
	// SPELL_AT_POS's "from": 30 m above the target (inf: the challenge scripts cast from the sky)
	const glm::vec3 from = target + glm::vec3(0.0f, 30.0f, 0.0f);
	const auto spell = script::CastSpellAtPos(target, type, from, creator, false, radius, duration, curl, glm::vec3(0.0f));
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Magic test: {} ({}) at ({:.1f}, {:.1f}) radius {:.1f} duration {:.1f} player {} -> spell {}", name,
	                   magic, x, z, radius, duration, player, spell == entt::null ? -1 : static_cast<int>(spell));
	// what the hand's CanCast would answer there (the script path does not ask)
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic test: CanCastAt({}) there = {} (class check, vt 0x30)", name,
	                   cast_rules::CanCastAt(type, ToMap(target)));
}
} // namespace

void magic::ResetDebugHooks()
{
	g_Done = false;
	heal_debug::ResetDebugHooks();
}

void magic::RunDebugHooks()
{
	heal_debug::RunDebugHooks(); // OPENBLACK_TEST_HURT_VILLAGERS (HealDebugHooks.cpp), before a heal cast below
	if (g_Done || !Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	// OPENBLACK_TEST_MAGIC_TURN=<n>: the three hooks below wait for game turn n (screenshots of what happens later)
	if (const char* wait = std::getenv("OPENBLACK_TEST_MAGIC_TURN");
	    wait != nullptr && CurrentTurn() < static_cast<unsigned int>(std::max(0, std::atoi(wait))))
	{
		return;
	}
	g_Done = true;
	if (const char* value = std::getenv("OPENBLACK_TEST_SPELL"); value != nullptr)
	{
		TestSpell(value);
	}
	if (const char* value = std::getenv("OPENBLACK_TEST_SEED"); value != nullptr)
	{
		char name[64] = {};
		int powerUp = -1;
		if (std::sscanf(value, "%63[^,],%d", name, &powerUp) >= 1)
		{
			const int seedType = SeedFromText(name);
			const auto seed = one_off::CreateSpellIntoHand(PlayerNames::PLAYER_ONE, static_cast<SpellSeedType>(seedType),
			                                               powerUp, 1.0f);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic test: seed {} ({}) pu {} into the hand -> {}", name, seedType,
			                   powerUp, seed == entt::null ? -1 : static_cast<int>(seed));
		}
	}
	if (const char* value = std::getenv("OPENBLACK_TEST_ONESHOT"); value != nullptr)
	{
		char name[64] = {};
		char tap[16] = {};
		float x = 0.0f;
		float z = 0.0f;
		int powerUp = -1;
		if (std::sscanf(value, "%63[^,],%f,%f,%d,%15s", name, &x, &z, &powerUp, tap) >= 3)
		{
			const int seedType = SeedFromText(name);
			const auto orb = one_off::Create(glm::vec3(x, Ground(x, z), z), static_cast<SpellSeedType>(seedType), powerUp, 1.0f);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic test: one-shot {} ({}) pu {} at ({:.1f}, {:.1f}) -> orb {}", name,
			                   seedType, powerUp, x, z, orb == entt::null ? -1 : static_cast<int>(orb));
			if (orb != entt::null && std::strcmp(tap, "tap") == 0)
			{
				const int result = one_off::InterfaceTap(orb, PlayerNames::PLAYER_ONE);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic test: tapped the orb -> {}", result);
			}
		}
	}
}
