/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureBodyKnock.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureKnock.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureTurnPose.h"
#include "Debug/DebugEnv.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureKnock.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePose.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Locator.h"
#include "Particles/ParticleObjectRules.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::creature_body_knock
{
namespace knock = openblack::creature_knock;
using components::Abode;
using components::Animal;
using components::CitadelHeart;
using components::Creature;
using components::CreatureAnimation;
using components::CreatureFighting;
using components::CreatureKnock;
using components::CreatureTurnPose;
using components::Field;
using components::Fragment;
using components::Indestructible;
using components::MagicTree;
using components::Mesh;
using components::SkeletalAnimation;
using components::StoragePit;
using components::Tree;
using components::Villager;

namespace
{
/// OPENBLACK_CREATURE_KNOCK_TRACE: one line each time a creature's knock starts or stops, and each thing it knocks
bool Traced()
{
	static const debug_env::Variable s_Trace("OPENBLACK_CREATURE_KNOCK_TRACE");
	return s_Trace.Get() != nullptr;
}

void Trace(const std::string& line)
{
	if (!Traced())
	{
		return;
	}
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		logger->info("{}", line);
	}
}

/// The species' knock bones, the left ones the body's mirrors of the right; none without its bones that act
std::optional<knock::BoneIndices> IndicesOf(const Registry& registry, entt::entity creature)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body.species);
	if (!rigs.Contains(rigId))
	{
		return std::nullopt;
	}
	const auto& points = rigs.Handle(rigId)->actionPoints;
	if (!points.has_value())
	{
		return std::nullopt;
	}
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto mirror = [animation](uint32_t bone) {
		return animation != nullptr && bone < animation->mirror.size() ? animation->mirror[bone] : bone;
	};
	return knock::BoneIndices {
	    .head = points->head,
	    .rump = points->groin,
	    .rightHand = points->rightHand,
	    .leftHand = mirror(points->rightHand),
	    .rightFoot = points->rightFoot,
	    .leftFoot = mirror(points->rightFoot),
	    .rightArmpit = points->rightArmpit,
	    .leftArmpit = mirror(points->rightArmpit),
	};
}

/// Where a bone is in the pose the game reads, if the body has it
std::optional<glm::vec3> PlaceOf(std::span<const glm::mat4> pose, uint32_t bone)
{
	if (bone >= pose.size())
	{
		return std::nullopt;
	}
	return glm::vec3(pose[bone][3]);
}

void Apply(Registry& registry, entt::entity creature, CreatureKnock& state, knock::Mark mark, const char* why)
{
	if (mark == knock::Mark::Stop)
	{
		if (state.on)
		{
			Trace(fmt::format("Creature {} knock stops: {}", entt::to_integral(creature), why));
		}
		state.on = false;
		return;
	}
	const auto& lookup = std::as_const(registry);
	if (mark == knock::Mark::SetUpAndStart)
	{
		if (const auto indices = IndicesOf(lookup, creature))
		{
			state.bones = knock::Initialise(state.bones, *indices, creature_pose::DrawnSize(lookup, creature));
		}
	}
	const auto* pose = lookup.TryGet<const CreatureTurnPose>(creature);
	const auto bones = pose != nullptr ? creature_turn_pose::Safe(*pose) : std::span<const glm::mat4> {};
	for (auto& bone : state.bones)
	{
		if (const auto place = PlaceOf(bones, bone.bone))
		{
			bone.place = *place;
		}
	}
	Trace(fmt::format("Creature {} knock starts: {}", entt::to_integral(creature), why));
	state.on = true;
}

/// What a thing is to the knock
knock::Facts FactsOf(const Registry& registry, entt::entity object)
{
	const auto type = abodes::TypeOf(object);
	const auto* animal = registry.TryGet<const Animal>(object);
	const bool canFly =
	    Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().CanBecomeAPhysicsObject(object);
	return {
	    .hasModel = registry.AllOf<Mesh>(object),
	    .indestructible = registry.AllOf<Indestructible>(object),
	    .creature = registry.AllOf<Creature>(object),
	    .templeHeart = registry.AllOf<CitadelHeart>(object),
	    .tree = registry.AnyOf<Tree, MagicTree>(object),
	    .storagePit = registry.AllOf<StoragePit>(object) || type == AbodeType::StoragePit,
	    .townCentre = type == AbodeType::TownCentre,
	    .fragment = registry.AllOf<Fragment>(object),
	    .villager = registry.AllOf<Villager>(object),
	    .animal = animal != nullptr,
	    .dove = animal != nullptr && animal_ai::IsDoveClass(animal->type),
	    .immovable = object_flags::IsImmovable(object),
	    .living = registry.AnyOf<Villager, Animal, Creature>(object),
	    .animatedModel = registry.AllOf<SkeletalAnimation>(object),
	    // a field is a home in the game, and the knock leaves it alone as one
	    .home = registry.AnyOf<Abode, Field>(object),
	    .field = registry.AllOf<Field>(object),
	    .canBecomePhysicsObject = canFly,
	};
}

