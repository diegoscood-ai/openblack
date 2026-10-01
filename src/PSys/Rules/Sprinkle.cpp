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
// raises it, and AppearanceRuleTumble, the logs' spin. Wiki: docs/bw1-notes/miracles.md, "Comida y madera".

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "Locator.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"

using namespace openblack::psys;

namespace
{
/// UR_HandSprinkle::ModifyAtomCollection 0x6A0220 (DefineProperties 0x6B19D0): one source atom at the gesture position,
/// no higher than 58 m above the land. The first step also starts the hand's raise (HandStateGrain) when this computer's
/// interface casts it. FracToCloseDownOn (+0x38) and KeyPoints (+0x44) are read but not used here: the hand uses its
/// own key points (the same ones).
class HandSprinkle final: public Modifier
{
public:
	explicit HandSprinkle(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , totalTime(object.Float("TotalTime", 1.0f))
	    , heightToRaise(object.Float("HeightToRaise", 0.0f))
	    , angleToRaise(object.Float("AngleToRaise", 0.0f))
	    , initSpeedYHuman(object.Float("InitSpeedYHumanPlayerCasting", 0.0f))
	    , clampHand(object.Bool("ClampHand", false))
	{
	}
	// It makes its one atom on the first step only, so it does not keep a finished effect alive (inf)
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		// PSysManager::GetCurrentGesturePosn 0x673600: PSysProcessInfo +0x0C
		glm::vec3 target = effect.GetProcessInfo().handPos;
		if (openblack::Locator::terrainSystem::has_value())
		{
			const float land = openblack::Locator::terrainSystem::value().GetHeightAt(glm::vec2(target.x, target.z));
			if (target.y - land > 58.0f)
			{
				target.y = land + 58.0f;
			}
		}
		// GlobalToLocal 0x675410: the sprinkle files have no hierarchy, so the point stays global
		if (slot.first)
		{
			// CollectionData +0x20: once per collection
			slot.first = false;
			if (effect.IsMyInterfaceCasting() && heightToRaise != 0.0f && !effect.Closing())
			{
				// fn_005B2F70(ClampHand, TotalTime, HeightToRaise, AngleToRaise, 1): the raise loops
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
		// not ported: this computer's interface casting clears the collection's interpolation flag (bit 1 of +0x38) and
		// gives each atom a DrawOffsetLT (0x6C75A0) that draws it at the hand between turns (here: where the step
		// left it)
		const float dt = effect.GetDt();
		const float extra = effect.IsHumanPlayerCasting() ? initSpeedYHuman : 0.0f;
		for (auto& atom : collection.atoms)
		{
			// vel = (0, (target.y - y) x (1 / dt) (0xD4E0F0) + InitSpeedYHumanPlayerCasting for a human caster, 0); pos = target
			// the max(dt, eps) is a port guard: the original multiplies by [0xD4E0F0] = 1/dt directly
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

/// AppearanceRuleTumble::ModifyAtomCore 0x6A6200 (DefineProperties 0x6ABC10): the atom turns by |v| x TumbleSpeed x dt
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
		const auto& v = atom.velocity;
		float speed = std::sqrt(v.z * v.z + v.y * v.y + v.x * v.x) * tumbleSpeed;
		if (restrict)
		{
			speed = std::clamp(speed, -maxTumbleSpeed, maxTumbleSpeed);
		}
		const float angle = speed * effect.GetDt();
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		// the rows of the atom's matrix (+0x44) are its axes, glm's columns here
		const bool aboutZ = std::abs(v.z) < std::abs(v.x);
		for (int r = 0; r < 3; ++r)
		{
			auto& axis = atom.rotation[r];
			if (aboutZ)
			{
				const float x = axis.x;
				axis.x = c * x + s * axis.y;
				axis.y = c * axis.y - s * x;
			}
			else
			{
				const float y = axis.y;
				axis.y = c * y + s * axis.z;
				axis.z = c * axis.z - s * y;
			}
		}
		return true;
	}
	float tumbleSpeed;
	float maxTumbleSpeed;
	bool restrict;
};
} // namespace

void openblack::psys::RegisterSprinkleRules()
{
	RegisterModifier("UR_HandSprinkle", MakeModifierOf<HandSprinkle>);
	RegisterModifier("AppearanceRuleTumble", MakeModifierOf<Tumble>);
}
