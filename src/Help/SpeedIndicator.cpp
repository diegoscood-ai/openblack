/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpeedIndicator.h"

#include <cmath>

#include <glm/gtc/constants.hpp>

#include "3D/Billboard.h"
#include "Camera/ScreenPoint.h"

// The game works these out in single precision, but keeps its sines and cosines at full precision until each product
// with them is rounded: here they are doubles, each product rounded back to a float.

namespace openblack::help
{
namespace
{
constexpr float k_Pi = glm::pi<float>();
constexpr float k_HalfPi = glm::half_pi<float>();
constexpr float k_TwoPi = glm::two_pi<float>();
constexpr float k_Cycle = 4.0f * k_Pi;
/// The depth the pitch is measured at
constexpr float k_PitchDepth = 10.0f;

/// A ray a unit long, unless it is all 0
glm::vec3 Unit(glm::vec3 v, bool zFirst)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return v;
	}
	const float lengthSquared = zFirst ? (v.z * v.z + v.y * v.y) + v.x * v.x : (v.x * v.x + v.y * v.y) + v.z * v.z;
	const float scale = 1.0f / std::sqrt(lengthSquared);
	return {scale * v.x, v.y * scale, v.z * scale};
}

/// A float times a full precision factor, rounded back to a float
float Times(float value, double factor)
{
	return static_cast<float>(static_cast<double>(value) * factor);
}

/// Each axis turned about x by the angle whose cosine and sine are given: (y, z) -> (y cos + z sin, z cos - y sin)
void TurnAboutX(std::array<glm::vec3, 3>& axes, double cosine, double sine)
{
	for (auto& axis : axes)
	{
		const float y = Times(axis.z, sine) + Times(axis.y, cosine);
		const float z = Times(axis.z, cosine) - Times(axis.y, sine);
		axis.y = y;
		axis.z = z;
	}
}

/// A vector of the camera's space in the world, along the camera's right, up and forward
glm::vec3 InWorld(const graphics::billboard::CameraFrame& camera, glm::vec3 v)
{
	return (camera.forward * v.z + camera.up * v.y) + camera.right * v.x;
}
} // namespace

SpeedIndicatorState StateOf(bool paused, float speed)
{
	if (paused)
	{
		return SpeedIndicatorState::Paused;
	}
	if (speed < 1.0f)
	{
		return SpeedIndicatorState::Slow;
	}
	return speed > 1.0f ? SpeedIndicatorState::Fast : SpeedIndicatorState::None;
}

float EasedFade(float fade)
{
	// Not a number counts as below 0
	if (!(fade >= 0.0f))
	{
		return 0.0f;
	}
	if (fade > 1.0f)
	{
		return 1.0f;
	}
	const auto cosine = std::cos(static_cast<double>(fade * k_Pi));
	return static_cast<float>(1.0 - cosine) * 0.5f;
}

std::optional<SpeedIndicator::Frame> SpeedIndicator::Update(SpeedIndicatorState state, uint32_t realMs,
                                                            const SpeedIndicatorGates& gates)
{
	// The milliseconds are read as a signed number
	const auto ms = static_cast<float>(static_cast<int32_t>(realMs));
	if (state == _shown && state != SpeedIndicatorState::None)
	{
		_fade += ms * k_SpeedFadeInPerMs;
	}
	else
	{
		_fade -= ms * k_SpeedFadeOutPerMs;
	}
	if (_fade > 0.0f)
	{
		if (_fade >= 1.0f)
		{
			_fade = 1.0f;
		}
	}
	else
	{
		_fade = 0.0f;
		_shown = state;
	}

	if (_shown == SpeedIndicatorState::None || !gates.gameRunning)
	{
		return std::nullopt;
	}
	if (gates.sequenceMode || (gates.scriptOwnsSpeed && state != SpeedIndicatorState::Paused) || gates.editorMode ||
	    gates.videoPlaying || gates.setupBoxFlag)
	{
		_shown = state;
		_fade = 0.0f;
		return std::nullopt;
	}
	const float t = EasedFade(_fade);
	if (t == 0.0f || _shown == SpeedIndicatorState::None)
	{
		return std::nullopt;
	}
	const float depth = (t + 1.0f) * 15.0f;
	_phase += ms * k_SpeedPhasePerMs;
	if (_phase > k_Cycle)
	{
		_phase -= k_Cycle;
	}
	return Frame {.shown = _shown, .t = t, .depth = depth, .phase = _phase};
}

