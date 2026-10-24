/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_VortexAttract: the rule of the effect that draws things into a way out of a land. The vortex hands its effect the
// things it takes (ecs::vortex::TakeIn); each step the rule first starts carrying each of them, the last handed first,
// with a particle of its own from where it stood (the thing leaves the land and its own doings and, unless it can't be
// destroyed, its town or flock, and is written to the crossing file), then moves every particle round and down into the
// middle (vortex::crossing::Attract). A particle that reaches the middle goes, and the thing it carried with it. Wiki:
// docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex.

#include "VortexAttract.h"

#include <memory>
#include <string>

#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Vortex.h"
#include "Locator.h"
#include "Magic/VortexCrossingRules.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleExitGuard.h"
#include "Particles/ParticleManager.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"

using namespace openblack;
using namespace openblack::particles;
using namespace openblack::vortex::crossing;

namespace
{
float LandAt(glm::vec2 xz)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
}

/// The thing a vortex draws in: its particle's creator, one per particle, kept in the particle's data so that it goes
/// with it. When the particle goes, at the middle or with its effect, the thing it carried goes too
struct VortexCarrier final: Creator
{
	entt::entity object {entt::null};

	VortexCarrier() { particles::exit_guard::Watch(); }
	VortexCarrier(const VortexCarrier&) = delete;
	VortexCarrier& operator=(const VortexCarrier&) = delete;
	VortexCarrier(VortexCarrier&&) = delete;
	VortexCarrier& operator=(VortexCarrier&&) = delete;
	~VortexCarrier() override
	{
		if (particles::exit_guard::Exiting() || !Locator::entitiesRegistry::has_value() || object == entt::null ||
		    !Locator::entitiesRegistry::value().Valid(object))
		{
			return;
		}
		ecs::ToBeDeleted(object);
	}
};

/// The key of a particle's carrier in its data (the rule's own data is under the rule itself)
const char k_CarrierKeyByte = 0;
const auto* const k_CarrierKey = reinterpret_cast<const Modifier*>(&k_CarrierKeyByte);

/// The rule's data on each particle it carries
struct AttractData
{
	/// The velocity the thing was taken with, and its height above the land then
	glm::vec3 takenVelocity {0.0f};
	float startHeight {0.0f};
	bool indestructible {false};
	bool started {false};
};

class VortexAttract final: public Modifier
{
public:
	explicit VortexAttract(const Object& object)
	{
		settings.radialSpeed = object.Float("RadialSpeed", settings.radialSpeed);
		settings.thetaSpeed = object.Float("ThetaSpeed", settings.thetaSpeed);
		settings.deletionRadius = object.Float("DeletionRadius", settings.deletionRadius);
		settings.minRadius = object.Float("MinRadius", settings.minRadius);
		settings.blendTime = object.Float("BlendTime", settings.blendTime);
		settings.blendTimeVelocity = object.Float("BlendTimeVel", settings.blendTimeVelocity);
		settings.yOffset = object.Float("YParam_YOffset", settings.yOffset);
		settings.rZeroHeight = object.Float("YParam_RZeroHeight", settings.rZeroHeight);
	}

	/// Modifier flag 2 without 4: it waits for the things handed to it for as long as its vortex lasts
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// the things handed over since the last step, the last handed first
		for (auto target = effect.TakeTarget(); target != entt::null; target = effect.TakeTarget())
		{
			if (const auto record = ecs::vortex::TakeQueued(target))
			{
				Adopt(effect, collection, *record);
			}
		}
		const float dt = effect.GetDt();
		if (!(dt > 0.0f))
		{
			return true;
		}
		const float invDt = 1.0f / dt;
		const auto origin = effect.GetOrigin();
		const GroundAt ground = [](const map_coords::MapCoords& at) { return LandAt(map_coords::ToMetres(at)); };
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			auto& data = AtomDataOf<AttractData>(atom, this);
			if (!data.started)
			{
				++i;
				continue;
			}
			const AttractAtom state {.position = atom.position,
			                         .takenVelocity = data.takenVelocity,
			                         .startHeight = data.startHeight,
			                         .age = effect.AtomAge(atom),
			                         .indestructible = data.indestructible};
			if (const auto step = Attract(settings, state, origin, dt, invDt, ground))
			{
				atom.position = step->position;
				atom.velocity = step->velocity;
				++i;
				continue;
			}
			// at the middle the particle goes, and the thing it carried with it (its carrier's end). The original's way
			// of throwing back out what can't be destroyed needs a vortex only a saved game gives the particle, so in
			// play that too is gone
			effect.NotifyAtomRemoved(atom);
			collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
		}
		return true;
	}

