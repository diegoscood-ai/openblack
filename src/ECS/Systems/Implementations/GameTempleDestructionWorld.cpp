/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameTempleDestructionWorld.h"

#include <algorithm>
#include <string>
#include <utility>

#include <LHVM.h>
#include <LHVMTypes.h>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "3D/ModelSurface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerScript.h"
#include "GameClock.h"
#include "Help/ScriptControl.h"
#include "Locator.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/Rules/ExplodeObject.h"
#include "Resources/ResourcesInterface.h"
#include "Temple/HeartFade.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// A temple's sounds are the in-game bank's
constexpr audio::SfxBank k_Bank = audio::SfxBank::InGame;
/// Their tags play while nothing of theirs plays, without following the thing
constexpr int k_TagMode = 2;
/// The heart's five spark sounds follow this one
constexpr int k_FirstSparkSample = 206;
/// A broken-up heart's pieces fly from its place at this speed, this spread about
constexpr float k_BreakUpSpeed = 80.0f;
constexpr float k_BreakUpSpread = 3.0f;

/// The effect of a spot visual, none for a visual that has gone or has no effect
ParticleSystemInterface::EffectId EffectOf(entt::entity visual)
{
	if (visual == entt::null || !Locator::particleSystem::has_value())
	{
		return ParticleSystemInterface::k_NoEffect;
	}
	const auto& containers = Locator::particleSystem::value().GetState().containers;
	const auto found = std::ranges::find(containers, visual, &particles::manager::Container::object);
	return found != containers.end() ? found->effect : ParticleSystemInterface::k_NoEffect;
}
} // namespace

Registry& GameTempleDestructionWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t GameTempleDestructionWorld::MillisecondsPerTurn() const
{
	return game_clock::MsPerTurn();
}

uint32_t GameTempleDestructionWorld::Turn() const
{
	return game_clock::Turn();
}

std::optional<PlayerNames> GameTempleDestructionWorld::LocalPlayer() const
{
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::playerSystem::value().GetLocalPlayer();
}

bool GameTempleDestructionWorld::PlaygroundGame() const
{
	// openblack starts no playground game, as the guidance's query of one also says
	return false;
}

entt::entity GameTempleDestructionWorld::CitadelHeartOf(PlayerNames player) const
{
	return worship::citadel::HeartOf(worship::citadel::Of(player));
}

std::vector<entt::entity> GameTempleDestructionWorld::PartsOf(entt::entity heart) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* part = registry.Valid(heart) ? registry.TryGet<const CitadelHeart>(heart) : nullptr;
	if (part == nullptr || part->citadel == entt::null)
	{
		return {};
	}
	const auto citadel = part->citadel;
	// The worship sites, each put at the head of the temple's parts as it was made, so the newest first; they leave the
	// parts as they are deleted
	std::vector<std::pair<int64_t, entt::entity>> sites;
	registry.Each<const WorshipSite>([citadel, &sites](entt::entity site, const WorshipSite& worship) {
		if (worship.citadel == citadel && ecs::IsAvailable(site))
		{
			sites.emplace_back(object_index::Of(site), site);
		}
	});
	std::ranges::sort(sites, [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<entt::entity> parts;
	parts.reserve(sites.size() + 1);
	for (const auto& site : sites)
	{
		parts.push_back(site.second);
	}
	// The heart was made before its sites
	if (ecs::IsAvailable(heart))
	{
		parts.push_back(heart);
	}
	return parts;
}

std::vector<openblack::physics::temple_heart::Town> GameTempleDestructionWorld::HeartTowns(PlayerNames owner) const
{
	// The player's towns in the order the player has them, each with its buildings and its homeless people, the newest
	// first
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	std::vector<openblack::physics::temple_heart::Town> towns;
	for (const auto town : map_cells::TownsOf(owner))
	{
		auto& list = towns.emplace_back();
		for (const auto abode : town_stats::AbodesOf(town))
		{
			const bool available = ecs::IsAvailable(abode);
			list.buildings.push_back({
			    .entity = abode,
			    .available = available,
			    .life = available ? life::LifeOf(abode) : 0.0f,
			    .built = available ? abodes::GetPercentBuilt(abode) : 0.0f,
			    .field = available && registry.AllOf<Field>(abode),
			    .footballPitch = available && abodes::TypeOf(abode) == AbodeType::FootballPitch,
			});
		}
		for (const auto villager : town_villagers::Homeless(town))
		{
			list.homeless.push_back({.entity = villager, .available = villager::IsAvailable(villager)});
		}
	}
	return towns;
}

bool GameTempleDestructionWorld::Available(entt::entity thing) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (thing == entt::null || !registry.Valid(thing))
	{
		return false;
	}
	// A villager is no longer there once it is dying
	return registry.AllOf<Villager>(thing) ? villager::IsAvailable(thing) : ecs::IsAvailable(thing);
}