/// Whether the bone meets one of the points of the thing's model, drawn near, through the thing's own world matrix: a
/// model with bones never is met
bool MeetsModel(const Registry& registry, entt::entity object, glm::vec3 at, float radius)
{
	const auto* mesh = registry.TryGet<const Mesh>(object);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return false;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return false;
	}
	const auto& model = *meshes.Handle(mesh->id);
	if ((model.GetFlags() & static_cast<uint32_t>(l3d::L3DMeshFlags::HasBones)) != 0)
	{
		return false;
	}
	const auto matrix = particles::object_surface::WorldMatrixOf(object);
	const knock::Rows rows {.x = matrix.x, .y = matrix.y, .z = matrix.z, .place = matrix.translation};
	for (const auto& subMesh : model.GetSubMeshes())
	{
		if ((subMesh->GetFlags().lodMask & 1) == 0)
		{
			continue;
		}
		const std::span<const glm::vec3> positions = subMesh->GetCollisionPositions();
		for (const auto& [first, count] : subMesh->GetCollisionVertexRanges())
		{
			if (first + count <= positions.size() && knock::MeetsAnyPoint(rows, positions.subspan(first, count), at, radius))
			{
				return true;
			}
		}
	}
	return false;
}

/// The thing flies off as the creature's throw for its player, from where it stands
void PushOff(entt::entity creature, entt::entity object, glm::vec3 velocity)
{
	if (!Locator::dynamicsSystem::has_value() || object_flags::IsImmovable(object))
	{
		return;
	}
	const auto owner = std::as_const(Locator::entitiesRegistry::value()).Get<const Creature>(creature).owner;
	static_cast<void>(Locator::dynamicsSystem::value().AddObject(object, velocity,
	                                                             systems::DynamicsSystemInterface::BodySpin {}, creature, false,
	                                                             creature_object_actions::ThrowerPlayerOf(owner)));
}

/// A home struck by the bone: pieces broken off it, and the damage for the creature's player when something broke off
void Strike(entt::entity creature, entt::entity home, glm::vec3 at, glm::vec3 blow, float radius)
{
	if (!Locator::buildingDamageSystem::has_value())
	{
		return;
	}
	const auto owner = std::as_const(Locator::entitiesRegistry::value()).Get<const Creature>(creature).owner;
	const std::array strikes {systems::BuildingDamageSystemInterface::BodilessBlow {.point = at, .velocity = blow}};
	static_cast<void>(Locator::buildingDamageSystem::value().StrikeWithoutBody(
	    home, creature, owner != PlayerNames::NEUTRAL ? std::optional(owner) : std::nullopt, radius, strikes));
}

/// One bone's knock in its cell, its move made bigger or shorter by each thing it knocks, as the game's is
void KnockInCell(entt::entity creature, size_t index, knock::Bone bone, glm::vec3 move)
{
	const auto cell = knock::CellOf(bone.place);
	if (!map_coords::InBounds(cell))
	{
		return;
	}
	map_cells::ForEachInCell(cell, [&](entt::entity object) {
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		if (!registry.Valid(object))
		{
			return true;
		}
		const auto kind = knock::KindOf(FactsOf(registry, object));
		if (kind == knock::Kind::None)
		{
			return true;
		}
		const auto coords = object::MapCoordsOf(object);
		const glm::vec2 at {static_cast<float>(coords.x) * map_coords::k_MetresPerFixed,
		                    static_cast<float>(coords.z) * map_coords::k_MetresPerFixed};
		if (!knock::WithinReach(kind, at, object::Get2DRadius(object), bone.place, bone.radius))
		{
			return true;
		}
		if (kind != knock::Kind::Living && !MeetsModel(registry, object, bone.place, bone.radius))
		{
			return true;
		}
		if (kind == knock::Kind::Home)
		{
			if (!creature_object_actions::CanHomeBeKicked(fire::IsOnFire(object), abodes::IsBuilt(object)))
			{
				return true;
			}
			const auto blow = knock::HomeBlow(move);
			Trace(fmt::format("Creature {} knock bone {} strikes home {} at ({:.2f}, {:.2f}, {:.2f})",
			                  entt::to_integral(creature), index, entt::to_integral(object), bone.place.x, bone.place.y,
			                  bone.place.z));
			Strike(creature, object, bone.place, blow, bone.radius);
			return true;
		}
		const auto velocity = knock::Push(move);
		Trace(fmt::format("Creature {} knock bone {} pushes {} off at ({:.2f}, {:.2f}, {:.2f})", entt::to_integral(creature),
		                  index, entt::to_integral(object), velocity.x, velocity.y, velocity.z));
		PushOff(creature, object, velocity);
		return true;
	});
}
} // namespace

