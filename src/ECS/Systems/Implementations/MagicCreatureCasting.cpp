/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Creatures casting miracles, on the miracles' core. A creature casts at an object as big as the object: a little
// bigger than its radius on the ground, a creature by its size, a fire seed's miracle by the caster's own height. The
// miracle takes its prayer power to start with and the creatures' timer from its tables, and the creature pays what it
// costs to make with its body; it can't cast while too tired to, nor at something the miracle can't be cast at. It is
// cast at the object's feet, or on the object for a seed that casts on objects. Its effect flows from between the
// creature's hands; a miracle cast from above flows from above what it is cast at, with a beam from each hand aimed at
// that point, which follows that hand each turn. Letting go of it stops a miracle that lasts only while held, such as a
// lightning bolt, and closes its beams down: they fade out, still following the hands, and go once their effects are
// done. A cast that is not from above leaves the closing beams of the cast before on the hands. Nothing casts through
// here yet.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/CreatureBody.h"
#include "Creature/CreatureHands.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSpellCasting.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/MagicTables.h"
#include "MagicSystem.h"
#include "ObjectMeasures.h"
#include "Particles/SpellLink.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Casting from above, the creature's effect flows from at least this far above what it is cast at, or this share of
/// its height above it
constexpr float k_CastFromAboveLeast = 10.0f;
constexpr float k_CastFromAboveShare = 1.3f;
/// The beams of a cast from above are this big, and this much bigger for each of the creature's size
constexpr float k_BeamSizeLeast = 0.4f;
constexpr float k_BeamSizePerSize = 0.3f;

/// The rig of a creature's species, if it is loaded
const creature::CreatureRig* RigOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body->species);
	return rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
}
} // namespace

std::array<entt::entity, 2> MagicSystem::StartCreatureBeams(entt::entity creature, glm::vec3 aim)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* rig = RigOf(registry, creature);
	const auto points = rig != nullptr ? creature_hands::BeamPointsOf(registry, creature, *rig) : std::nullopt;
	if (!points.has_value() || !Locator::particleSystem::has_value())
	{
		return {entt::null, entt::null};
	}
	auto& particles = Locator::particleSystem::value();
	const float size = k_BeamSizeLeast + (k_BeamSizePerSize * ecs::object::GetScale(creature));
	// Each is aimed as it is made, the mirror's first
	const auto mirror = particles.StartSpotVisual(SpotVisualType::MagicBeam, points->first, std::nullopt, creature, size);
	particles.AddSpotVisualTarget(mirror, aim);
	const auto hand = particles.StartSpotVisual(SpotVisualType::MagicBeam, points->second, std::nullopt, creature, size);
	particles.AddSpotVisualTarget(hand, aim);
	return {mirror, hand};
}

float MagicSystem::CastFromAboveRaise(float targetHeight)
{
	const float raised = k_CastFromAboveShare * targetHeight;
	return raised >= k_CastFromAboveLeast ? raised : k_CastFromAboveLeast;
}

bool MagicSystem::CanCreatureCastAt(MagicType type, entt::entity target)
{
	return magic::cast_rules::CanCastOn(type, target);
}