float GameTempleDestructionWorld::LifeOf(entt::entity thing) const
{
	return life::LifeOf(thing);
}

glm::vec3 GameTempleDestructionWorld::MapPointOf(entt::entity thing) const
{
	const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(thing);
	return map_coords::ToWorld(map_coords::FromWorld(transform.position));
}

float GameTempleDestructionWorld::HeightOf(entt::entity thing) const
{
	return object::GetHeight(thing);
}

void GameTempleDestructionWorld::TagSound(entt::entity thing, int sample, bool loops)
{
	if (thing == entt::null || !Locator::soundTagSystem::has_value())
	{
		return;
	}
	audio::tags::Create(thing, sample, false, k_TagMode, loops ? -1 : 0, false, true, k_Bank, 0);
}

void GameTempleDestructionWorld::RemoveSoundTags(entt::entity thing, int sample)
{
	if (thing == entt::null || !Locator::soundTagSystem::has_value())
	{
		return;
	}
	audio::tags::Remove(thing, sample, k_Bank);
}

void GameTempleDestructionWorld::PlaySpark(entt::entity heart, bool hurt)
{
	// The next of the five, whether or not it can be heard; in 3D at the heart's point, owned by the heart, not following
	// it
	const auto counter = hurt ? audio::Counter::CitadelSparkDamage : audio::Counter::CitadelSparkEffect;
	const int sample = k_FirstSparkSample + audio::NextCounter(counter);
	audio::PlayOptions options;
	options.sample = {audio::Bank(k_Bank), sample};
	options.owner = audio::Owner::Thing(heart);
	options.is3D = true;
	options.track = false;
	options.position = MapPointOf(heart);
	audio::PlaySoundEffect(options);
}

entt::entity GameTempleDestructionWorld::StartSpotVisual(SpotVisualType type, glm::vec3 position, int32_t turns)
{
	if (!Locator::particleSystem::has_value())
	{
		return entt::null;
	}
	return Locator::particleSystem::value().StartSpotVisual(type, position, turns, entt::null, 1.0f);
}

bool GameTempleDestructionWorld::SpotVisualGoing(entt::entity visual) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return visual != entt::null && registry.Valid(visual) && ecs::IsAvailable(visual);
}

void GameTempleDestructionWorld::SetSpotVisualPlayer(entt::entity visual, PlayerNames player)
{
	if (const auto effect = EffectOf(visual); effect != ParticleSystemInterface::k_NoEffect)
	{
		Locator::particleSystem::value().SetPlayer(effect, static_cast<int>(player));
	}
}

void GameTempleDestructionWorld::SetSpotVisualScale(entt::entity visual, float scale)
{
	if (const auto effect = EffectOf(visual); effect != ParticleSystemInterface::k_NoEffect)
	{
		if (auto* running = Locator::particleSystem::value().Find(effect); running != nullptr)
		{
			running->SetMagnitude(scale);
		}
	}
}

void GameTempleDestructionWorld::AddSpotVisualTarget(entt::entity visual, entt::entity thing)
{
	if (const auto effect = EffectOf(visual); effect != ParticleSystemInterface::k_NoEffect)
	{
		Locator::particleSystem::value().AddTarget(effect, thing);
	}
}

void GameTempleDestructionWorld::PowerGlowByHeartbeat(entt::entity /*visual*/)
{
	// (pending) the glow's power is the local player's heartbeat pulse, from a quarter to whole
	// (temple_destruction::GlowPower): openblack's spot visuals step their effects with no power yet
}

