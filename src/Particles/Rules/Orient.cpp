/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The orient-with-velocity rule (the flames of the fireball in the hand): the sprite's roll follows its smoothed velocity as
// the camera sees it. Wiki: docs/wiki/magic/lightning-explosion.md#lightning-explosion-and-missing-psys-classes.

#include "Particles/Rules/Orient.h"

#include <cmath>

#include <memory>

#include "3D/Billboard.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
using OrientData = orient_velocity::AtomData;

/// The orient-with-velocity rule, per atom (properties SmoothFactor, ProportionDefault)
class OrientSpriteWithVelocity final: public Modifier
{
public:
	explicit OrientSpriteWithVelocity(const Object& object)
	    : smoothFactor(object.Float("SmoothFactor", 0.0f))
	    , proportionDefault(object.Float("ProportionDefault", 0.0f))
	{
	}
	[[nodiscard]] RuleData SavedData() const override { return RuleData::OrientSpriteWithVelocity; }

	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = AtomDataOf<OrientData>(atom, this);
		atom.NoteDataMade(this);
		if (data.first)
		{
			// the first time: the velocity as it is, rate = -10 ln(1 - SmoothFactor)
			data.first = false;
			data.velocity = atom.velocity;
			data.rate = std::log(1.0f - smoothFactor) * -10.0f;
		}
		// v += (velocity - v) (1 - e^(-dt rate)), dt the step
		data.velocity += (atom.velocity - data.velocity) * (1.0f - std::exp(-effect.GetDt() * data.rate));
		// u = -v + (0, ProportionDefault, 0), in the camera's frame (the world-to-camera rotation: x = u . right,
		// y = u . up). (inferred) the vector u: the original's code for it was not read
		const glm::vec3 u(-data.velocity.x, -data.velocity.y + proportionDefault, -data.velocity.z);
		if (!Locator::camera::has_value())
		{
			return true;
		}
		const auto& camera = Locator::camera::value();
		// the Y angle atan2(-y, x) + pi / 2, billboard::ScreenVelocity
		atom.rotation = affine::AngleY(graphics::billboard::ScreenVelocity(u, camera.GetRight(), camera.GetUp()));
		return true;
	}
	float smoothFactor, proportionDefault;
};
} // namespace

const orient_velocity::AtomData* orient_velocity::DataOf(const Atom& atom, const Modifier& rule)
{
	if (dynamic_cast<const OrientSpriteWithVelocity*>(&rule) == nullptr)
	{
		return nullptr;
	}
	const auto found = atom.modifierData.find(&rule);
	return found != atom.modifierData.end() ? static_cast<const AtomData*>(found->second.get()) : nullptr;
}

bool orient_velocity::RestoreData(Atom& atom, const Modifier& rule, const AtomData& data)
{
	if (dynamic_cast<const OrientSpriteWithVelocity*>(&rule) == nullptr)
	{
		return false;
	}
	atom.modifierData.insert_or_assign(&rule, std::static_pointer_cast<void>(std::make_shared<AtomData>(data)));
	atom.NoteDataMade(&rule);
	return true;
}

void openblack::particles::RegisterOrientRules()
{
	RegisterModifier("UR_OrientSpriteWithVelocity", MakeModifierOf<OrientSpriteWithVelocity>);
}
