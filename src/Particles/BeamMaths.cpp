/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BeamMaths.h"

#include <cmath>

#include <array>

#include "Rules/KeyPoints.h"

using namespace openblack::particles;

float maths::BeamBulge(float t)
{
	const float f = (t + t) - 1.0f;
	return 1.0f - (f * f);
}

std::vector<glm::vec3> maths::BeamKeyPoints(glm::vec3 start, glm::vec3 end, int count, const BeamWiggle& wiggle, float age,
                                            int beam, const std::function<float(float)>& noise,
                                            const std::function<float(glm::vec2)>& landHeight)
{
	std::vector<glm::vec3> keys;
	// (port guard) fewer than two key points crash the game; every file gives six
	if (count < 2)
	{
		return keys;
	}
	keys.reserve(static_cast<size_t>(count));
	const float step = 1.0f / (static_cast<float>(count) - 1.0f);
	const auto offset = static_cast<float>(beam);
	for (int i = 0; i < count; ++i)
	{
		const float t = static_cast<float>(i) * step;
		auto point = start + ((end - start) * t);
		if (i != 0 && i != count - 1)
		{
			const float bulge = BeamBulge(t);
			const float along = t * wiggle.frequency;
			point.x += noise((age * wiggle.speed) + along + offset) * wiggle.amount * bulge;
			point.z += noise((age * wiggle.speed * k_BeamDepthDriftShare) + along + offset) * wiggle.amount * bulge;
			point.y += (noise((age * wiggle.speed * k_BeamHeightDriftShare) + along + offset) + 1.0f) * wiggle.amount * bulge *
			           k_BeamHeightShare;
			const float ground = landHeight({point.x, point.z});
			// Not (at or above): a difference that is not a number clamps too, as in the game
			if (!(point.y - ground >= wiggle.minHeight))
			{
				point.y = ground + wiggle.minHeight;
			}
		}
		keys.push_back(point);
	}
	return keys;
}

float maths::PlasmaDrift(float age, float arcSpeed, float wiggleSpeed)
{
	return (age * arcSpeed) * wiggleSpeed;
}

uint8_t maths::PlasmaAlpha(float age, float life, uint8_t alpha, int32_t maxAlpha)
{
	float share = age / life;
	// Not above 0, or not a number: the start; not below 1: the end
	if (!(share > 0.0f))
	{
		share = 0.0f;
	}
	else if (!(share < 1.0f))
	{
		share = 1.0f;
	}
	const float f = (share + share) - 1.0f;
	const auto scaled = static_cast<float>(static_cast<int32_t>(alpha) * maxAlpha);
	// over 255 as the game has it: its float of 1 / 255
	constexpr float k_OverByte = 0.003921568859368563f;
	return static_cast<uint8_t>(static_cast<int32_t>((scaled * (1.0f - (f * f))) * k_OverByte));
}

std::vector<glm::vec3> maths::PlasmaKeyPoints(const PlasmaArc& arc, glm::vec3 startPush, glm::vec3 endPush,
                                              const PlasmaShape& shape, int count, float drift, int beam,
                                              const std::function<float(float)>& noise)
{
	std::vector<glm::vec3> keys;
	// (port guard) fewer than two key points crash the game; every file gives six
	if (count < 2)
	{
		return keys;
	}
	const glm::vec3 across = arc.start - arc.end;
	const float length = std::sqrt(((across.z * across.z) + (across.y * across.y)) + (across.x * across.x));
	// the tangents pushed by their random points, then made the arc's length times scaleTangents
	const float lengthen = length * shape.scaleTangents;
	const glm::vec3 t0 = ((startPush * shape.randomTangents) + arc.startTangent) * lengthen;
	const glm::vec3 t1 = ((endPush * shape.randomTangents) + arc.endTangent) * lengthen;
	// the cubic's coefficients, each summed in the game's order: the start's term, the end's, then the two tangents'
	const glm::vec3& s = arc.start;
	const glm::vec3& e = arc.end;
	const glm::vec3 a = (((s * 2.0f) + (e * -2.0f)) + (t0 * 1.0f)) + (t1 * 1.0f);
	const glm::vec3 b = (((s * -3.0f) + (e * 3.0f)) + (t0 * -2.0f)) + (t1 * -1.0f);
	const glm::vec3 c = (((s * 0.0f) + (e * 0.0f)) + (t0 * 1.0f)) + (t1 * 0.0f);
	const glm::vec3 d = (((s * 1.0f) + (e * 0.0f)) + (t0 * 0.0f)) + (t1 * 0.0f);
	keys.reserve(static_cast<size_t>(count));
	const float step = 1.0f / (static_cast<float>(count) - 1.0f);
	const auto offset = static_cast<float>(beam);
	for (int i = 0; i < count; ++i)
	{
		const float t = static_cast<float>(i) * step;
		const float t2 = t * t;
		const float t3 = t2 * t;
		auto point = (((a * t3) + (b * t2)) + (c * t)) + d;
		if (i != 0 && i != count - 1)
		{
			const float bulge = BeamBulge(t);
			const float along = t * shape.frequency;
			point.x = ((noise((along + offset) + drift) * shape.amount) * bulge) + point.x;
			point.z = ((noise((along + (drift * k_BeamDepthDriftShare)) + offset) * shape.amount) * bulge) + point.z;
			point.y = ((((noise((along + (drift * k_BeamHeightDriftShare)) + offset) + 1.0f) * shape.amount) * bulge) *
			           k_BeamHeightShare) +
			          point.y;
		}
		keys.push_back(point);
	}
	return keys;
}

std::vector<maths::BeamJoint> maths::BeamJoints(std::span<const glm::vec3> keys, size_t count, float minScale, float maxScale)
{
	std::vector<BeamJoint> joints;
	// (port guard) no curve without two key points (the game crashes before); no joints, nothing to lay
	if (keys.size() < 2 || count == 0)
	{
		return joints;
	}
	// One curve for each axis, through the key points at their shares of the way
	const float keyStep = 1.0f / (static_cast<float>(keys.size()) - 1.0f);
	std::array<std::vector<float>, 3> pairs;
	for (size_t i = 0; i < keys.size(); ++i)
	{
		const float t = static_cast<float>(i) * keyStep;
		for (size_t axis = 0; axis < pairs.size(); ++axis)
		{
			pairs.at(axis).push_back(t);
			pairs.at(axis).push_back(keys[i][static_cast<glm::length_t>(axis)]);
		}
	}
	const std::array<particles::key_points::Spline, 3> curves {
	    particles::key_points::Make(pairs[0]), particles::key_points::Make(pairs[1]), particles::key_points::Make(pairs[2])};
	// As in the game, a lone joint divides by zero: its place on the curve is not a number
	const float step = 1.0f / (static_cast<float>(count) - 1.0f);
	const float range = maxScale - minScale;
	joints.reserve(count);
	// Where two key points share a place on the curve it gives nothing, and the joint stays where the last one was
	glm::vec3 position = keys.back();
	for (size_t j = 0; j < count; ++j)
	{
		const float u = static_cast<float>(j) * step;
		for (size_t axis = 0; axis < curves.size(); ++axis)
		{
			const auto component = static_cast<glm::length_t>(axis);
			position[component] = particles::key_points::EvaluateVector(curves.at(axis), u, position[component]);
		}
		joints.push_back({.position = position, .scale = (BeamBulge(u) * range) + minScale});
	}
	return joints;
}
