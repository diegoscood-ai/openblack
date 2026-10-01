/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Mod world.foliage, [flyer] sections (e.g. the butterflies module): things flying around a kind of plant. Each lives
// on one plant (Foliage::FlyerHome, picked when its block is placed): it flies around it for a while, rising from
// the plant and coming back to it, then sits on it beating its wings slowly. All from the time and the flyer's seed;
// only its flight from the hand is kept: the hand coming close sends it darting away and up, like the fish from a
// splash, and then it flies back to its plant. By day only,
// unless night = on: at dusk they go one by one and come back at dawn (the game's hour, SkyInterface::GetTime).

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <LNDFile.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/Foliage.h"
#include "3D/LandIslandInterface.h"
#include "3D/SkyInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"

using namespace openblack;

namespace
{
constexpr float k_Pi = 3.1415927f;
constexpr float k_CellSize = 10.0f;
/// flyers further than this are not drawn (they are small: a pixel or two by then)
constexpr float k_FlyerReach = 110.0f;

/// 0..1 from a seed and a salt
float Unit(float seed, float salt)
{
	const float x = std::sin(seed * 12.9898f + salt * 78.233f) * 43758.5453f;
	return x - std::floor(x);
}
} // namespace

void Foliage::UpdateFlyers(LandIslandInterface& island, glm::vec3 cameraPosition, float distance, float seconds)
{
	_flyerInstances.clear();
	if (_flyers.empty())
	{
		return;
	}
	const glm::vec2 eye(cameraPosition.x, cameraPosition.z);
	const float reach = std::min(distance, k_FlyerReach);
	const int last = island.GetCellsPerSide() - 1;
	const float dt = std::clamp(seconds - _flyerSeconds, 0.0f, 0.1f);
	_flyerSeconds = seconds;
	// by day: 1 from 7 to 18:30, 0 from 20 to 5:30
	const float hour = Locator::skySystem::has_value() ? Locator::skySystem::value().GetTime() : 12.0f;
	const float daylight = glm::smoothstep(5.5f, 7.0f, hour) * (1.0f - glm::smoothstep(18.5f, 20.0f, hour));
	static const bool trace = std::getenv("OPENBLACK_HAND_TRACE") != nullptr;
	std::vector<glm::vec3> hands;
	if (Locator::handSystem::has_value())
	{
		for (const auto& hand : Locator::handSystem::value().GetPlayerHandPositions())
		{
			if (hand.has_value())
			{
				hands.push_back(*hand);
			}
		}
	}
	for (auto& chunk : _chunks)
	{
		if (!chunk.built || chunk.homes.empty() || glm::distance(chunk.centre, eye) > reach + 120.0f)
		{
			continue;
		}
		for (auto& home : chunk.homes)
		{
			const glm::vec2 homePoint(home.top.x, home.top.z);
			if (glm::distance(homePoint, eye) > reach)
			{
				continue;
			}
			const auto& flyer = _flyers[home.flyer];
			const auto& animation = _animations[home.animation];
			const float seed = home.seed;
			if (!flyer.night && Unit(seed, 11.0f) >= daylight)
			{
				home.fleeTime = 0.0f;
				home.offset = glm::vec3(0.0f);
				continue;
			}
			const float flight = glm::mix(flyer.flight.x, flyer.flight.y, Unit(seed, 1.0f));
			const float rest = glm::mix(flyer.rest.x, flyer.rest.y, Unit(seed, 2.0f));
			const float range = glm::mix(flyer.range.x, flyer.range.y, Unit(seed, 3.0f));
			const float above = glm::mix(flyer.height.x, flyer.height.y, Unit(seed, 4.0f));
			const float cycle = flight + rest;
			const float time = seconds * flyer.speed + seed * 1000.0f;
			const float inCycle = std::fmod(time, cycle);

			// the path: a wandering loop around the plant (two sines per axis), pulled in to the plant at the start and
			// the end of the flight
			const float w1 = 0.45f + 0.35f * Unit(seed, 5.0f);
			const float w2 = 0.45f + 0.35f * Unit(seed, 6.0f);
			const float p1 = Unit(seed, 7.0f) * 2.0f * k_Pi;
			const float p2 = Unit(seed, 8.0f) * 2.0f * k_Pi;
			const auto positionAt = [&](float t, float u) {
				const float envelope = std::sqrt(std::max(std::sin(k_Pi * std::clamp(u, 0.0f, 1.0f)), 0.0f));
				const glm::vec2 wander(std::sin(w1 * t + p1) + 0.4f * std::sin(2.3f * w1 * t + p2),
				                       std::sin(w2 * t + p2) + 0.4f * std::sin(1.7f * w2 * t + p1));
				const glm::vec2 point = homePoint + wander * (range * envelope / 1.4f);
				const float bob = 0.2f * std::sin(9.0f * t + p1) + 0.15f * std::sin(3.1f * t + p2);
				const float ground = island.GetDrawnHeightAt(point) + 0.3f;
				return glm::vec3(point.x, std::max(home.top.y + envelope * (above + bob), ground), point.y);
			};

			glm::vec3 position;
			float yaw;
			float beat = time; // wing beats, slower while it sits
			if (inCycle < flight)
			{
				const float u = inCycle / flight;
				position = positionAt(time, u);
				const glm::vec3 ahead = positionAt(time + 0.1f, u + 0.1f / flight);
				const glm::vec2 heading(ahead.x - position.x, ahead.z - position.z);
				// the image's top (the head) points along the plane's side: (-sin, cos) of the yaw
				yaw = glm::length(heading) > 1e-4f ? std::atan2(-heading.x, heading.y) : Unit(seed, 9.0f) * 2.0f * k_Pi;
				yaw += 0.12f * std::sin(11.0f * time + p2); // flutter
			}
			else
			{
				position = home.top;
				yaw = Unit(seed, 9.0f + std::floor(time / cycle)) * 2.0f * k_Pi;
				beat = time * 0.25f;
			}

			// the hand coming close
			if (flyer.flee > 0.0f)
			{
				// like the fish from a splash (FishShoals.cpp): the hand coming within flee (and not far above or
				// below) sends it darting straight away from it for 2 s, 4 times faster, easing back over the last
				// second and climbing; a hand still close when it slows down sends it off again. Then it flies back to
				// its path at its own pace, waiting where it is while the hand stays within 1.5 flee.
				const glm::vec3 at = position + home.offset;
				float nearest = 1e9f;
				for (const auto& hand : hands)
				{
					const glm::vec2 fromHand = glm::vec2(at.x, at.z) - glm::vec2(hand.x, hand.z);
					if (std::abs(hand.y - at.y) < 2.0f * flyer.flee)
					{
						nearest = std::min(nearest, glm::length(fromHand));
					}
					if (home.fleeTime < 1.0f && glm::length(fromHand) < flyer.flee && std::abs(hand.y - at.y) < 2.0f * flyer.flee)
					{
						home.fleeTime = 2.0f;
						home.away = glm::length(fromHand) > 1e-3f
						                ? glm::normalize(fromHand)
						                : glm::vec2(std::cos(seed * 6.28f), std::sin(seed * 6.28f));
						if (trace)
						{
							SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Flyer trace: flees at {:.1f},{:.1f},{:.1f}, hand {:.1f} away",
							                   at.x, at.y, at.z, glm::length(fromHand));
						}
						break;
					}
				}
				// its own speed along the path: the loop's size times its turning rate
				const float pace = std::max(range * 0.6f * flyer.speed, 1.0f);
				if (home.fleeTime > 0.0f)
				{
					const float boost = home.fleeTime >= 1.0f ? 4.0f : 1.0f + 3.0f * home.fleeTime;
					home.fleeTime = std::max(0.0f, home.fleeTime - dt);
					home.offset += glm::vec3(home.away.x, 0.0f, home.away.y) * (pace * boost * dt);
					home.offset.y += pace * 0.4f * boost * dt;
					beat = time * 1.5f; // beating hard
					yaw = std::atan2(-home.away.x, home.away.y) + 0.12f * std::sin(11.0f * time + p2);
				}
				else if (glm::length(home.offset) > 1e-3f)
				{
					const float left = glm::length(home.offset);
					const glm::vec3 back = -home.offset / left;
					beat = time; // it flies until it is back, even if its path is resting on the plant
					if (nearest > 1.5f * flyer.flee)
					{
						home.offset += back * std::min(pace * dt, left);
					}
					if (nearest > 1.5f * flyer.flee && left > 0.5f && glm::length(glm::vec2(back.x, back.z)) > 0.1f)
					{
						yaw = std::atan2(-back.x, back.z) + 0.12f * std::sin(11.0f * time + p2);
					}
				}
				position += home.offset;
				position.y = std::max(position.y, island.GetDrawnHeightAt(glm::vec2(position.x, position.z)) + 0.3f);
			}

			// the frame by the image's own delays, a whole one (frame_anim::DelayClock)
			const auto sample = animation.sprite.clock.At(beat);
			const auto frame = sample.frame;
			const auto layer = static_cast<uint16_t>(animation.sprite.first + frame);
			// folding, on top of the image's own frames: its halves turned up about the body by fold times the fold
			// that matches this frame's width, easing into the next frame's (i_data4.z = 3, w = the fold). The geometry
			// eases, the texture does not blend
			float fold = 0.0f;
			if (flyer.fold > 0.0f)
			{
				fold = flyer.fold * glm::mix(animation.folds[frame], animation.folds[sample.next], sample.fraction);
			}

			const auto cell = glm::clamp(glm::ivec2(glm::floor(glm::vec2(position.x, position.z) / k_CellSize)), 0, last);
			const float luminosity = static_cast<float>(island.GetCell(glm::u16vec2(cell)).luminosity) / 255.0f;
			const float width = home.width;
			// flat (i_data4.z = 2, or 3 folding), alpha tested like the plants: it writes depth, so the plants behind
			// it don't cover it
			_flyerInstances.push_back({{position.x, position.y, position.z, width},
			                           {width * _layerAspect[layer], static_cast<float>(layer), luminosity, yaw},
			                           {_layerTop[layer], 0.0f, 0.0f, static_cast<float>(Tint::None)},
			                           {0.0f, 0.0f, 0.0f, 0.0f},
			                           {0.0f, 0.0f, flyer.fold > 0.0f ? 3.0f : 2.0f, fold}});
		}
	}
}