bool MagicSystem::CastByCreature(entt::entity creature, MagicType type, entt::entity target)
{
	// It lets go of what it held first
	ReleaseCreatureCast(creature);
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto at = object_measures::PositionOf(lookup, target);
	if (!lookup.Valid(creature) || !lookup.AllOf<Creature>(creature) || !at.has_value() || type == MagicType::None ||
	    static_cast<size_t>(type) >= magic::k_MagicTypeCount || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value();
	// As big as what it is cast at: a creature by its size, a fire seed's miracle by the caster's own height
	const bool fireSeed = magic::GetMagicInfo(info, type).spellSeedType == SpellSeedType::Fire;
	const auto targetScale = lookup.AllOf<Creature>(target) ? std::optional(ecs::object::GetScale(target)) : std::nullopt;
	const float magnitude = creature_spell_casting::CastMagnitude(object_measures::TwoDRadius(lookup, target), targetScale,
	                                                              fireSeed, object_measures::Height(lookup, creature));
	const auto& effect = magic::GetMagicEffectInfo(info, type);
	magic::SpellCastData cast {
	    .magnitude = magnitude,
	    .chants = effect.initialChants,
	    .duration = magic::GetTimerWhenCreatureCasting(info, type),
	    .maxObjectsToCreate = -1,
	};
	// Too tired, or at something the miracle can't be cast at, it can't cast
	if (!magic::creator::CreatureCanPayFor(creature, effect.costToCreate) || !CanCreatureCastAt(type, target))
	{
		return false;
	}
	// Its hands and the way from them, then the point it is cast at; cast from above, its effect flows from above that
	// point, which stays, with a beam from each hand
	const auto creator = magic::creator::OfCreature(creature);
	psys::ProcessInfo process {};
	magic::creator::UpdateSpellInfo(creator, entt::null, process);
	process.interfacePos = magic::ToWorld(magic::ToMap(*at));
	std::array<entt::entity, 2> beams {entt::null, entt::null};
	const bool fromAbove = magic::IsCreatureCastFromAbove(info, type);
	if (fromAbove)
	{
		process.handPos =
		    process.interfacePos + glm::vec3(0.0f, CastFromAboveRaise(object_measures::Height(lookup, target)), 0.0f);
		beams = StartCreatureBeams(creature, process.handPos);
	}
	entt::entity spell = entt::null;
	magic::CastAtObject(type, creator, target, &spell, &cast, process);
	auto* existing = registry.TryGet<CreatureCasting>(creature);
	auto& casting = existing != nullptr ? *existing : registry.AssignState<CreatureCasting>(creature);
	casting.held = spell;
	// Only a cast from above has beams of its own; any other leaves those of the cast before, closing down
	if (fromAbove)
	{
		casting.beams = beams;
	}
	// The creature pays for making it with its body
	if (spell != entt::null)
	{
		static_cast<void>(magic::creator::MaintainSpell(creator, spell, effect.costToCreate));
	}
	return true;
}

void MagicSystem::ReleaseCreatureCast(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!lookup.Valid(creature) || !lookup.AllOf<CreatureCasting>(creature))
	{
		return;
	}
	auto& casting = registry.Get<CreatureCasting>(creature);
	// The miracle it holds stops when it lasts only while held; any other runs its time
	if (casting.held != entt::null)
	{
		const auto* spell = lookup.Valid(casting.held) ? lookup.TryGet<const Spell>(casting.held) : nullptr;
		if (spell != nullptr && !(spell->duration >= 0.0f))
		{
			CloseDown(casting.held);
		}
		casting.held = entt::null;
	}
	// Then its beams close down: they are kept, still following the hands each turn, until their effects are done
	if (Locator::particleSystem::has_value())
	{
		for (const auto beam : casting.beams)
		{
			if (beam != entt::null)
			{
				Locator::particleSystem::value().CloseDownSpotVisual(beam);
			}
		}
	}
}

void MagicSystem::KeepCreatureBeams()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> casting;
	lookup.Each<const CreatureCasting>([&casting](entt::entity entity, const CreatureCasting& cast) {
		if (cast.held != entt::null || cast.beams[0] != entt::null || cast.beams[1] != entt::null)
		{
			casting.push_back(entity);
		}
	});
	for (const auto creature : casting)
	{
		// What it holds that has gone, or is going, is no longer held
		auto& cast = registry.Get<CreatureCasting>(creature);
		if (cast.held != entt::null && !ecs::IsAvailable(cast.held))
		{
			cast.held = entt::null;
		}
		for (auto& beam : cast.beams)
		{
			if (beam != entt::null && !ecs::IsAvailable(beam))
			{
				beam = entt::null;
			}
		}
		// The beams that are left go to the hand's mirror and the hand, as the body was posed the turn before
		const auto* rig = RigOf(lookup, creature);
		const auto points = rig != nullptr ? creature_hands::BeamPointsOf(lookup, creature, *rig) : std::nullopt;
		if (!points.has_value())
		{
			continue;
		}
		const std::array<glm::vec3, 2> at {points->first, points->second};
		for (size_t i = 0; i < cast.beams.size(); ++i)
		{
			if (cast.beams.at(i) != entt::null && lookup.AllOf<Transform>(cast.beams.at(i)))
			{
				registry.Get<Transform>(cast.beams.at(i)).position = at.at(i);
			}
		}
	}
}
