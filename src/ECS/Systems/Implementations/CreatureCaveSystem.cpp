/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureCaveSystem.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/ScreenPick.h"
#include "3D/TempleInteriorInterface.h"
#include "Creature/CaveCopyPick.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/CreatureWatching.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureNaming.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::systems
{

using namespace components;

namespace
{
/// The number of the game's help text for a creature having learnt a miracle, by the miracle's place in its table, 0
/// for none; the magic scroll leaves out those it has none for
uint32_t LearntText(size_t miracle)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value();
	return miracle < info.magicEffect.size()
	           ? static_cast<uint32_t>(magic::GetMagicEffectInfo(info, static_cast<MagicType>(miracle)).creatureLearntEnum)
	           : 0;
}

/// The creature room's copy of the creature under the cursor, as the room draws it: in its creature's mesh, by the
/// body's drawn matrix, in its pose (else the mesh's rest pose) and shaped as its creature is. Each sub-mesh the main
/// view draws counts: not a physics one, of the first level of detail and the first stage
bool CopyUnderCursor(const screen_pick::View& view)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return false;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& meshes = Locator::resources::value().GetMeshes();
	bool over = false;
	registry.Each<const CreatureCopy, const Transform>([&](entt::entity copy, const CreatureCopy& creatureCopy,
	                                                       const Transform& /* unused */) {
		const auto* meshComponent = registry.TryGet<const Mesh>(creatureCopy.source);
		if (over || meshComponent == nullptr || !meshes.Contains(meshComponent->id))
		{
			return;
		}
		const auto mesh = meshes.Handle(meshComponent->id);
		const auto model = ecs::DrawnBodyModel(registry, copy);
		// its pose when it fits the mesh, else the mesh's rest pose
		std::span<const glm::mat4> bones = mesh->GetBoneMatrices();
		if (const auto* animation = registry.TryGet<const CreatureAnimation>(copy); animation != nullptr)
		{
			const auto& drawn = creature_turn_pose::BonesDrawn(*animation);
			if (!drawn.empty() && drawn.size() == mesh->GetBoneMatrices().size())
			{
				bones = drawn;
			}
		}
		// the shape: towards the species' meshes that are loaded, the base standing in for the others
		const auto* shape = registry.TryGet<const CreatureMorph>(copy);
		const auto morph = shape != nullptr ? shape->drawn : creature_morph::Morph {};
		const auto targets =
		    creature_morph::MeshesOf(creatureCopy.species, morph, [&meshes](entt::id_type id) { return meshes.Contains(id); });
		const std::array<entt::id_type, 3> shapeMeshes {targets.evilGood, targets.thinFat, targets.weakStrong};

		const auto box = mesh->GetBoundingBox();
		const float scale = glm::length(glm::vec3(model[0]));
		const cave_copy_pick::Sphere sphere {
		    .centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f)),
		    .radius = glm::length(box.Size() * 0.5f) * scale,
		    .origin = glm::vec3(model[3]),
		};
		std::vector<cave_copy_pick::Part> parts;
		const auto& subMeshes = mesh->GetSubMeshes();
		for (size_t s = 0; s < subMeshes.size(); ++s)
		{
			const auto& subMesh = *subMeshes[s];
			const auto flags = subMesh.GetFlags();
			if (subMesh.IsPhysics() || flags.status != 0 || (flags.lodMask & 1) != 1)
			{
				continue;
			}
			std::array<std::span<const glm::vec3>, 3> shapes {};
			for (size_t axis = 0; axis < shapeMeshes.size(); ++axis)
			{
				if (shape != nullptr && meshes.Contains(shapeMeshes.at(axis)))
				{
					const auto target = meshes.Handle(shapeMeshes.at(axis));
					if (s < target->GetSubMeshes().size())
					{
						shapes.at(axis) = target->GetSubMeshes()[s]->GetSkinLocalPositions();
					}
				}
			}
			cave_copy_pick::Part part;
			part.corners =
			    cave_copy_pick::PoseCorners(model, mesh->IsBoned() ? bones : std::span<const glm::mat4> {}, flags.hasBones,
			                                subMesh.GetSkinBones(), subMesh.GetSkinLocalPositions(), shapes, morph);
			part.uvs = subMesh.GetCollisionUVs();
			part.indices = subMesh.GetCollisionIndices();
			const auto& primitives = subMesh.GetPrimitives();
			const auto& ranges = subMesh.GetCollisionRanges();
			for (size_t p = 0; p < primitives.size() && p < ranges.size(); ++p)
			{
				part.primitives.push_back(
				    {.first = ranges[p].first, .count = ranges[p].second, .twoSided = primitives[p].twoSided});
			}
			parts.push_back(std::move(part));
		}
		over = cave_copy_pick::CursorOver(view, sphere, parts);
	});
	return over;
}
} // namespace

