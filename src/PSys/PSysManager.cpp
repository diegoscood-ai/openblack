/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PSysManager.h"

#include "TownBelief.h"

#include <cstdio>
#include <cstdlib>

#include <array>
#include <chrono>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/SpellSounds.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// GSpotVisualInfo (info.dat DETAIL_SPOT_VISUAL) -> PARTICLE_TYPE -> spell file (name table 0xD4EBB0), and the default
/// life in turns. Empty: no spell file (the effect is drawn by other code or not at all).
struct SpotVisual
{
	std::string_view file;
	int life;
};
constexpr std::array<SpotVisual, 50> k_SpotVisuals = {{
    {"", 0},                            // 0 NONE
    {"", 100},                          // 1 APPLY_SPELL_EFFECT
    {"SF_GripLandscape", 100},          // 2 GRIP_LANDSCAPE
    {"", 100},                          // 3 SUCEED_CAST
    {"SF_FailedApply", 100},            // 4 FAIL_CAST
    {"", 200},                          // 5 FIREWORK_SINGLE
    {"SF_FireWorks", 200},              // 6 FIREWORKS
    {"SF_FireWorks", 200},              // 7 FIREWORKS_PU1
    {"SF_FireWorks", 200},              // 8 FIREWORKS_PU2
    {"SF_MagicObjectCreated", 100},     // 9 MAGIC_OBJECT_CREATED
    {"SF_CreatureTarget", 100},         // 10 COMMAND_SUCCEED
    {"SF_MagicObjectCreated", 100},     // 11 COMMAND_FAIL
    {"SF_CreatureTarget", -1},          // 12 CREATURE_TARGET
    {"SF_SimpleBeamCreatureCast", -1},  // 13 CREATURE_CAST_VISUAL
    {"SF_TeleportVillager", 30},        // 14 VILLAGER_TELEPORT
    {"", 30},                           // 15 FIRE_FX
    {"", 30},                           // 16 FIRE_FX_ON_OBJECT
    {"SF_VolFX", 30},                   // 17 MAGIC_FX
    {"SF_VolFXArtifact", -1},           // 18 MAGIC_FX_ON_OBJECT
    {"SF_VolFXCitadel", -1},            // 19 MAGIC_FX_ON_CITADEL
    {"SF_SimpleBeam", 30},              // 20 MAGIC_BEAM
    {"SF_SimpleBeamCitadel", -1},       // 21 MAGIC_BEAM_ON_CITADEL
    {"SF_Steam", 30},                   // 22 STEAM
    {"SF_Smoke", 30},                   // 23 SMOKE
    {"", 30},                           // 24 DUST
    {"SF_Bonfire", 30},                 // 25 BONFIRE
    {"SF_EvilSmoke", 30},               // 26 EVIL_SMOKE
    {"SF_MagicObjectCreated2", 30},     // 27 OBJECT_APPEAR
    {"", 30},                           // 28 OBJECT_DISAPPEAR
    {"SF_SmokeExplode", 30},            // 29 BANG
    {"", 30},                           // 30 SING_STONES_GLOW
    {"SF_PlayerIconFountain", 60},      // 31 PLAYER_ICON_FOUNTAIN
    {"SF_BeamExplosionCitadel", -1},    // 32 EXPLOSION_CITADEL
    {"SF_HealChakra", 60},              // 33 HEAL_FX
    {"SF_HighlightOnObject", -1},       // 34 HIGHLIGHT_ON_OBJECT
    {"SF_LightningSingleStrike", 40},   // 35 LIGHTNING_STRIKE
    {"SF_BeamExplosionFX", 60},         // 36 BEAM_EXPLOSION_FX
    {"SF_Butterflies", 200},            // 37 BUTTERFLIES
    {"SF_ButterfliesOnObject", 200},    // 38 BUTTERFLIES_ON_OBJECT
    {"SF_Flies", 200},                  // 39 FLIES
    {"SF_FliesOnObject", 200},          // 40 FLIES_ON_OBJECT
    {"SF_SimpleBeamCreatureSwap", 200}, // 41 MAGIC_BEAM_CREATURE_SWAP
    {"SF_Flash", 50},                   // 42 FLASH
    {"SF_TickerTape", 100},             // 43 TICKER_TAPE
    {"SF_ForestCreated", 50},           // 44 FOREST_CREATED
    {"SF_SingingStonesHeal", 100},      // 45 SINGING_STONES_HEAL
    {"SF_SparklesFromObject", 100},     // 46 PILEFOOD_SPEEDUP
    {"SF_SeeThisBeam", 100},            // 47 SEE_THIS_BEAM
    {"", 100},                          // 48 SEE_THIS_BEAM2
    {"", 100},                          // 49 TEST
}};

struct Running
{
	std::unique_ptr<Effect> effect;
	bool ownedBySpell {false}; ///< stepped by its spell (StartForSpell), not by ProcessTurn
	bool perFrame {false};     ///< stepped every frame (the hand's and the interface's effects): drawn as last stepped
};