void TakeMarks()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> creatures;
	lookup.Each<const Creature, const CreatureAnimation>(
	    [&creatures](entt::entity entity, const Creature&, const CreatureAnimation&) { creatures.push_back(entity); });
	for (const auto creature : creatures)
	{
		TakeMarksOf(creature);
	}
}

void TakeMarksOf(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature, CreatureAnimation>(creature))
	{
		return;
	}
	{
		auto* state = registry.TryGet<CreatureKnock>(creature);
		if (state == nullptr)
		{
			state = &registry.AssignState<CreatureKnock>(creature);
		}
		// the states its fighter entered
		if (auto* fighting = registry.TryGet<CreatureFighting>(creature); fighting != nullptr)
		{
			if (const auto mark = std::exchange(fighting->fighter.knockMark, std::nullopt))
			{
				Apply(registry, creature, *state, *mark, "a fight state");
			}
		}
		// the change of what its body plays; the fight marks its own moves
		const auto seen = knock::BodyStateOf(lookup.Get<const CreatureAnimation>(creature).body);
		if (state->seen.has_value() && *state->seen != seen && seen != knock::BodyState::Fighting)
		{
			if (const auto mark = knock::BodyMark(seen))
			{
				Apply(registry, creature, *state, *mark, "a body state");
			}
		}
		state->seen = seen;
		// a slap of the hand
		if (std::exchange(state->slapped, false))
		{
			Apply(registry, creature, *state, knock::Mark::SetUpAndStart, "a slap");
		}
	}
}

void Knock()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> knocking;
	std::as_const(registry).Each<const Creature, const CreatureKnock>(
	    [&knocking](entt::entity entity, const Creature&, const CreatureKnock& state) {
		    if (state.on)
		    {
			    knocking.push_back(entity);
		    }
	    });
	for (const auto creature : knocking)
	{
		KnockOf(creature);
	}
}

void KnockOf(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* knock = registry.Valid(creature) ? std::as_const(registry).TryGet<const CreatureKnock>(creature) : nullptr;
	if (knock == nullptr || !knock->on || !std::as_const(registry).AllOf<Creature>(creature))
	{
		return;
	}
	{
		if (!registry.Valid(creature))
		{
			return;
		}
		const auto* pose = std::as_const(registry).TryGet<const CreatureTurnPose>(creature);
		if (pose == nullptr)
		{
			return;
		}
		const auto bones = creature_turn_pose::Safe(*pose);
		const float thresholdSq = knock::MoveThresholdSq(creature_pose::DrawnSize(std::as_const(registry), creature));
		for (size_t i = 0; i < knock::k_BoneCount; ++i)
		{
			auto* state = registry.TryGet<CreatureKnock>(creature);
			if (state == nullptr)
			{
				break;
			}
			auto& bone = state->bones.at(i);
			const auto now = PlaceOf(bones, bone.bone);
			if (!now.has_value())
			{
				continue;
			}
			const auto move = knock::MoveOf(bone.place, *now, thresholdSq);
			if (!move.knocks)
			{
				continue;
			}
			bone.place = *now;
			KnockInCell(creature, i, bone, move.by);
		}
	}
}

void Slapped(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	auto* state = registry.TryGet<CreatureKnock>(creature);
	if (state == nullptr)
	{
		state = &registry.AssignState<CreatureKnock>(creature);
	}
	state->slapped = true;
}
} // namespace openblack::ecs::creature_body_knock
