/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandGlow.h"

#include <cmath>

#include <algorithm>

#include "3D/ObjectMatrix.h"
#include "Creature/CreatureTurnPose.h"
#include "Graphics/GlowShellLook.h"

using namespace openblack;

namespace
{
/// The game turns its milliseconds into seconds with this
constexpr float k_SecondsPerMillisecond = 0.001f;
} // namespace

void creature_hand_glow::Set(Glows& glows, Place place, float scale, float power)
{
	auto& glow = glows.at(static_cast<size_t>(place));
	// A power that is not a number compares as equal to 0 in the game, so it removes the glow too
	if (power == 0.0f || std::isnan(power))
	{
		glow.reset();
		return;
	}
	if (!glow.has_value())
	{
		glow = Glow {};
	}
	glow->power = power;
	glow->scale = scale;
}

std::optional<creature_hand_glow::Place> creature_hand_glow::PlaceOf(int32_t number)
{
	if (number < 0 || number >= static_cast<int32_t>(k_PlaceCount))
	{
		return std::nullopt;
	}
	return static_cast<Place>(number);
}

bool creature_hand_glow::AnyActive(const Glows& glows)
{
	return std::ranges::any_of(glows, [](const auto& glow) { return glow.has_value() && glow->power > 0.0f; });
}

creature_hand_glow::Hum creature_hand_glow::SetCreedPower(Glows& glows, Place place, float scale, float power)
{
	const bool before = AnyActive(glows);
	Set(glows, place, scale, power);
	const bool after = AnyActive(glows);
	if (before == after)
	{
		return Hum::Unchanged;
	}
	return after ? Hum::Start : Hum::Stop;
}

std::optional<creature_hand_glow::CreedEvent> creature_hand_glow::CreedEventOf(int32_t event)
{
	if (event < k_FirstCreedEvent || event > k_LastCreedEvent)
	{
		return std::nullopt;
	}
	const auto step = static_cast<size_t>(event - k_FirstCreedEvent);
	const float level = k_CreedEventLevels.at(step % k_CreedEventLevels.size());
	// The power is the level added to itself
	return CreedEvent {
	    .place = k_CreedEventPlaces.at(step / k_CreedEventLevels.size()),
	    .power = level + level,
	};
}

float creature_hand_glow::Zoom(const Glow& glow)
{
	return k_ZoomPerPower * glow.power;
}

void creature_hand_glow::Turn(Glow& glow, uint32_t gameMilliseconds)
{
	const float scaled = static_cast<float>(gameMilliseconds) * k_TurnSpeed;
	const float radians = scaled * k_SecondsPerMillisecond;
	glow.turn = radians + glow.turn;
}

glm::mat3 creature_hand_glow::CirclingAxes(float turn)
{
	// The sine and cosine are worked at a wider precision, then each kept as a float
	const auto c = static_cast<float>(std::cos(static_cast<double>(turn)));
	const auto s = static_cast<float>(std::sin(static_cast<double>(turn)));
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c)};
}

glm::vec3 creature_hand_glow::CirclingPoint(glm::vec3 bodyOrigin, float size)
{
	const float sized = k_CirclingHeight * size;
	const float height = sized * k_HeightPerSize;
	return {bodyOrigin.x, height + bodyOrigin.y, bodyOrigin.z};
}

glm::mat4 creature_hand_glow::ModelMatrix(const glm::mat3& axes, glm::vec3 point, float bodyScale, float glowScale)
{
	const float scale = k_BodyScale * bodyScale;
	glm::mat4 model(1.0f);
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			float value = axes[i][j] * scale;
			if (i == 2)
			{
				value = value * k_BackAxis;
			}
			model[i][j] = value * glowScale;
		}
	}
	model[3] = glm::vec4(point, 1.0f);
	return model;
}

std::optional<creature_hand_glow::Placed> creature_hand_glow::PlaceOn(Glow& glow, Place place, const Body& body,
                                                                      uint32_t realMilliseconds, uint32_t gameMilliseconds)
{
	glm::mat3 axes(1.0f);
	glm::vec3 point(0.0f);
	if (place == Place::Circling)
	{
		Turn(glow, gameMilliseconds);
		axes = CirclingAxes(glow.turn);
		point = CirclingPoint(body.origin, body.size);
	}
	else
	{
		auto bone = place == Place::Belly ? body.belly : body.rightHand;
		if (place == Place::LeftHand)
		{
			if (bone >= body.mirror.size())
			{
				return std::nullopt;
			}
			bone = body.mirror[bone];
		}
		if (bone >= body.bones.size())
		{
			return std::nullopt;
		}
		const auto& world = body.bones[bone];
		axes = glm::mat3(world);
		affine::NormaliseRows(axes);
		point = glm::vec3(world[3]);
		// A hand glow is at the middle of the ends of the bones under the hand, its fingertips
		if (place != Place::Belly)
		{
			const auto ends = creature_turn_pose::LeafMean(body.parents, body.bones, bone);
			if (!ends.has_value())
			{
				return std::nullopt;
			}
			point = *ends;
		}
	}
	// The shell reaches out by the power, then its texture slides on
	const float zoom = Zoom(glow);
	glow.slide = glow_shell::Slide(glow.slide, k_SlideSpeed, realMilliseconds);
	return Placed {.model = ModelMatrix(axes, point, body.scale, glow.scale),
	               .scale = k_BodyScale * body.scale * glow.scale,
	               .zoom = zoom,
	               .slide = glow.slide};
}