float Pitch(glm::vec3 centreRay, glm::vec3 topRay)
{
	const auto centre = Unit(centreRay, false);
	const auto top = Unit(topRay, true);
	const float dot = (centre.z * top.z + centre.y * top.y) + top.x * centre.x;
	return static_cast<float>(std::acos(static_cast<double>(dot)));
}

int32_t LetterColumn(int width, size_t letters, float depth, size_t letter)
{
	const auto half = static_cast<float>(width / 2);
	const float spacing = half / static_cast<float>(letters) / depth * 30.0f;
	const float first = half - (static_cast<float>(letters - 1) * spacing) * 0.5f;
	return static_cast<int32_t>(first + static_cast<float>(letter) * spacing);
}

LetterTurn TurnOf(float phase, size_t letter)
{
	float angle = phase - static_cast<float>(letter) * k_SpeedLetterLag;
	while (angle < 0.0f)
	{
		angle += k_Cycle;
	}
	while (angle > k_Cycle)
	{
		angle -= k_Cycle;
	}
	LetterTurn turn;
	if (!(angle < k_TwoPi))
	{
		turn.aboutVertical = true;
		angle -= k_TwoPi;
	}
	if (angle > k_Pi)
	{
		angle -= k_Pi;
	}
	if (angle >= k_HalfPi)
	{
		angle -= k_Pi;
	}
	turn.angle = angle;
	return turn;
}

std::array<glm::vec3, 3> LetterAxes(const LetterTurn& turn, float pitch)
{
	const double a = turn.angle;
	const auto cosine = static_cast<float>(std::cos(a));
	const auto sine = static_cast<float>(std::sin(a));
	std::array<glm::vec3, 3> axes {};
	if (turn.aboutVertical)
	{
		axes = {glm::vec3(cosine, 0.0f, sine), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-sine, 0.0f, cosine)};
	}
	else
	{
		axes = {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, cosine, -sine), glm::vec3(0.0f, sine, cosine)};
	}
	const double p = pitch;
	TurnAboutX(axes, std::cos(p), std::sin(p));
	const double halfTurn = k_Pi;
	TurnAboutX(axes, std::cos(halfTurn), std::sin(halfTurn));
	return axes;
}

std::vector<openblack::TextFrame> PlaceLetters(const graphics::billboard::CameraFrame& camera, const screen_point::Lens& lens,
                                               int width, int height, size_t letters, float depth, float phase)
{
	const int32_t middleX = width / 2;
	const int32_t row = height / 20;
	const auto centre = screen_point::PointFromScreen(camera, lens, middleX, height / 2, k_PitchDepth) - camera.eye;
	const auto top = screen_point::PointFromScreen(camera, lens, middleX, row, k_PitchDepth) - camera.eye;
	const float pitch = Pitch(centre, top);

	std::vector<openblack::TextFrame> frames;
	frames.reserve(letters);
	for (size_t i = 0; i < letters; ++i)
	{
		const auto axes = LetterAxes(TurnOf(phase, i), pitch);
		frames.push_back({
		    .origin = screen_point::PointFromScreen(camera, lens, LetterColumn(width, letters, depth, i), row, depth),
		    .axes = {InWorld(camera, axes[0]), InWorld(camera, axes[1]), InWorld(camera, axes[2])},
		});
	}
	return frames;
}

} // namespace openblack::help
