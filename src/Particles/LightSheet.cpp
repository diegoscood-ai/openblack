/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LightSheet.h"

#include <cmath>

#include <algorithm>
#include <span>
#include <utility>

using namespace openblack::particles;

namespace
{
constexpr uint32_t k_Opaque = 0xFF000000u;
/// The added colour is half the light, its alpha an eighth
constexpr uint32_t k_HalfMask = 0xFEFEFEu;
constexpr uint32_t k_SpecularAlpha = 0x40000000u;
constexpr uint32_t k_ByteMax = 255u;
constexpr uint32_t k_CornersPerPoint = 3;

/// Each channel of a colour times a light of 0 to 255, over 256
uint32_t Lit(uint32_t argb, uint32_t light)
{
	const uint32_t red = ((argb & 0xFF0000u) * light) & 0xFF0000FFu;
	const uint32_t green = ((argb & 0xFF00u) * light) & 0xFF0000u;
	const uint32_t blue = ((argb & 0xFFu) * light) & 0xFF00u;
	return (red | green | blue) >> 8u;
}

/// The middle of the points as the game works it out: their sum, a float at a time, times the float of one over their
/// number
glm::vec3 MiddleOf(std::span<const glm::vec3> points)
{
	glm::vec3 sum(0.0f);
	for (const auto& point : points)
	{
		sum += point;
	}
	const float share = 1.0f / static_cast<float>(points.size());
	return share * sum;
}

/// A force field point's light, 0 to 255: its strength times 255, cut to a whole number
uint32_t ForceFieldLight(float strength)
{
	const float light = strength * 255.0f;
	if (!(light > 0.0f))
	{
		return 0;
	}
	return light >= 255.0f ? k_ByteMax : static_cast<uint32_t>(light);
}
} // namespace

void LightSheet::Start(std::vector<glm::vec3> points, uint32_t rgb, float height, float shiftSeconds)
{
	_points = std::move(points);
	_strengths.assign(_points.size(), 0.0f);
	_heights.assign(_points.size(), 0.0f);
	_rgb = rgb & 0xFFFFFFu;
	_spread = k_DefaultSpread;
	_slideSpeed = k_SlideSpeed;
	_height = height;
	_shiftSeconds = shiftSeconds;
	_strength = 0.0f;
	_time = 0.0f;
	_sinceShift = 0.0f;
	_slide = 0.0f;
}

void LightSheet::StartForceField(uint32_t rgb, float height, size_t capacity)
{
	_points.clear();
	_forceField = true;
	_capacity = capacity;
	_strengths.assign(capacity, 0.0f);
	_heights.assign(capacity, height);
	_rgb = rgb & 0xFFFFFFu;
	_spread = 1.0f;
	_slideSpeed = k_SlideSpeed;
	_height = height;
	_shiftSeconds = 0.0f;
	_strength = 0.0f;
	_time = 0.0f;
	_sinceShift = 0.0f;
	_slide = 0.0f;
}

void LightSheet::SetForceFieldPoints(std::span<const glm::vec3> fence)
{
	_points.assign(fence.begin(), fence.end());
	if (!_points.empty())
	{
		_points.push_back(_points.front());
	}
	// (the game has room for 256 and writes past them for a longer fence; ours stops at its room)
	_points.resize(std::min(_points.size(), _capacity));
}

void LightSheet::PulseForceField(const glm::vec3& at, float radius)
{
	if (!(radius > 0.0f) || !_forceField)
	{
		return;
	}
	for (size_t i = 0; i < _points.size(); ++i)
	{
		const float x = _points[i].x - at.x;
		const float z = _points[i].z - at.z;
		const float distance = std::sqrt((x * x) + (z * z));
		if (distance < radius)
		{
			_strengths[i] += 1.0f - (distance / radius);
		}
	}
}

float LightSheet::ForceFieldFade(float seconds)
{
	// e^-t as 2^(-t log2 e): the power's whole part and its fraction apart, each step rounded to a float as the game's
	// single precision does
	const float power = -static_cast<float>(1.4426950408889634 * static_cast<double>(seconds));
	const float whole = std::nearbyint(power);
	const auto fraction = static_cast<float>(power - whole);
	const auto kept = static_cast<float>(1.0 + (std::exp2(static_cast<double>(fraction)) - 1.0));
	return 1.0f - std::ldexp(kept, static_cast<int>(whole));
}

void LightSheet::Update(float seconds)
{
	// every step rounded to a float, as the game's single precision does
	_time += seconds;
	const float since = seconds + _sinceShift;
	_sinceShift = since;
	_slide -= seconds * _slideSpeed;
	if (_forceField)
	{
		// each point's strength fades towards nothing
		const float fade = ForceFieldFade(seconds);
		for (auto& strength : std::span(_strengths).first(_points.size()))
		{
			strength += (0.0f - strength) * fade;
		}
		return;
	}
	if (_shiftSeconds < since && !_strengths.empty())
	{
		do
		{
			_sinceShift -= _shiftSeconds;
			std::shift_right(_strengths.begin(), _strengths.end(), 1);
			_strengths.front() = _strength;
		} while (_shiftSeconds < _sinceShift);
	}
	const auto count = static_cast<int>(_points.size());
	if (count < 2)
	{
		return;
	}
	for (int i = 0; i < count; ++i)
	{
		const float along = (static_cast<float>(i) * k_WaveSpan) / static_cast<float>(count - 1);
		const double wave = std::cos(static_cast<double>(along - (_time * static_cast<float>(k_WaveSpeed))));
		const auto rolled = static_cast<float>(static_cast<double>(static_cast<float>(wave * k_WaveDepth)) + k_WaveMiddle);
		_heights[static_cast<size_t>(i)] = rolled * _height;
	}
}