struct Container
{
	uint32_t effect {0};
	entt::entity object {entt::null}; ///< the script's handle
	entt::entity owner {entt::null};
	int turns {-1}; ///< -1: forever
	bool hadOwner {false};
};

std::unordered_map<uint32_t, Running> g_Effects;
std::vector<Container> g_Containers;
uint32_t g_NextId = 1;
uint32_t g_Seed = 12345;
bool g_DebugDone = false;
} // namespace

uint32_t manager::Start(const std::string& file, glm::vec3 origin, float magnitude)
{
	auto data = File::Load(file);
	if (!data)
	{
		return 0;
	}
	const uint32_t id = g_NextId++;
	g_Effects[id].effect = std::make_unique<Effect>(std::move(data), origin, magnitude, g_Seed++ * 2654435761u);
	return id;
}

uint32_t manager::StartForSpell(const std::string& file, glm::vec3 origin, glm::vec3 direction, float magnitude,
                                SpellSink* sink)
{
	const uint32_t id = Start(file, origin, magnitude);
	if (id == 0)
	{
		return 0;
	}
	auto& running = g_Effects[id];
	running.ownedBySpell = true;
	running.effect->SetDirection(direction);
	running.effect->SetSink(sink);
	return id;
}

bool manager::ProcessForSpell(uint32_t id, const ProcessInfo& info, float dt)
{
	const auto it = g_Effects.find(id);
	if (it == g_Effects.end())
	{
		return false;
	}
	auto& effect = *it->second.effect;
	effect.SetProcessInfo(info);
	effect.Step(dt);
	if (effect.Finished() || (effect.Closing() && effect.DeleteOnCloseDown()))
	{
		g_Effects.erase(it);
		return false;
	}
	return true;
}

void manager::Delete(uint32_t id)
{
	g_Effects.erase(id);
}

void manager::SetPerFrame(uint32_t id)
{
	if (const auto it = g_Effects.find(id); it != g_Effects.end())
	{
		it->second.perFrame = true;
	}
}

Effect* manager::Find(uint32_t id)
{
	const auto it = g_Effects.find(id);
	return it == g_Effects.end() ? nullptr : it->second.effect.get();
}

uint32_t manager::IdOf(const Effect* effect)
{
	for (const auto& [id, running] : g_Effects)
	{
		if (running.effect.get() == effect)
		{
			return id;
		}
	}
	return 0;
}

void manager::CloseDown(uint32_t id)
{
	if (const auto it = g_Effects.find(id); it != g_Effects.end())
	{
		it->second.effect->CloseDown();
	}
}

void manager::SetOrigin(uint32_t id, glm::vec3 origin)
{
	if (const auto it = g_Effects.find(id); it != g_Effects.end())
	{
		it->second.effect->SetOrigin(origin);
	}
}

entt::entity manager::CreateSpotVisual(int spotVisual, glm::vec3 position, float seconds, entt::entity owner,
                                       float magnitude)
{
	if (spotVisual < 0 || spotVisual >= static_cast<int>(k_SpotVisuals.size()))
	{
		return entt::null;
	}
	const auto& info = k_SpotVisuals[static_cast<size_t>(spotVisual)];
	if (info.file.empty())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "PSys: spot visual {} has no spell file", spotVisual);
		return entt::null;
	}
	const uint32_t id = Start(std::string(info.file), position, magnitude);
	if (id == 0)
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto object = registry.Create();
	registry.Assign<ecs::components::Transform>(object, position, glm::mat3(1.0f), glm::vec3(1.0f));
	// turns = ftol(seconds x 1000 / turn ms); 0 takes the entry's life
	int turns = info.life;
	if (seconds < 0.0f)
	{
		turns = -1;
	}
	else if (seconds > 0.0f)
	{
		turns = static_cast<int>(seconds * 1000.0f / static_cast<float>(game_clock::MsPerTurn()));
	}
	g_Containers.push_back({id, object, owner, turns, owner != entt::null});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys: spot visual {} ({}) at ({:.1f}, {:.1f}, {:.1f}) for {} turns", spotVisual,
	                   info.file, position.x, position.y, position.z, turns);
	return object;
}

void manager::CloseSpotVisual(entt::entity object)
{
	// the container's effect closes down on the next turn, as when a script deletes it (ProcessTurn)
	auto& registry = Locator::entitiesRegistry::value();
	if (object != entt::null && registry.Valid(object))
	{
		registry.Destroy(object);
	}
}

