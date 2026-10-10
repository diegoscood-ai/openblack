/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// ER_GlintsOnTarget: glints that sparkle on the points of an object's model, as on a script highlight's scroll or a
// frozen-creature phial. Each step takes one of the objects the effect was given and makes it a parent atom, whose group
// holds its glints. The parents are walked newest first: one whose object has gone goes with its glints, unstepped;
// the others get new glints at the most allowed over the age at which they vanish, each on a point of the model picked
// once, and every glint follows its point as the model moves, pulsing as it grows and shrinks, at a fixed alpha. Wiki:
// docs/bw1-notes/particles.md, "The glints on a target".

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/GlintTargetsInterface.h"
#include "Locator.h"
#include "Particles/GlintMaths.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// What the glints read of their objects (Locator::glintTargets)
const ecs::systems::GlintTargetsInterface& Targets()
{
	if (!Locator::glintTargets::has_value())
	{
		std::fputs("ER_GlintsOnTarget: no glint targets in the locator (Locator::glintTargets)\n", stderr);
		std::abort();
	}
	return Locator::glintTargets::value();
}

/// A parent atom's data: its object, the glints due so far (fractional) and the glints made
struct ParentData
{
	entt::entity object {entt::null};
	float due {0.0f};
	int32_t made {0};
};

/// A glint's data: the point of the model it sits on
struct GlintData
{
	uint32_t point {0};
};

/// ER_GlintsOnTarget. Flags 7: a creator
class GlintsOnTarget final: public Modifier
{
public:
	explicit GlintsOnTarget(const Object& object)
	    : creator(object.String("PCreator"))
	    , glintCreator(object.String("GlintCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , maxAtoms(object.Int("MaxAtoms", 20))
	    , maxAlpha(object.Int("MaxAlpha", 40))
	    , glintGroup(object.Int("GlintGroup", -1))
	    , pulseMagnitude(object.Float("PulseMagnitude", 1.0f))
	    , pulseSpeed(object.Float("PulseSpeed", 1.0f))
	    , ageMaxSize(object.Float("AtomAgeMaxSize", 0.25f))
	    , ageZeroSize(object.Float("AtomAgeZeroSize", 2.0f))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* parentCreator = effect.FindCreator(creator);
		if (parentCreator == nullptr)
		{
			return false;
		}
		// One object given a step becomes a parent atom, holding a group of glints
		if (!effect.GetTargets().empty())
		{
			if (const auto object = effect.TakeTarget(); object != entt::null)
			{
				auto& parent = effect.NewAtom(collection, parentCreator, {glintGroup});
				AtomDataOf<ParentData>(parent, this).object = object;
			}
		}
		// Newest first. A parent whose object has gone (or that the rule did not make) goes with its glints
		for (size_t i = collection.atoms.size(); i-- > 0;)
		{
			auto& parent = *collection.atoms[i];
			auto& data = AtomDataOf<ParentData>(parent, this);
			if (data.object == entt::null || !Targets().ObjectPosition(data.object).has_value())
			{
				data.object = entt::null;
				effect.NotifyAtomRemoved(parent);
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			if (!parent.subCollections.empty())
			{
				Glints(effect, data, *parent.subCollections.front());
			}
		}
		return true;
	}

private:
	void Glints(Effect& effect, ParentData& parent, Collection& glints) const
	{
		const auto* made = effect.FindCreator(glintCreator);
		if (made == nullptr)
		{
			return;
		}
		const float rate = particles::maths::GlintRate(maxAtoms, ageZeroSize);
		if (!(rate > 0.0f))
		{
			return;
		}
		const auto& targets = Targets();
		// The glints due grow before the points are counted, so an object without points still runs them up
		parent.due += effect.GetDt() * rate;
		const auto points = targets.TargetPointCount(parent.object);
		// "made < due" also holds when due is not a number
		if (points > 0 && !(static_cast<float>(parent.made) >= parent.due))
		{
			do
			{
				if (static_cast<int64_t>(glints.atoms.size()) >= maxAtoms)
				{
					break;
				}
				++parent.made;
				auto& glint = effect.NewAtom(glints, made, nextGroups);
				glint.baseScale = targets.TargetScale(parent.object) * glint.baseScale;
				AtomDataOf<GlintData>(glint, this).point = static_cast<uint32_t>(effect.Rand(static_cast<int32_t>(points)));
			} while (!(static_cast<float>(parent.made) >= parent.due));
		}
		// Newest first; the point is in the world, as no glint file has a hierarchy
		for (size_t i = glints.atoms.size(); i-- > 0;)
		{
			auto& glint = *glints.atoms[i];
			if (const auto point = targets.TargetPoint(parent.object, AtomDataOf<GlintData>(glint, this).point))
			{
				glint.position = *point;
			}
			const float age = effect.AtomAge(glint);
			const float pulse = particles::maths::GlintPulse(age, pulseSpeed, pulseMagnitude);
			glint.ruleScale = particles::maths::GlintSize(age, ageMaxSize, ageZeroSize) * pulse;
			glint.colour[3] = static_cast<uint8_t>(maxAlpha);
			if (age > ageZeroSize)
			{
				effect.NotifyAtomRemoved(glint);
				glints.atoms.erase(glints.atoms.begin() + static_cast<std::ptrdiff_t>(i));
			}
		}
	}

	std::string creator;
	std::string glintCreator;
	std::vector<int> nextGroups;
	int maxAtoms;
	int maxAlpha;
	int glintGroup;
	float pulseMagnitude;
	float pulseSpeed;
	float ageMaxSize;
	float ageZeroSize;
};
} // namespace

void openblack::psys::RegisterGlintRules()
{
	RegisterModifier("ER_GlintsOnTarget", MakeModifierOf<GlintsOnTarget>);
}