void GameTempleDestructionWorld::AddPlasma(entt::entity visual, const particles::PlasmaCommand& command)
{
	if (const auto effect = EffectOf(visual); effect != ParticleSystemInterface::k_NoEffect)
	{
		Locator::particleSystem::value().AddPlasma(effect, command);
	}
}

std::vector<model_surface::Triangle> GameTempleDestructionWorld::DrawnTrianglesOf(entt::entity object) const
{
	// The model as it is drawn: a temple's outside blended for its look
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mesh = registry.Valid(object) ? registry.TryGet<const Mesh>(object) : nullptr;
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return {};
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return {};
	}
	return model_surface::DrawnTriangles(*meshes.Handle(mesh->id), {.detailLevels = 1, .latestState = 0});
}

glm::mat4 GameTempleDestructionWorld::PlacementOf(entt::entity object) const
{
	return affine::Model(Locator::entitiesRegistry::value().Get<const Transform>(object));
}

GameRandomInterface* GameTempleDestructionWorld::LocalRandom()
{
	return Locator::gameRandom::has_value() ? &Locator::gameRandom::value() : nullptr;
}

float GameTempleDestructionWorld::GameFloatRand(float range)
{
	return game_random::GameFloatRand(range);
}

void GameTempleDestructionWorld::HoldHeartbeat(float pace)
{
	audio::guidance::SetHeartBeatOverride(pace);
}

void GameTempleDestructionWorld::FadeHeart(entt::entity heart, uint8_t alpha)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* temple = registry.TryGet<Temple>(heart);
	if (temple == nullptr)
	{
		return;
	}
	// Its tint takes the alpha. Once it has been drawn see-through it stays so, at the alpha it is drawn with
	temple->fadeAlpha = alpha;
	const float drawn = static_cast<float>(heart_fade::DrawnAlpha(alpha)) / 255.0f;
	if (auto* seeThrough = registry.TryGet<Alpha>(heart); seeThrough != nullptr)
	{
		seeThrough->value = drawn;
		return;
	}
	if (!heart_fade::TurnsSeeThrough(alpha, temple->destroying))
	{
		return;
	}
	// The first see-through draw changes its model's materials for good: no depth written, back faces culled
	if (const auto* mesh = std::as_const(registry).TryGet<const Mesh>(heart);
	    mesh != nullptr && Locator::resources::has_value())
	{
		auto& meshes = Locator::resources::value().GetMeshes();
		if (meshes.Contains(mesh->id))
		{
			meshes.Handle(mesh->id)->SetMaterialProperties(heart_fade::k_SeeThroughMaterial);
		}
	}
	registry.Assign<Alpha>(heart, drawn);
}

void GameTempleDestructionWorld::ShowShell(entt::entity heart, bool shown)
{
	// The renderer draws the shell over the heart while it is shown (heart_shell)
	if (auto* temple = Locator::entitiesRegistry::value().TryGet<Temple>(heart); temple != nullptr)
	{
		temple->shellShown = shown;
	}
}

void GameTempleDestructionWorld::DrawHeart(entt::entity heart, bool drawn)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(heart))
	{
		return;
	}
	if (drawn)
	{
		registry.Remove<NotDrawn>(heart);
	}
	else if (!registry.AllOf<NotDrawn>(heart))
	{
		registry.Assign<NotDrawn>(heart);
	}
}

void GameTempleDestructionWorld::BreakUpModel(entt::entity heart)
{
	// From the heart's place on the map
	const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(heart);
	const auto origin = map_coords::ToWorld(map_coords::FromWorld(transform.position));
	static_cast<void>(particles::explode_object::QueueObject(heart, origin, k_BreakUpSpeed, k_BreakUpSpread, false));
}

void GameTempleDestructionWorld::ToBeDeleted(entt::entity object)
{
	ecs::ToBeDeleted(object, false);
}

void GameTempleDestructionWorld::StartScript(std::string_view name)
{
	// With the script types a game of one player allows
	if (Locator::vm::has_value())
	{
		Locator::vm::value().StartScript(std::string(name),
		                                 static_cast<lhvm::ScriptType>(help::script_control::k_SinglePlayerScriptTypes));
	}
}