void LightSheet::Build(std::vector<Vertex>& vertices, std::vector<uint32_t>& triangles)
{
	vertices.clear();
	triangles.clear();
	const auto count = _points.size();
	if (count < 2)
	{
		return;
	}
	if (_forceField)
	{
		BuildForceField(vertices, triangles);
		return;
	}
	_middle = MiddleOf(_points);
	_slide -= static_cast<float>(static_cast<int>(_slide));

	vertices.reserve(count * k_CornersPerPoint);
	float along = 0.0f;
	for (size_t i = 0; i < count; ++i)
	{
		const auto& point = _points[i];
		auto top = ((point - _middle) * _spread) + _middle;
		const float rise = _heights[i] * _strengths[i];
		top.y = rise + top.y;
		const auto bright = ((top - point) * k_BrightLine) + point;
		const auto light =
		    std::min(static_cast<uint32_t>(static_cast<int64_t>(rise * static_cast<float>(k_LightPerHeight))), k_ByteMax);
		const uint32_t lit = Lit(_rgb, light);
		vertices.push_back({.position = point, .uv = {along, 1.0f - _slide}, .argb = k_Opaque, .specularArgb = 0});
		vertices.push_back({.position = bright,
		                    .uv = {along, k_BrightRow - _slide},
		                    .argb = lit | k_Opaque,
		                    .specularArgb = ((lit & k_HalfMask) | k_SpecularAlpha) >> 1u});
		vertices.push_back({.position = top, .uv = {along, -_slide}, .argb = k_Opaque, .specularArgb = 0});
		const auto step = point - _points[(i + 1) % count];
		const float distance = std::sqrt(((step.z * step.z) + (step.y * step.y)) + (step.x * step.x));
		along = (distance * k_StarsPerUnit) + along;
	}

	triangles.reserve((count - 1) * 12);
	for (uint32_t s = 0; s + 1 < count; ++s)
	{
		const uint32_t b = s * k_CornersPerPoint;
		triangles.insert(triangles.end(), {b, b + 1, b + 3, b + 1, b + 4, b + 3, b + 1, b + 2, b + 4, b + 2, b + 5, b + 4});
	}
}

void LightSheet::BuildForceField(std::vector<Vertex>& vertices, std::vector<uint32_t>& triangles)
{
	const auto count = _points.size();
	_middle = MiddleOf(_points);
	_slide *= k_ForceFieldSlideKept;
	_slide -= static_cast<float>(static_cast<int>(_slide));
	if (std::ranges::none_of(std::span(_strengths).first(count),
	                         [](float strength) { return strength > k_ForceFieldShowsAbove; }))
	{
		return;
	}
	vertices.reserve(count * k_CornersPerPoint * 2);
	triangles.reserve((count - 1) * 12 * 2);
	for (uint32_t pass = 0; pass < 2; ++pass)
	{
		const auto first = static_cast<uint32_t>(vertices.size());
		float along = 0.0f;
		for (size_t i = 0; i < count; ++i)
		{
			const auto& point = _points[i];
			auto top = ((point - _middle) * _spread) + _middle;
			top.y = _heights[i] + top.y;
			const auto middle = ((top - point) * 0.5f) + point;
			const uint32_t argb = _rgb + (ForceFieldLight(_strengths[i]) << 24u);
			// the texture runs a tenth of a tenth of the distance along, sliding one way in the first pass and the other
			// in the second
			const float u = pass == 0 ? (along * k_StarsPerUnit) + _slide : (along * k_StarsPerUnit) - _slide;
			vertices.push_back({.position = point, .uv = {u, 0.0f}, .argb = argb, .specularArgb = 0});
			vertices.push_back({.position = middle,
			                    .uv = {u, 0.5f},
			                    .argb = argb,
			                    .specularArgb = ((argb & k_HalfMask) | k_SpecularAlpha) >> 1u});
			vertices.push_back({.position = top, .uv = {u, 1.0f}, .argb = argb & 0xFFFFFFu, .specularArgb = 0});
			const auto step = point - _points[(i + 1) % count];
			const float distance = std::sqrt(((step.z * step.z) + (step.y * step.y)) + (step.x * step.x));
			along = (distance * k_StarsPerUnit) + along;
		}
		for (uint32_t s = 0; s + 1 < count; ++s)
		{
			const uint32_t b = first + (s * k_CornersPerPoint);
			triangles.insert(triangles.end(), {b, b + 1, b + 3, b + 1, b + 4, b + 3, b + 1, b + 2, b + 4, b + 2, b + 5, b + 4});
		}
	}
}
