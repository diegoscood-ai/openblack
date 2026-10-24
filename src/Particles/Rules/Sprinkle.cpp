/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The sprinkle miracles' rules (SF_Food, SF_Wood, SF_Water): UR_HandSprinkle, the source atom that follows the hand and
// raises it, and AppearanceRuleTumble, the logs' spin. Wiki: docs/wiki/magic/food-wood-water-heal.md#food-and-wood.

#include "Particles/Rules/Sprinkle.h"

#include <cmath>

#include <algorithm>
#include <optional>

#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"

using namespace openblack::particles;

namespace
{
/// UR_HandSprinkle: one source atom at the gesture position,
/// no higher than 58 m above the land. The first step also starts the hand's raise (HandStateGrain) when this computer's
/// interface casts it. FracToCloseDownOn and KeyPoints are read but not used here: the hand uses its
/// own key points (the same ones).
class HandSprinkle final: public Modifier
{
public:
	explicit HandSprinkle(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , totalTime(object.Float("TotalTime", 1.0f))
	    , heightToRaise(object.Float("HeightToRaise", 0.0f))
	    , angleToRaise(object.Float("AngleToRaise", 0.0f))
	    , initSpeedYHuman(object.Float("InitSpeedYHumanPlayerCasting", 0.0f))
	    , clampHand(object.Bool("ClampHand", false))
	{
	}
	// It makes its one atom on the first step only, so it does not keep a finished effect alive (inferred)
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		// the current gesture position of the process info
		glm::vec3 target = effect.GetProcessInfo().handPos;
		if (openblack::Locator::terrainSystem::has_value())
		{
			const float land = openblack::Locator::terrainSystem::value().GetHeightAt(glm::vec2(target.x, target.z));
			if (target.y - land > 58.0f)
			{
				target.y = land + 58.0f;
			}
		}
		// to local: the sprinkle files have no hierarchy, so the point stays global
		if (slot.first)
		{
			// once per collection
			slot.first = false;
			if (effect.IsMyInterfaceCasting() && heightToRaise != 0.0f && !effect.Closing())
			{
				// the hand's raise (ClampHand, TotalTime, HeightToRaise, AngleToRaise, 1): it loops
				openblack::ecs::systems::hand_grain::Start(clampHand, totalTime, heightToRaise, angleToRaise, true);
			}
			const auto* source = effect.FindCreator(creator);
			if (source == nullptr)
			{
				return false; // no PCreator: the rule detaches
			}
			auto& atom = effect.NewAtom(collection, source, nextGroups);
			atom.position = target;
		}
		// not ported: this computer's interface casting clears the collection's interpolation flag and gives each atom
		// a draw offset that draws it at the hand between turns (here: where the step left it)
		const float dt = effect.GetDt();
		const float extra = effect.IsHumanPlayerCasting() ? initSpeedYHuman : 0.0f;
		for (auto& atom : collection.atoms)
		{
			// vel = (0, (target.y - y) x (1 / dt) + InitSpeedYHumanPlayerCasting for a human caster, 0); pos = target
			// the max(dt, eps) is a port guard: the original multiplies by 1/dt directly
			atom->velocity = glm::vec3(0.0f, (target.y - atom->position.y) / std::max(dt, 1e-4f) + extra, 0.0f);
			atom->position = target;
		}
		return true;
	}
	std::string creator;
	std::vector<int> nextGroups;
	float totalTime;
	float heightToRaise;
	float angleToRaise;
	float initSpeedYHuman;
	bool clampHand;
};

/// AppearanceRuleTumble: the atom turns by |v| x TumbleSpeed x dt
/// (at most MaxTumbleSpeed with RestrictMaxRotation) about z when it moves more along x than along z, else about x
class Tumble final: public Modifier
{
public:
	explicit Tumble(const Object& object)
	    : tumbleSpeed(object.Float("TumbleSpeed", 0.0f))
	    , maxTumbleSpeed(object.Float("MaxTumbleSpeed", 0.0f))
	    , restrict(object.Bool("RestrictMaxRotation", false))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		const float angle = sprinkle::TumbleAngle(atom.velocity, tumbleSpeed,
		                                          restrict ? std::optional(maxTumbleSpeed) : std::nullopt, effect.GetDt());
		// every row's (x, y) about Z when |vz| < |vx|, else its (y, z) about X
		openblack::affine::TurnRows(atom.rotation, sprinkle::TumblesAboutZ(atom.velocity) ? 2 : 0, angle);
		return true;
	}
	float tumbleSpeed;
	float maxTumbleSpeed;
	bool restrict;
};
} // namespace

float openblack::particles::sprinkle::TumbleAngle(const glm::vec3& velocity, float tumbleSpeed,
                                                  std::optional<float> maxTumbleSpeed, float dt)
{
	const auto& v = velocity;
	float speed = std::sqrt(v.z * v.z + v.y * v.y + v.x * v.x) * tumbleSpeed;
	if (maxTumbleSpeed.has_value())
	{
		speed = std::clamp(speed, -*maxTumbleSpeed, *maxTumbleSpeed);
	}
	return speed * dt;
}

bool openblack::particles::sprinkle::TumblesAboutZ(const glm::vec3& velocity)
{
	return std::abs(velocity.z) < std::abs(velocity.x);
}

void openblack::particles::RegisterSprinkleRules()
{
	RegisterModifier("UR_HandSprinkle", MakeModifierOf<HandSprinkle>);
	RegisterModifier("AppearanceRuleTumble", MakeModifierOf<Tumble>);
}