private:
	/// A particle starts carrying the thing from where it was handed over, as it stood
	void Adopt(Effect& effect, Collection& collection, const ecs::components::VortexObjectInfo& record) const
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto object = record.object;
		if (object == entt::null || !registry.Valid(object))
		{
			return;
		}
		// what struck it in flight leaves the physics first: only while it still flies there and no particle carries it
		if (record.fromPhysics)
		{
			if (registry.AllOf<ecs::components::InPhysics>(object) &&
			    !registry.AllOf<ecs::components::CarriedByTornado>(object) && Locator::dynamicsSystem::has_value())
			{
				Locator::dynamicsSystem::value().RemoveObject(object);
			}
			else
			{
				return;
			}
		}
		// it leaves the land as a thing starting to move does, without a body of its own: refused when it is moving
		// already or cannot be moved
		if (!ecs::physics::particle_carried_objects::Take(object))
		{
			return;
		}
		// it leaves its town or flock and is written down for the next land, unless it can't be destroyed
		if (!record.indestructible && record.vortex != entt::null)
		{
			ecs::vortex::HandOff(record.vortex, object);
		}
		auto carrier = std::make_shared<VortexCarrier>();
		carrier->kind = Creator::Kind::Other;
		carrier->className = "RenderParticleGameObject";
		carrier->object = object;
		auto& atom = effect.NewAtom(collection, carrier.get(), {});
		atom.modifierData.insert_or_assign(k_CarrierKey, std::static_pointer_cast<void>(carrier));
		// the particle takes the matrix it was handed: the place, its size as the length of its up axis, and the turn
		const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
		const float scaleY = transform != nullptr ? transform->scale.y : 1.0f;
		atom.position = record.position;
		atom.ruleScale = glm::length(record.rows[1] * scaleY);
		atom.baseScale = 1.0f;
		atom.rotation = record.rows;
		auto& data = AtomDataOf<AttractData>(atom, this);
		data.started = true;
		data.takenVelocity = record.velocity;
		data.startHeight = atom.position.y - LandAt({atom.position.x, atom.position.z});
		data.indestructible = record.indestructible;
	}

	AttractSettings settings;
};
} // namespace

void vortex_attract::UpdateCarriedObjects()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	bool moved = false;
	for (const auto& drawable : manager::Collect(Creator::Kind::Other))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* carrier = dynamic_cast<const VortexCarrier*>(atom.creator);
			if (carrier == nullptr || !ecs::IsAvailable(carrier->object))
			{
				continue;
			}
			// the thing's matrix is the particle's (its turn and size)
			if (auto* transform = registry.TryGet<ecs::components::Transform>(carrier->object); transform != nullptr)
			{
				transform->position = atom.position;
				transform->rotation = atom.rotation;
				transform->scale = glm::vec3(atom.scale);
				moved = true;
			}
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

size_t vortex_attract::CarriedCount()
{
	size_t count = 0;
	for (const auto& drawable : manager::Collect(Creator::Kind::Other))
	{
		for (const auto& atom : drawable.atoms)
		{
			count += dynamic_cast<const VortexCarrier*>(atom.creator) != nullptr ? 1 : 0;
		}
	}
	return count;
}

void openblack::particles::RegisterVortexAttractRules()
{
	RegisterModifier("UR_VortexAttract", MakeModifierOf<VortexAttract>);
}
