/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The vortex attract rule: the rule of the effect that draws things into a way out of a land. The vortex hands its effect the
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
#include "ECS/Components/DrawImportance.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Components/ParticleDrawnMatrix.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Vortex.h"
#include "GameClock.h"
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
	/// What a save writes of the carried thing beside it (vortex_attract::CarriedObject); the object is above
	mutable vortex_attract::CarriedObject saved;
	/// Whether the thing goes with the particle: not for one a load is still making, whose effect may yet be dropped
	mutable bool owned {true};

	VortexCarrier() { particles::exit_guard::Watch(); }
	VortexCarrier(const VortexCarrier&) = delete;
	VortexCarrier& operator=(const VortexCarrier&) = delete;
	VortexCarrier(VortexCarrier&&) = delete;
	VortexCarrier& operator=(VortexCarrier&&) = delete;
	/// Each step: the thing out of the map cells is placed where its particle is this step
	void AfterAtomStep(Effect& /*effect*/, Atom& atom) const override
	{
		const auto& matrix = atom.savedDraw[1].matrix;
		ecs::physics::particle_carried_objects::FollowParticle(object, {matrix[9], matrix[10], matrix[11]});
		// the turn it is found available on; once it is not, it is let go for a save
		if (!saved.letGo)
		{
			if (object != entt::null && ecs::physics::particle_carried_objects::IsAvailable(object))
			{
				saved.availableTurn = game_clock::Turn();
			}
			else
			{
				saved.letGo = true;
			}
		}
	}
	~VortexCarrier() override
	{
		if (!owned || particles::exit_guard::Exiting() || !Locator::entitiesRegistry::has_value() || object == entt::null ||
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

using AttractData = vortex_attract::AtomData;

/// What the take finds of the thing's 3D object: drawn as paper (a tree's, unless a physics start cleared it, which
/// the take out of the physics does not give back), fading with the distance (components::NeverFades), and its draw
/// importance (components::DrawImportance)
AttractData::TakenLook TakenLookOf(const ecs::Registry& registry, entt::entity object, bool fromPhysics)
{
	const auto* importance = registry.TryGet<const ecs::components::DrawImportance>(object);
	return {.wasPaper = registry.AllOf<ecs::components::Tree>(object) && !fromPhysics,
	        .wasDisappearing = !registry.AllOf<ecs::components::NeverFades>(object),
	        .importance = importance != nullptr ? importance->importance : 0.0f};
}

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
	[[nodiscard]] RuleData SavedData() const override { return RuleData::VortexAttract; }

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
		// what its 3D object was before the take turns its paper and its fade with the distance off (and its importance
		// to 10, which only the release by a fling gives back)
		const auto look = TakenLookOf(registry, object, record.fromPhysics);
		registry.AssignOrReplace<ecs::components::NeverFades>(object);
		// it leaves its town or flock and is written down for the next land, unless it can't be destroyed
		if (!record.indestructible && record.vortex != entt::null)
		{
			ecs::vortex::HandOff(record.vortex, object);
		}
		auto carrier = std::make_shared<VortexCarrier>();
		carrier->kind = Creator::Kind::Other;
		carrier->className = "RenderParticleGameObject";
		carrier->object = object;
		// the effect's player and the turn it is taken on; the take clears the byte
		carrier->saved = {
		    .object = object, .player = effect.GetPlayer(), .availableTurn = game_clock::Turn(), .letGo = false, .byte = 0};
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
		atom.NoteDataMade(this);
		data.started = true;
		data.takenVelocity = record.velocity;
		data.startHeight = atom.position.y - LandAt({atom.position.x, atom.position.z});
		data.indestructible = record.indestructible;
		data.look = look;
	}

	AttractSettings settings;
};
} // namespace

const vortex_attract::AtomData* vortex_attract::DataOf(const Atom& atom, const Modifier& rule)
{
	if (dynamic_cast<const VortexAttract*>(&rule) == nullptr)
	{
		return nullptr;
	}
	const auto found = atom.modifierData.find(&rule);
	return found != atom.modifierData.end() ? static_cast<const AtomData*>(found->second.get()) : nullptr;
}

std::optional<vortex_attract::CarriedObject> vortex_attract::CarriedOf(const Atom& atom)
{
	const auto* carrier = dynamic_cast<const VortexCarrier*>(atom.creator);
	if (carrier == nullptr)
	{
		return std::nullopt;
	}
	auto carried = carrier->saved;
	carried.object = carrier->object;
	return carried;
}

void vortex_attract::RestoreCarrier(Atom& atom, const CarriedObject& carried)
{
	auto carrier = std::make_shared<VortexCarrier>();
	carrier->kind = Creator::Kind::Other;
	carrier->className = "RenderParticleGameObject";
	carrier->object = carried.object;
	carrier->saved = carried;
	carrier->owned = false;
	atom.creator = carrier.get();
	atom.modifierData.insert_or_assign(k_CarrierKey, std::static_pointer_cast<void>(carrier));
}

void vortex_attract::RestoreData(Atom& atom, const Modifier& rule, const AtomData& data)
{
	atom.modifierData.insert_or_assign(&rule, std::static_pointer_cast<void>(std::make_shared<AtomData>(data)));
	atom.NoteDataMade(&rule);
}

namespace
{
void TakeCarriedOf(const Collection& collection)
{
	for (const auto& atom : collection.atoms)
	{
		if (const auto* carrier = dynamic_cast<const VortexCarrier*>(atom->creator); carrier != nullptr && !carrier->owned)
		{
			carrier->owned = true;
			// carried as the save left it: out of the map cells, in the physics with no body, marked carried
			if (carrier->object != entt::null && !carrier->saved.letGo)
			{
				ecs::physics::particle_carried_objects::RestoreCarried(carrier->object);
			}
		}
		for (const auto& sub : atom->subCollections)
		{
			TakeCarriedOf(*sub);
		}
	}
}
} // namespace

void vortex_attract::TakeRestoredCarriers(const Effect& effect)
{
	for (const auto& root : effect.GetRoots())
	{
		TakeCarriedOf(*root);
	}
}

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
			// it is drawn with the atom's drawn rows, the scale in them, at its drawn place, as the original's particle gives
			// its 3D object; its turn and scale stay as they are, its game place follows the particle's step (AfterAtomStep)
			glm::mat4 model(atom.rotation);
			model[3] = glm::vec4(atom.position, 1.0f);
			registry.AssignOrReplace<ecs::components::ParticleDrawnMatrix>(carrier->object,
			                                                               ecs::components::ParticleDrawnMatrix {model});
			moved = true;
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
