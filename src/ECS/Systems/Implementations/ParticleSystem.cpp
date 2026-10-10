/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ParticleSystem.h"

#include <cstdint>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

#include "Audio/Services/SpellSounds.h"
#include "ECS/Town/TownBelief.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Particles/PSys.h"
#include "Particles/ParticleTypes.h"
#include "Particles/Rules/Shield.h"
#include "Particles/Utility.h"
#include "Resources/LoaderTypes.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
[[nodiscard]] game_random::psys::NetGameType ToNetGameType(bool synced)
{
	return synced ? game_random::psys::NetGameType::Synced : game_random::psys::NetGameType::Local;
}
} // namespace

ParticleSystem::EffectId ParticleSystem::Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced)
{
	return psys::manager::Start(std::string(file), origin, magnitude, ToNetGameType(synced));
}

ParticleSystem::EffectId ParticleSystem::Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced)
{
	const auto file = particles::ParticleTypeFile(type);
	return file.empty() ? k_NoEffect : Start(file, origin, magnitude, synced);
}

ParticleSystem::EffectId ParticleSystem::StartForSpell(std::string_view file, glm::vec3 origin, glm::vec3 direction,
                                                       float magnitude, psys::SpellSink* sink, bool synced)
{
	return psys::manager::StartForSpell(std::string(file), origin, direction, magnitude, sink, ToNetGameType(synced));
}

bool ParticleSystem::ProcessForSpell(EffectId id, const psys::ProcessInfo& info, float seconds)
{
	return psys::manager::ProcessForSpell(id, info, seconds);
}

entt::entity ParticleSystem::StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns,
                                             entt::entity owner, float magnitude)
{
	return psys::manager::StartSpotVisual(static_cast<int>(type), position, turns, owner, magnitude);
}

void ParticleSystem::SetPlayer(EffectId id, int player)
{
	if (auto* effect = psys::manager::Find(id); effect != nullptr)
	{
		effect->SetPlayer(player);
	}
}

const particles::ShieldSphere* ParticleSystem::FindShield(glm::vec3 point, float margin) const
{
	return psys::shields::FindShieldContainingPoint(point, margin);
}

size_t ParticleSystem::GetSoundCount() const
{
	return audio::spell_sounds::Count();
}

void ParticleSystem::AddTarget(EffectId id, entt::entity target)
{
	if (auto* effect = psys::manager::Find(id); effect != nullptr)
	{
		effect->AddTarget(target);
	}
}

void ParticleSystem::AddTargetPosition(EffectId id, glm::vec3 position)
{
	if (auto* effect = psys::manager::Find(id); effect != nullptr)
	{
		effect->AddTargetPoint(position);
	}
}

void ParticleSystem::AddBeliefSprite(const particles::BeliefSprite& sprite)
{
	ecs::town_belief::QueueBeliefSprite(sprite.position, sprite.amount, sprite.colour);
}

void ParticleSystem::AddGestureTrail(std::shared_ptr<particles::GestureTrail> trail)
{
	if (trail != nullptr)
	{
		// moved out only when nobody else holds it
		_state.pendingGestures.push_back(trail.use_count() == 1 ? std::move(*trail) : *trail);
	}
}

void ParticleSystem::UpdateFrame(float gameSeconds, const HandFrame& hand)
{
	for (const auto& sheet : LightSheets())
	{
		sheet->Update(gameSeconds);
	}
	psys::utility::Update(gameSeconds, hand.position, hand.size, glm::distance(hand.cameraPosition, hand.position));
}

std::vector<std::shared_ptr<particles::LightSheet>> ParticleSystem::LightSheets()
{
	std::erase_if(_state.lightSheets, [](const auto& sheet) { return sheet.expired(); });
	std::vector<std::shared_ptr<particles::LightSheet>> sheets;
	sheets.reserve(_state.lightSheets.size());
	for (const auto& sheet : _state.lightSheets)
	{
		sheets.push_back(sheet.lock());
	}
	return sheets;
}

std::vector<ParticleSystem::EffectInfo> ParticleSystem::GetEffects() const
{
	// the turns left of each spot visual's effect
	std::unordered_map<uint32_t, int> turnsLeft;
	for (const auto& container : _state.containers)
	{
		turnsLeft.try_emplace(container.effect, container.turns);
	}
	std::vector<EffectInfo> result;
	for (const auto& [id, running] : _state.effects)
	{
		const auto& effect = *running.effect;
		std::optional<float> secondsLeft;
		if (const auto turns = turnsLeft.find(id); turns != turnsLeft.end() && turns->second >= 0)
		{
			secondsLeft = static_cast<float>(turns->second) * game_clock::k_TurnSeconds;
		}
		result.push_back({
		    .id = id,
		    .file = effect.GetFile().name,
		    .origin = effect.GetOrigin(),
		    .age = effect.GetAge(),
		    .atoms = effect.AtomCount(),
		    .collections = effect.CollectionCount(),
		    .closing = effect.Closing(),
		    .ownedBySpell = running.ownedBySpell,
		    .path = running.path,
		    .targets = effect.GetTargets().size() + effect.TargetPointCount(),
		    .secondsLeft = secondsLeft,
		    .unportedClasses = effect.UnportedClasses(),
		});
	}
	return result;
}

std::vector<std::string> ParticleSystem::GetFileNames() const
{
	return resources::PSysFileNames();
}