void manager::ProcessTurn(float turnSeconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (auto it = g_Containers.begin(); it != g_Containers.end();)
	{
		const auto effect = g_Effects.find(it->effect);
		if (effect == g_Effects.end())
		{
			if (registry.Valid(it->object))
			{
				registry.Destroy(it->object);
			}
			it = g_Containers.erase(it);
			continue;
		}
		const bool ownerGone = it->hadOwner && !registry.Valid(it->owner);
		const bool deleted = !registry.Valid(it->object);
		if (ownerGone || deleted)
		{
			effect->second.effect->CloseDown();
		}
		else if (it->turns >= 0 && --it->turns <= 0)
		{
			effect->second.effect->CloseDown();
		}
		if (it->hadOwner && !ownerGone)
		{
			if (const auto* transform = registry.TryGet<const ecs::components::Transform>(it->owner); transform != nullptr)
			{
				effect->second.effect->SetOrigin(transform->position);
			}
		}
		else if (!deleted)
		{
			if (const auto* transform = registry.TryGet<const ecs::components::Transform>(it->object); transform != nullptr)
			{
				effect->second.effect->SetOrigin(transform->position);
			}
		}
		++it;
	}
	static const bool trace = std::getenv("OPENBLACK_PSYS_TRACE") != nullptr;
	static uint32_t turn = 0;
	++turn;
	for (auto it = g_Effects.begin(); it != g_Effects.end();)
	{
		if (it->second.ownedBySpell)
		{
			++it;
			continue;
		}
		auto& effect = *it->second.effect;
		effect.Step(turnSeconds);
		if (trace && turn % 20 == 0)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys trace: effect {} {} age {:.1f} atoms {} closing {}", it->first,
			                   effect.GetFile().name, effect.GetAge(), effect.AtomCount(), effect.Closing());
		}
		// Process_ returns 5 (delete) when finished, or at once on close-down with DeleteOnCloseDown
		if (effect.Finished() || (effect.Closing() && effect.DeleteOnCloseDown()))
		{
			it = g_Effects.erase(it);
			continue;
		}
		++it;
	}
}

void manager::RunDebugHooks()
{
	// OPENBLACK_TEST_PSYS="SF_Name,x,z[,height[,magnitude[,seconds]]]": that spell file at (x, ground + height, z);
	// seconds > 0 closes it down after that long
	if (g_DebugDone || !Locator::terrainSystem::has_value())
	{
		return;
	}
	g_DebugDone = true;
	const char* test = std::getenv("OPENBLACK_TEST_PSYS");
	if (test == nullptr)
	{
		return;
	}
	char name[64] = {};
	float x = 0.0f, z = 0.0f, height = 0.0f, magnitude = 1.0f, seconds = -1.0f;
	if (std::sscanf(test, "%63[^,],%f,%f,%f,%f,%f", name, &x, &z, &height, &magnitude, &seconds) >= 3)
	{
		const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
		const auto id = Start(name, glm::vec3(x, ground + height, z), magnitude);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys test: {} at ({}, {}, {}) -> effect {}", name, x, ground + height, z, id);
		if (id != 0 && seconds > 0.0f)
		{
			auto& registry = Locator::entitiesRegistry::value();
			const auto object = registry.Create();
			registry.Assign<ecs::components::Transform>(object, glm::vec3(x, ground + height, z), glm::mat3(1.0f), glm::vec3(1.0f));
			g_Containers.push_back({id, object, entt::null, game_clock::TicksForSeconds(seconds), false});
		}
	}
}

void manager::Clear()
{
	g_Effects.clear();
	g_Containers.clear();
	g_DebugDone = false;
	town_belief::Clear();
	audio::spell_sounds::Clear();
}

namespace
{
std::vector<manager::DrawableSource>& DrawableSources()
{
	static std::vector<manager::DrawableSource> sources;
	return sources;
}
} // namespace

std::vector<manager::Drawable> manager::Collect(Creator::Kind kind)
{
	// g_game +0x205D64, the fraction of the turn GJPSysInterface::Draw_ 0x67370D passes on
	const float t = game_clock::TurnFraction();
	std::vector<Drawable> result;
	for (const auto& [id, running] : g_Effects)
	{
		Drawable drawable {running.effect->GetOrigin(), {}, running.perFrame ? 1.0f : t};
		running.effect->Collect(drawable.t, drawable.atoms, kind);
		if (!drawable.atoms.empty())
		{
			result.push_back(std::move(drawable));
		}
	}
	if (kind == Creator::Kind::Sprite && Locator::camera::has_value() && Locator::entitiesRegistry::has_value())
	{
		town_belief::Collect(Locator::camera::value().GetOrigin(), result);
	}
	for (const auto source : DrawableSources())
	{
		source(result);
	}
	return result;
}

std::vector<Effect::DrawChain> manager::CollectChains()
{
	// g_game +0x205D64, the fraction of the turn GJPSysInterface::Draw_ 0x67370D passes on
	const float t = game_clock::TurnFraction();
	std::vector<Effect::DrawChain> result;
	for (const auto& [id, running] : g_Effects)
	{
		running.effect->CollectChains(running.perFrame ? 1.0f : t, result);
	}
	return result;
}

void manager::AddDrawableSource(DrawableSource source)
{
	DrawableSources().push_back(source);
}
