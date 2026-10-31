/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_ScaleByCameraDist: the camera-distance scale of the atoms of an effect a player owns. Wiki:
// docs/wiki/physics/particles.md#the-camera-distance-scale.

#include "CameraScale.h"

#include <cmath>

#include "Camera/Camera.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"

using namespace openblack;
using namespace openblack::particles;

bool camera_scale::Runs(const Range& range, bool ownedByPlayer)
{
	// the game's test of the two distances counts a NaN as equal
	const bool apart = range.distMin < range.distMax || range.distMin > range.distMax;
	return apart && ownedByPlayer;
}

float camera_scale::CameraDistance(const glm::vec3& camera, const glm::vec3& point)
{
	const float dx = camera.x - point.x;
	const float dy = camera.y - point.y;
	const float dz = camera.z - point.z;
	return std::sqrt((dz * dz + dy * dy) + dx * dx);
}

float camera_scale::ScaleAtDistance(const Range& range, float distance)
{
	// a NaN distance takes the near scale, as in the game's compare
	if (!(distance >= range.distMin))
	{
		return range.scaleAtMin;
	}
	if (distance > range.distMax)
	{
		return range.scaleAtMax;
	}
	return (distance - range.distMin) / (range.distMax - range.distMin) * (range.scaleAtMax - range.scaleAtMin) +
	       range.scaleAtMin;
}

namespace
{
/// UR_ScaleByCameraDist: each step, every atom of its group takes the rule scale of its distance from the camera
class ScaleByCameraDist final: public Modifier
{
public:
	explicit ScaleByCameraDist(const Object& object)
	    : range {
	          .distMin = object.Float("CameraDistMin", 10.0f),
	          .distMax = object.Float("CameraDistMax", 100.0f),
	          .scaleAtMin = object.Float("ScaleAtCameraDistMin", 0.1f),
	          .scaleAtMax = object.Float("ScaleAtCameraDistMax", 1.0f),
	      }
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// an effect no player owns, or equal distances: it detaches and leaves the atoms as they are
		if (!camera_scale::Runs(range, effect.GetPlayer() >= 0))
		{
			return false;
		}
		if (!Locator::camera::has_value())
		{
			return true; // (port guard) the game always has its camera
		}
		const glm::vec3 camera = Locator::camera::value().GetOrigin();
		for (auto& atom : collection.atoms)
		{
			atom->ruleScale =
			    camera_scale::ScaleAtDistance(range, camera_scale::CameraDistance(camera, effect.GlobalPosition(*atom)));
		}
		return true;
	}
	camera_scale::Range range;
};
} // namespace

void openblack::particles::RegisterCameraScaleRules()
{
	RegisterModifier("UR_ScaleByCameraDist", MakeModifierOf<ScaleByCameraDist>);
}
