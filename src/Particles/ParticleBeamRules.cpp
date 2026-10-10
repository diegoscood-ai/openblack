/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_SimpleBeam: wiggling ribbons of light from the effect's origin to each object or point it is given. Each target
// the effect is given while it is not closing down becomes an atom of its own carrying a few beams, each a collection of
// ribbon joints. Every step each beam is laid afresh from the origin, wherever it is now, to its target, an object being
// followed for as long as it is available: a few key points along the way are pushed aside by noise drifting along the
// beam, most at its middle, and kept above the land, then the joints are spread evenly along a smooth curve through
// them, thickest at the middle. A chain's texture slides along it. Wiki: docs/bw1-notes/particles.md, "The simple beam".

#include <cstdint>

#include <algorithm>
#include <bit>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "3D/LandMorph.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"
#include "Particles/BeamMaths.h"
#include "Particles/Noise.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The bits of the particle allocator's fill, read as a float (-431602080)
constexpr uint32_t k_UnsetTarget = 0xCDCDCDCDu;

/// What a beam's atom keeps: the object it follows (none for a point) and the two ends of its beams
struct BeamTarget
{
	entt::entity object {entt::null};
	glm::vec3 target {0.0f};
	glm::vec3 origin {0.0f};
};

/// A beam's number among its atom's beams, the newest 0, so that no two wiggle alike. A beam that was not numbered is 0
struct BeamNumber
{
	int number {0};
};

/// An available object with a place
bool Available(entt::entity object)
{
	return object != entt::null && Locator::entitiesRegistry::has_value() && ecs::IsAvailable(object) &&
	       Locator::entitiesRegistry::value().AllOf<ecs::components::Transform>(object);
}

/// Where a beam on an object ends: half the object's height above its place. (pending) The game starts from the object's
/// map coordinates, and ends a beam on a creature at one of its bones
glm::vec3 BeamEnd(entt::entity object)
{
	auto end = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object).position;
	end.y = (ecs::object::GetHeight(object) * 0.5f) + end.y;
	return end;
}

/// UR_SimpleBeam. Flag 2 without 4: it makes no atoms of its own accord, but the effect waits for its targets until it
/// closes down.
class SimpleBeam final: public Modifier
{
public:
	// The game makes the rule zeroed, so MaxJointsPerFork and MinHeight are 0 when a file does not give them (every file
	// does). The properties' ranges are only the editor's: the values are kept as the file gives them
	explicit SimpleBeam(const Object& object)
	    : creator(object.String("PCreator"))
	    , joints(object.Int("MaxJointsPerFork", 0))
	    , keyPoints(object.Int("NumSplinePoints", 5))
	    , beams(object.Int("NumBeams", 3))
	    , beamGroup(object.Int("BeamGroup", -1))
	    , wiggle(particles::maths::BeamWiggle {
	          .frequency = object.Float("WiggleFreq", 4.0f),
	          .speed = object.Float("WiggleSpeed", 1.0f),
	          .amount = object.Float("RandomFrac", 0.1f),
	          .minHeight = object.Float("MinHeight", 0.0f),
	      })
	    , speedV(object.Float("SpeedV", 1.0f))
	    , forkScaleMin(object.Float("ForkScaleMin", 1.0f))
	    , forkScaleMax(object.Float("ForkScaleMax", 1.0f))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// Where the beams start this step: the parent atom, or the effect's origin
		const auto origin = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
		if (!effect.Closing())
		{
			while (effect.GetTargets().size() + effect.TargetPointCount() > 0)
			{
				Begin(effect, collection, origin);
			}
		}
		// The original's lists grow at their heads, so it walks the newest atom and the newest beam first
		const float magnitude = effect.GetMagnitude();
		for (auto& atom : std::ranges::reverse_view(collection.atoms))
		{
			const auto found = atom->modifierData.find(this);
			// (port guard) an atom without the beam's data crashes the game; only carriers are in this group
			if (found == atom->modifierData.end())
			{
				continue;
			}
			auto& data = *std::static_pointer_cast<BeamTarget>(found->second);
			Follow(data);
			data.origin = origin;
			for (auto& ribbon : std::ranges::reverse_view(atom->subCollections))
			{
				Lay(effect, *ribbon, data, magnitude);
			}
		}
		return true;
	}

private:
	/// A new atom for the next target, objects before points, carrying its beams and their joints
	void Begin(Effect& effect, Collection& collection, const glm::vec3& origin) const
	{
		// (port guard) no sub-collections for a negative NumBeams
		const std::vector<int> groups(static_cast<size_t>(std::max(0, beams)), beamGroup);
		// The atom has no creator: it is never drawn, it only carries the beams
		auto& atom = effect.NewAtom(collection, nullptr, groups);
		auto& data = AtomDataOf<BeamTarget>(atom, this);
		data.origin = origin;
		if (!effect.GetTargets().empty())
		{
			// An object's end is set when it is followed, the same step. Until then it keeps what the game's particle
			// allocator fills every block with, 0xCDCDCDCD, which an object unavailable the step it is given keeps
			data.target = glm::vec3(std::bit_cast<float>(k_UnsetTarget));
			data.object = effect.TakeTarget();
		}
		else
		{
			effect.TakeTargetPoint(data.target);
		}
		atom.position = origin;
		// (port guard) the game calls through a missing creator and crashes; every file gives one. Here the joints have none
		const auto* jointCreator = effect.FindCreator(creator);
		const bool chain = jointCreator != nullptr && jointCreator->kind == Creator::Kind::Chain;
		int number = 0;
		for (auto& ribbon : std::ranges::reverse_view(atom.subCollections))
		{
			CollectionDataOf<BeamNumber>(*ribbon, this).number = number++;
			if (chain)
			{
				ribbon->chainScrollRate = speedV;
			}
			for (int j = 0; j < joints; ++j)
			{
				effect.NewAtom(*ribbon, jointCreator, {});
			}
		}
	}

	/// An object's beam ends at its middle for as long as it is available, and stays where it last was after
	static void Follow(BeamTarget& data)
	{
		if (data.object == entt::null)
		{
			return;
		}
		if (Available(data.object))
		{
			data.target = BeamEnd(data.object);
		}
		else
		{
			data.object = entt::null;
		}
	}

	/// A beam laid from the origin to the target, its joints from the newest at the origin to the first made at the
	/// target
	void Lay(const Effect& effect, Collection& ribbon, const BeamTarget& data, float magnitude) const
	{
		const int number = CollectionDataOf<BeamNumber>(ribbon, this).number;
		const auto keys =
		    particles::maths::BeamKeyPoints(data.origin, data.target, keyPoints, wiggle, effect.CollectionAge(ribbon), number,
		                                    noise::SignedValueNoise, land_morph::CurrentAltitude());
		const auto laid =
		    particles::maths::BeamJoints(keys, ribbon.atoms.size(), magnitude * forkScaleMin, magnitude * forkScaleMax);
		for (size_t j = 0; j < laid.size(); ++j)
		{
			auto& joint = *ribbon.atoms[ribbon.atoms.size() - 1 - j];
			joint.position = laid[j].position;
			joint.ruleScale = laid[j].scale;
		}
	}

	std::string creator;
	int joints;
	int keyPoints;
	int beams;
	int beamGroup;
	particles::maths::BeamWiggle wiggle;
	/// How fast a chain's texture slides along it
	float speedV;
	float forkScaleMin;
	float forkScaleMax;
};
} // namespace

void openblack::psys::RegisterBeamRules()
{
	RegisterModifier("UR_SimpleBeam", MakeModifierOf<SimpleBeam>);
}
