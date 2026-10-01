/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_OrientSpriteWithVelocity (the flames of SF_FireBallInHand): the sprite's roll follows its smoothed velocity as the
// camera sees it. Wiki: docs/bw1-notes/miracles.md, "Explosión de rayo y clases de PSys que faltaban (M6b)" (las clases que faltaban).

#include <cmath>

#include <memory>
#include <numbers>

#include <glm/geometric.hpp>

#include "Camera/Camera.h"
#include "Locator.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// AtomData of UR_OrientSpriteWithVelocity (0x38 bytes, ctor 0x560DC0): +0x20 first, +0x24 the smoothed velocity,
/// +0x30 its rate
struct OrientData
{
	bool first {true};
	glm::vec3 velocity {0.0f};
	float rate {0.0f};
};

/// AtomCore::SetAngleY 0x674360: rows (c, 0, s), (0, 1, 0), (-s, 0, c), the rows being the columns here
glm::mat3 AngleY(float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c)};
}

/// UR_OrientSpriteWithVelocity::ModifyAtomCore 0x69A790 (DefineProperties 0x6AC610: +0x20 SmoothFactor,
/// +0x24 ProportionDefault)
class OrientSpriteWithVelocity final: public Modifier
{
public:
	explicit OrientSpriteWithVelocity(const Object& object)
	    : smoothFactor(object.Float("SmoothFactor", 0.0f))
	    , proportionDefault(object.Float("ProportionDefault", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& slot = atom.modifierData[this];
		if (slot == nullptr)
		{
			slot = std::make_shared<OrientData>();
		}
		auto& data = *std::static_pointer_cast<OrientData>(slot);
		if (data.first)
		{
			// the first time: the velocity as it is, rate = -10 ln(1 - SmoothFactor) (fldln2 / fyl2x, [0x8C7670] = -10)
			data.first = false;
			data.velocity = atom.velocity;
			data.rate = std::log(1.0f - smoothFactor) * -10.0f;
		}
		// v += (velocity - v) (1 - e^(-dt rate)) ([0xD4E0EC] = dt; f2xm1 / fscale for the exponential)
		data.velocity += (atom.velocity - data.velocity) * (1.0f - std::exp(-effect.GetDt() * data.rate));
		// u = -v + (0, ProportionDefault, 0), in the camera's frame (the LHMatrix at 0xEA1D28 x u: its columns 0 and 1).
		// (inferido) 0xEA1D28 taken as the world-to-camera rotation: x = u . right, y = u . up
		const glm::vec3 u(-data.velocity.x, -data.velocity.y + proportionDefault, -data.velocity.z);
		if (!Locator::camera::has_value())
		{
			return true;
		}
		const auto& camera = Locator::camera::value();
		const float x = glm::dot(u, camera.GetRight());
		const float y = glm::dot(u, camera.GetUp());
		// SetAngleY(atan2(-y, x) + pi / 2) ([0x8C78D8])
		atom.rotation = AngleY(std::atan2(-y, x) + std::numbers::pi_v<float> * 0.5f);
		return true;
	}
	float smoothFactor, proportionDefault;
};
} // namespace

void openblack::psys::RegisterOrientRules()
{
	RegisterModifier("UR_OrientSpriteWithVelocity", MakeModifierOf<OrientSpriteWithVelocity>);
}