CreatureCaveSystem::CreatureCaveSystem(Services services)
    : _services(std::move(services))
{
}

bool CreatureCaveSystem::InTemple() const
{
	return Locator::temple::has_value();
}

void CreatureCaveSystem::Update()
{
	if (InTemple())
	{
		// The temple's creature room is the cave: F5 takes the player there, and Escape back out
		const auto& temple = Locator::temple::value();
		_screen.open = temple.Active() && temple.GetCurrentRoom() == TempleRoom::CreatureCave;
		return;
	}
	// Without a temple to go into, F5 shows the cave on its own and hides it again
	if (Locator::gameActionSystem::has_value())
	{
		using input::BindableActionMap;
		const auto& actions = Locator::gameActionSystem::value();
		if (actions.Get(BindableActionMap::ZOOM_TO_CREATURE_ROOM) &&
		    actions.GetChanged(BindableActionMap::ZOOM_TO_CREATURE_ROOM))
		{
			if (_screen.open)
			{
				Close();
			}
			else
			{
				Open();
			}
		}
	}
}

void CreatureCaveSystem::Open()
{
	if (InTemple())
	{
		auto& temple = Locator::temple::value();
		if (temple.Active())
		{
			temple.GoToRoom(TempleRoom::CreatureCave);
		}
		else
		{
			temple.Activate(TempleRoom::CreatureCave);
		}
		return;
	}
	// Going into the cave lets go of the creature the camera followed
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	_screen.open = true;
}

void CreatureCaveSystem::Close()
{
	if (InTemple())
	{
		if (Locator::temple::value().Active())
		{
			Locator::temple::value().RequestLeave();
		}
		return;
	}
	_screen.open = false;
}

bool CreatureCaveSystem::Escape()
{
	// In the temple, its own Escape leads the way out
	if (InTemple() || !_screen.open)
	{
		return false;
	}
	Close();
	return true;
}

std::optional<entt::entity> CreatureCaveSystem::GetCreature() const
{
	// The cave is the player's own creature's, whichever creature the camera follows
	return Locator::creatureModeSystem::has_value() ? Locator::creatureModeSystem::value().PlayersCreature() : std::nullopt;
}

bool CreatureCaveSystem::TakeGodsDesire(entt::entity creature)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return false;
	}
	// Only the desires it feels itself count (the original also counts all of them under a start-up option of its own,
	// which openblack does not have)
	const auto activated = [mind](size_t desire) {
		return mind->desires.has_value() && desire < creature_desires::k_DesireCount &&
		       mind->desires->desires.at(desire).activated;
	};
	return creature_perceived_desires::TakeDominant(mind->perceivedDesires, activated).has_value();
}

