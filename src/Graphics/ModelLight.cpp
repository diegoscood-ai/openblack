/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModelLight.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "3D/LandMorph.h"
#include "ArgbColour.h"
#include "ECS/Systems/RenderFrameSystemInterface.h"
#include "Locator.h"

using namespace openblack;

namespace
{
constexpr float k_FocusLift = 10.0f; ///< the focus's least height over the land
/// Below it, on a sky type counted from 0 at night, the night light: the same as above 1.5 on the game's sky type
constexpr float k_NightSkyType = 0.5f;
constexpr float k_LightDistance = 3.0f; ///< from the focus towards the camera
constexpr float k_ColourSteps = 255.0f;

/// What the renderer keeps from frame to frame (Locator::renderFrameSystem)
openblack::ecs::systems::RenderFrameSystemInterface& RenderFrame()
{
	return openblack::Locator::renderFrameSystem::value();
}
} // namespace

glm::vec3 model_light::Light()
{
	return RenderFrame().GetModelLight();
}

void model_light::SetLight(const glm::vec3& position)
{
	RenderFrame().SetModelLight(position);
}

int model_light::Ambient()
{
	return RenderFrame().GetModelAmbient();
}

void model_light::SetAmbient(int ambient)
{
	RenderFrame().SetModelAmbient(ambient);
}

glm::vec3 model_light::FrameLight(glm::vec3 hand, float groundUnderHand, const glm::vec3& camera, float skyType, bool inTemple)
{
	// "not less" than 0.5 (and the unordered case) takes the day branch
	if (inTemple || !(skyType < k_NightSkyType))
	{
		return k_Sun;
	}
	// the focus never goes below the land under it plus 10
	hand.y = std::max(hand.y, groundUnderHand + k_FocusLift);
	// focus + 3 normalize(camera - focus), with only the exactly null vector left unnormalised
	const auto toCamera = camera - hand;
	const auto lengthSquared = glm::dot(toCamera, toCamera);
	const auto direction = lengthSquared > 0.0f ? toCamera / std::sqrt(lengthSquared) : toCamera;
	return hand + direction * k_LightDistance;
}

void model_light::UpdateFrameLight(glm::vec3 focus, const glm::vec3& cameraPosition, float skyType)
{
	const float ground = land_morph::CurrentAltitude()(glm::vec2(focus.x, focus.z));
	// The game's sky type counts from 0 by day to 2 at night; FrameLight's from 0 at night. 2 - s is exact from 1 up
	// and over 1 below it, so "less than 0.5" there is "greater than 1.5" here, and NaN stays NaN (the day branch)
	SetLight(FrameLight(focus, ground, cameraPosition, 2.0f - skyType, false));
}

glm::vec3 model_light::LightInMeshSpace(const glm::mat4& model)
{
	// the 3x3 inverse by adjugate / determinant and the translation -t A^-1, which is glm::affineInverse, then the light
	// as a point through it, normalised
	const auto local = glm::vec3(glm::affineInverse(model) * glm::vec4(Light(), 1.0f));
	const auto lengthSquared = glm::dot(local, local);
	return lengthSquared > 0.0f ? local / std::sqrt(lengthSquared) : local;
}

glm::vec4 model_light::Uniform()
{
	return {Light(), static_cast<float>(Ambient())};
}

int model_light::Intensity(float dot, bool truncate)
{
	const auto lit = k_ColourSteps * dot;
	// to the nearest, halves to even (the FPU's default mode, which std::lrint keeps) or, in the truncating variant,
	// towards zero
	return truncate ? static_cast<int>(lit) : static_cast<int>(std::lrint(lit));
}

int model_light::Factor(int intensity, int ambient)
{
	return intensity < 0 ? ambient : ambient + (((0xFF - ambient) * intensity) >> 8);
}

uint32_t model_light::Apply(uint32_t colour, int intensity, int ambient)
{
	// one multiply per channel and the bits of the byte kept, so each channel is (c f) >> 8 truncated
	return argb_colour::ScaleRgbShift8KeepAlpha(colour, static_cast<uint32_t>(Factor(intensity, ambient)));
}

model_light::TwoSidedColours model_light::TwoSided(uint32_t colour, float dot, int ambient)
{
	// rounded once, negated for the back
	const int intensity = Intensity(dot);
	return {Apply(colour, intensity, ambient), Apply(colour, -intensity, ambient)};
}
