/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandMotion.h"

#include <cmath>

#include "ECS/FastExp.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// The smoothing rate that gets a share of the way in a tenth of a second, worked out in double and rounded once
float RateForATenth(float share)
{
	return static_cast<float>(-10.0 * std::log(1.0 - static_cast<double>(share)));
}

/// A slope this large stands for none: the end is natural
bool IsNatural(std::optional<float> slope)
{
	return !slope.has_value() || *slope > 0.99e30f;
}
} // namespace

glm::vec3 magic::FilterHandVelocity(glm::vec3 smoothed, glm::vec3 raw, float seconds)
{
	const auto rate = RateForATenth(k_HandVelocityShare);
	return smoothed + (raw - smoothed) * (1.0f - gutils::ExpSinglePrecision(-(seconds * rate)));
}

void magic::StepHandSpin(HandSpin& spin, glm::vec3 step, glm::vec3 smoothedVelocity, float seconds)
{
	const auto rate = RateForATenth(k_HandLateralShare);
	const glm::vec3 previous = spin.lastStep;
	spin.lastStep = step;
	const float perSecond = 1.0f / seconds;
	// How far it moved sideways to the way it moved the frame before, across the land
	glm::vec3 normal(previous.z, 0.0f, -previous.x);
	float side = 0.0f;
	if (normal.x != 0.0f || normal.z != 0.0f)
	{
		if (const float length = std::sqrt(normal.z * normal.z + normal.x * normal.x); length != 0.0f)
		{
			const float inv = 1.0f / length;
			normal.x *= inv;
			normal.z *= inv;
			side = (normal.z * step.z + 0.0f * step.y) + normal.x * step.x;
		}
	}
	spin.lateral =
	    (side * perSecond * perSecond - spin.lateral) * (1.0f - gutils::ExpSinglePrecision(-(seconds * rate))) + spin.lateral;
	const float speed = std::sqrt((smoothedVelocity.z * smoothedVelocity.z + smoothedVelocity.y * smoothedVelocity.y) +
	                              smoothedVelocity.x * smoothedVelocity.x);
	spin.spin = speed > k_HandStill ? -(spin.lateral / speed) : 0.0f;
}

CubicSpline::CubicSpline(std::span<const glm::vec2> points, std::optional<float> startSlope, std::optional<float> endSlope)
    : _points(points.begin(), points.end())
    , _bends(points.size(), 0.0f)
{
	const size_t n = _points.size();
	if (n < 2)
	{
		return;
	}
	// The second derivatives at the points, from the tridiagonal system that makes the slopes meet, decomposed going up
	// and solved coming back
	std::vector<float> u(n, 0.0f);
	if (!IsNatural(startSlope))
	{
		const float h = _points[1].x - _points[0].x;
		_bends[0] = -0.5f;
		u[0] = (3.0f / h) * ((_points[1].y - _points[0].y) / h - *startSlope);
	}
	for (size_t i = 1; i + 1 < n; ++i)
	{
		const auto& before = _points[i - 1];
		const auto& at = _points[i];
		const auto& after = _points[i + 1];
		const float sig = (at.x - before.x) / (after.x - before.x);
		const float p = sig * _bends[i - 1] + 2.0f;
		_bends[i] = (sig - 1.0f) / p;
		const float d = (after.y - at.y) / (after.x - at.x) - (at.y - before.y) / (at.x - before.x);
		u[i] = (6.0f * d / (after.x - before.x) - sig * u[i - 1]) / p;
	}
	float qn = 0.0f;
	float un = 0.0f;
	if (!IsNatural(endSlope))
	{
		const float h = _points[n - 1].x - _points[n - 2].x;
		qn = 0.5f;
		un = (3.0f / h) * (*endSlope - (_points[n - 1].y - _points[n - 2].y) / h);
	}
	_bends[n - 1] = (un - qn * u[n - 2]) / (qn * _bends[n - 2] + 1.0f);
	for (size_t k = n - 1; k-- > 0;)
	{
		_bends[k] = _bends[k] * _bends[k + 1] + u[k];
	}
}

float CubicSpline::operator()(float x) const
{
	if (_points.size() < 2)
	{
		return _points.empty() ? 0.0f : _points.front().y;
	}
	// The piece around x, by bisection
	size_t lo = 0;
	size_t hi = _points.size() - 1;
	while (hi - lo > 1)
	{
		const size_t k = (hi + lo) / 2;
		if (_points[k].x > x)
		{
			hi = k;
		}
		else
		{
			lo = k;
		}
	}
	const auto& a = _points[lo];
	const auto& b = _points[hi];
	const float step = b.x - a.x;
	if (step == 0.0f)
	{
		return x;
	}
	const float toB = (b.x - x) / step;
	const float fromA = (x - a.x) / step;
	return toB * a.y + fromA * b.y +
	       ((toB * toB * toB - toB) * _bends[lo] + (fromA * fromA * fromA - fromA) * _bends[hi]) * (step * step) *
	           (1.0f / 6.0f);
}

void magic::StartPour(PourState& pour, const PourSettings& settings)
{
	// The pose it is in carries on from where it was
	pour.active = true;
	pour.settings = settings;
	pour.progress = 0.0f;
}

void magic::StartPour(PourState& pour, const PourSettings& settings, const glm::vec3& hand)
{
	StartPour(pour, settings);
	pour.pinned = hand;
}

void magic::StopPour(PourState& pour)
{
	// What was drawn last turn is kept, so that the hand eases back to rest over the next
	pour.settings.clampHand = false;
	pour.current.raise = 0.0f;
	pour.current.tilt = 0.0f;
	pour.active = false;
}

void magic::StepPour(PourState& pour, float seconds)
{
	pour.previous.raise = pour.current.raise;
	pour.previous.tilt = pour.current.tilt;
	if (!pour.active)
	{
		return;
	}
	pour.progress += seconds / pour.settings.totalTime;
	if (pour.progress > 1.0f)
	{
		if (pour.settings.loops)
		{
			pour.progress = 0.0f;
		}
		else
		{
			StopPour(pour);
		}
	}
	if (!pour.active)
	{
		return;
	}
	// Natural at both ends: with end slopes of 0 the raise would peak at 1.96 instead of 1.61
	static const CubicSpline k_Curve(k_PourKeyPoints);
	const float shape = k_Curve(pour.progress);
	pour.current.raise = shape * pour.settings.heightToRaise;
	pour.current.tilt = shape * pour.settings.angleToRaise;
}

std::optional<glm::vec3> magic::PinnedHand(const PourState& pour)
{
	return pour.settings.clampHand ? std::optional(pour.pinned) : std::nullopt;
}

PourPose magic::PourPoseAt(const PourState& pour, float fraction)
{
	return {.raise = pour.previous.raise + (pour.current.raise - pour.previous.raise) * fraction,
	        .tilt = pour.previous.tilt + (pour.current.tilt - pour.previous.tilt) * fraction,
	        .pinned = PinnedHand(pour)};
}