std::optional<creature_cave::Snapshot> CreatureCaveSystem::Snapshot() const
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(*creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	creature_cave::Snapshot snapshot;
	// Its own name, none without a mind
	snapshot.name = creature_naming::NameOf(registry, *creature);
	snapshot.strength = body->strength;
	snapshot.fatness = body->fatness;
	snapshot.alignment = body->alignment;
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(*creature))
	{
		snapshot.needs = needs->needs;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(*creature);
	if (mind == nullptr)
	{
		return snapshot;
	}
	snapshot.attitudeToPlayer = mind->attitudeToPlayer;
	snapshot.turnsLookedAt = mind->turnsLookedAt;
	// Read as the mind scroll is written, each time, which forgets what it read
	snapshot.takeGodsDesire = [creature = *creature] { return TakeGodsDesire(creature); };
	const auto* tables = _services.mindTables ? _services.mindTables() : nullptr;
	if (mind->learnt.has_value())
	{
		const auto& learnt = *mind->learnt;
		snapshot.creaturesKnown = learnt.creatures.size();
		if (mind->desires.has_value())
		{
			const auto& desires = mind->desires->desires;
			snapshot.anger = desires.at(static_cast<size_t>(creature_desires::Desire::Anger)).value;
			snapshot.compassion = desires.at(static_cast<size_t>(creature_desires::Desire::Compassion)).value;
			for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
			{
				const auto& state = mind->desires->desires.at(d);
				if (state.activated)
				{
					const auto desire = static_cast<creature_desires::Desire>(d);
					snapshot.likes.push_back(
					    {.desire = desire,
					     .level = creature_learning::LikesLevel(desire, state, learnt.initialThresholds.at(d))});
				}
			}
		}
		for (size_t a = 0; a < learnt.opinions.size(); ++a)
		{
			const auto name = tables != nullptr && a < tables->actions.size() ? tables->actions[a].name : std::to_string(a);
			snapshot.opinions.push_back({.action = name, .opinion = learnt.opinions[a]});
		}
		if (tables != nullptr)
		{
			const auto& knowledge = learnt.knowledge;
			for (size_t i = 0; i < tables->skills.size(); ++i)
			{
				const bool known = i < knowledge.skillsKnown.size() && knowledge.skillsKnown[i];
				snapshot.skills.push_back({.name = tables->skills[i].name, .known = known});
			}
			const auto multiplier = creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species));
			for (size_t i = 0; i < tables->miracles.size(); ++i)
			{
				const auto& rule = tables->miracles[i];
				// How far it has learnt it, a share of the times needed, cut short to a whole percent
				const auto count = i < knowledge.miraclesSeen.size() ? knowledge.miraclesSeen[i].count : 0u;
				const auto needed = creature_watching::TimesNeeded(rule.timesToSee, multiplier);
				const auto share = std::min(1.0f, static_cast<float>(count) / needed);
				snapshot.miracles.push_back({
				    .name = rule.name,
				    .percent = static_cast<int32_t>(share * 100.0f),
				    .knownAbout = i < knowledge.miraclesKnown.size() && knowledge.miraclesKnown[i],
				    .learntText = LearntText(i),
				});
			}
		}
	}
	return snapshot;
}

bool CreatureCaveSystem::SetTattoo(entt::entity creature, const std::optional<creature_cave::TattooEdit>& edit) const
{
	if (!edit.has_value())
	{
		return false;
	}
	_services.setTattoo(creature, edit->slot, edit->tattoo);
	return true;
}

bool CreatureCaveSystem::ApplyTattoo(uint8_t site, uint8_t design, glm::u8vec3 colour)
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !_services.setTattoo || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto* tattoos = Locator::entitiesRegistry::value().TryGet<const CreatureTattoos>(*creature);
	return tattoos != nullptr && SetTattoo(*creature, creature_cave::Apply(tattoos->slots, site, design, colour));
}

bool CreatureCaveSystem::CursorOverCaveCreature(const screen_pick::View& view)
{
	_cursorOverCopy = CopyUnderCursor(view);
	return _cursorOverCopy;
}

bool CreatureCaveSystem::RemoveTattoo(uint8_t site)
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !_services.setTattoo || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto* tattoos = Locator::entitiesRegistry::value().TryGet<const CreatureTattoos>(*creature);
	return tattoos != nullptr && SetTattoo(*creature, creature_cave::Remove(tattoos->slots, site));
}

} // namespace openblack::ecs::systems
