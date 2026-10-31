/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishShoals.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <functional>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "Common/GUtilsX87.h"
#include "Common/GameRandom.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/FishFarms.h"
#include "ECS/FishPuzzle.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack::ecs::components;
namespace map_coords = openblack::map_coords;
namespace x87 = openblack::gutils::x87;

namespace
{
/// Set by the splashes of this frame, cleared after the shoals have seen it
struct FishShoalsSplashState
{
	std::optional<glm::vec3> splash;
};

/// This frame's splash (Locator::worldEffects)
FishShoalsSplashState& Splash()
{
	if (!openblack::Locator::worldEffects::has_value())
	{
		std::fputs("ecs::fish_shoals: no world effects in the locator (Locator::worldEffects)\n", stderr);
		std::abort();
	}
	return openblack::Locator::worldEffects::value().Get<FishShoalsSplashState>();
}

bool Trace()
{
	static const bool k_Trace = openblack::debug_env::HandTrace();
	return k_Trace;
}

/// What the fish puzzle's trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct FishShoalsDebugHooksState
{
	uint32_t lastInside {~0u}; // OPENBLACK_HAND_TRACE: the last "inside" count logged
};

FishShoalsDebugHooksState& FishShoalsDebugHooksData()
{
	if (!openblack::Locator::debugHooks::has_value())
	{
		std::fputs("ecs::fish_shoals: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return openblack::Locator::debugHooks::value().Get<FishShoalsDebugHooksState>();
}

/// The game's CRT random, the shoals' draws
float CrtRandom(float low, float high)
{
	return openblack::game_random::crt::Random(low, high);
}
} // namespace

void openblack::ecs::StepFish(Fish& fish, const glm::vec3& target, float seconds)
{
	// a fish scared by a splash: its fright runs down by the whole frame's time first, then it swims 4 times faster
	// while a second or more is left, easing back to normal over its last second (the original's 1 - (1 - left))
	float boost = 1.0f;
	if (fish.fleeTime != 0.0f)
	{
		fish.fleeTime -= seconds;
		if (fish.fleeTime < 0.0f)
		{
			fish.fleeTime = 0.0f;
		}
		boost = fish.fleeTime < 1.0f ? (1.0f - (1.0f - fish.fleeTime)) * 3.0f + 1.0f : 4.0f;
	}
	// the step itself is at most 0.1 s
	const float dt = openblack::graphics::frame_anim::FishDt(seconds);
	// the frame, the cell before the wrap, then frame -= 15 truncate(frame / 15)
	fish.cell = openblack::graphics::frame_anim::FishFrame(fish.frame, dt, fish.speed);
	// the game's own sine and cosine, kept unrounded into the first product; the x step is stored before it is added,
	// the z step is not
	const auto [sin, cos] = x87::SinCos(fish.heading);
	const float stepX = x87::Store(x87::Mul(x87::Mul(x87::Mul(cos, fish.speed), dt), boost));
	const double stepZ = x87::Mul(x87::Mul(x87::Mul(sin, fish.speed), dt), boost);
	fish.position.x = x87::Store(x87::Add(stepX, fish.position.x));
	fish.position.z = x87::Store(x87::Add(stepZ, fish.position.z));
	// turn towards the shoal's target
	const float dx = target.x - fish.position.x;
	const float dz = target.z - fish.position.z;
	const float side = x87::Sub(x87::Mul(sin, dx), x87::Mul(dz, cos)) < 0.0 ? 1.0f : -1.0f;
	fish.heading += side * fish.turnRate * dt;
	// whole turns off, truncated towards 0, then one more turn off above pi: nothing is added below -pi
	constexpr float k_TurnsPerRadian = 0.15915494f;
	fish.heading -= static_cast<float>(static_cast<int32_t>(fish.heading * k_TurnsPerRadian)) * glm::two_pi<float>();
	if (fish.heading > glm::pi<float>())
	{
		fish.heading -= glm::two_pi<float>();
	}
}

void openblack::ecs::InitFishShoal(FishShoal& shoal, const glm::vec3& centre)
{
	InitFishShoal(shoal, centre, CrtRandom);
}

void openblack::ecs::InitFishShoal(FishShoal& shoal, const glm::vec3& centre, const FishRandom& random)
{
	const auto& Random = random;
	/// About 0.2 pi
	const auto turnScale = std::bit_cast<float>(0x3F20D97Cu);
	shoal.centre = centre;
	shoal.target = centre;
	// ten CRT draws per fish, in this order
	for (auto& fish : shoal.fish)
	{
		// the size, at least 1e-4
		fish.halfSize = std::max(Random(0.8f, 1.2f), 1e-4f);
		// the sprite's cell, truncate(r) & 0x3F
		fish.cell = static_cast<uint8_t>(static_cast<int32_t>(Random(0.0f, 15.5f)) & 0x3F);
		// the sprite's angle; every fish step sets it to the heading, the drawing uses the heading, so only the draw is
		// kept
		static_cast<void>(Random(0.0f, glm::pi<float>()));
		// the animation phase
		fish.frame = Random(0.0f, 15.0f);
		// z, y, x in that order, then the centre added
		const float z = Random(-5.0f, 5.0f);
		const float y = Random(-1.0f, 0.0f);
		const float x = Random(-5.0f, 5.0f);
		fish.position = glm::vec3(centre.x + x, centre.y + y, centre.z + z);
		fish.heading = Random(-glm::pi<float>(), glm::pi<float>());
		fish.speed = Random(0.5f, 1.5f);
		// ((r + 1) x speed) x about 0.2 pi
		const float r = Random(-0.1f, 0.1f);
		const float r1 = r + 1.0f;
		const float rs = r1 * fish.speed;
		fish.turnRate = rs * turnScale;
		fish.fleeTime = 0.0f;
	}
}

std::optional<glm::vec2> openblack::ecs::FindShoalCentre(glm::vec2 farm, const std::function<bool(glm::vec2)>& isOpenSea)
{
	// how many rings running each direction has been open sea on
	std::array<uint8_t, 32> seaCount {};
	for (float radius = 2.0f; radius < 50.0f; radius += 2.0f)
	{
		for (size_t i = 0; i < seaCount.size(); ++i)
		{
			const float angle = static_cast<float>(i) * glm::two_pi<float>() * 0.03125f;
			const glm::vec2 point(farm.x + std::cos(angle) * radius, farm.y + std::sin(angle) * radius);
			if (!isOpenSea(point))
			{
				seaCount[i] = 0;
				continue;
			}
			if (++seaCount[i] == 2)
			{
				return point;
			}
		}
	}
	return std::nullopt;
}

std::optional<uint8_t> openblack::ecs::ShoalAlpha(float distanceSquared)
{
	// nothing when d^2 > 90000 (300 units); when d^2 > 40000 the alpha byte is the low byte of
	// truncate((1 - (d^2 - 40000) * 0.0001) * 255), which reaches 0 at d^2 = 50000 (223.6 units) and then wraps
	// around (the truncation goes negative), exactly as the original stores it
	if (!(distanceSquared <= 90000.0f))
	{
		return std::nullopt;
	}
	return distanceSquared > 40000.0f
	           ? static_cast<uint8_t>(static_cast<int>((1.0f - (distanceSquared - 40000.0f) * 1e-4f) * 255.0f) & 0xFF)
	           : uint8_t {255};
}

void openblack::ecs::RetargetShoal(FishShoal& shoal, float elapsed, const FishRandom& random)
{
	// below 0 a new target: z then x drawn in the range, y the centre's, and the timer 0.5 x the distance from the
	// previous target
	shoal.timer -= elapsed;
	if (shoal.timer < 0.0f)
	{
		const auto previous = shoal.target;
		const float z = random(-FishShoal::k_Range, FishShoal::k_Range);
		const float x = random(-FishShoal::k_Range, FishShoal::k_Range);
		shoal.target = glm::vec3(x + shoal.centre.x, shoal.centre.y, z + shoal.centre.z);
		shoal.timer = 0.5f * glm::distance(shoal.target, previous);
	}
}

size_t openblack::ecs::ScareShoal(FishShoal& shoal, size_t shown, const glm::vec3& splash, const FishRandom& random)
{
	// the shoal darts 2 units in a random direction (a draw in 0..2 pi, at (cos, sin) in x and z); the fish within 8
	// units of the splash flee from it for 2 s, and only then the shoal's timer is 2
	// the game's own sine and cosine: the original turns (2, 0) by the angle in full, the cosine stored first and the
	// sine unrounded into its products
	const float r = random(0.0f, glm::two_pi<float>());
	const auto [sin, cos] = x87::SinCos(r);
	const float cosine = x87::Store(cos);
	const x87::Extended minusSin {-sin.high, -sin.low};
	const float x = x87::Store(x87::Add(x87::Add(x87::Mul(cosine, 2.0), x87::Mul(minusSin, 0.0)), 0.0));
	const float z = x87::Store(x87::Add(x87::Add(x87::Mul(cosine, 0.0), x87::Mul(sin, 2.0)), 0.0));
	shoal.target = glm::vec3(x87::Store(x87::Add(shoal.centre.x, x)), x87::Store(x87::Add(shoal.centre.y, 0.0)),
	                         x87::Store(x87::Add(shoal.centre.z, z)));
	size_t fled = 0;
	for (size_t i = 0; i < std::min(shown, shoal.fish.size()); ++i)
	{
		auto& fish = shoal.fish[i];
		// the squared distance summed z, y, x, as the original
		const float dx = splash.x - fish.position.x;
		const float dy = splash.y - fish.position.y;
		const float dz = splash.z - fish.position.z;
		if (dz * dz + dy * dy + dx * dx < 64.0f)
		{
			fish.fleeTime = 2.0f;
			shoal.timer = 2.0f;
			// the arctangent in double precision, stored to a float
			const float awayX = fish.position.x - splash.x;
			const float awayZ = fish.position.z - splash.z;
			fish.heading = static_cast<float>(std::atan2(static_cast<double>(awayZ), static_cast<double>(awayX)));
			++fled;
		}
	}
	return fled;
}

bool openblack::ecs::HasShownFishNear(const FishShoal& shoal, size_t shown, const glm::vec3& point)
{
	const auto count = static_cast<std::ptrdiff_t>(std::min(shown, shoal.fish.size()));
	return std::any_of(shoal.fish.begin(), shoal.fish.begin() + count, [&point](const Fish& fish) {
		const float dx = fish.position.x - point.x;
		const float dz = fish.position.z - point.z;
		return dx * dx + dz * dz < 4.0f;
	});
}

openblack::ecs::ShoalMove openblack::ecs::MoveShoal(FishShoal& shoal, size_t shown, float seconds,
                                                    const std::optional<glm::vec3>& splash, const FishBait* bait,
                                                    const FishRandom& random)
{
	ShoalMove move;
	// before the splash: the timer runs down by the frame's whole game time, in whole milliseconds (not capped: only
	// the fish's step is)
	const float elapsed = static_cast<float>(static_cast<uint32_t>(seconds * 1000.0f)) * 0.001f;
	RetargetShoal(shoal, elapsed, random);
	move.fled = splash.has_value() ? ScareShoal(shoal, shown, *splash, random) : 0;
	// every frame, a paused one too (its step of 0 s still takes each fish's cell from its frame and counts the bait);
	// the hidden fish (the stock is low) stay where they are until they come back
	for (size_t i = 0; i < std::min(shown, shoal.fish.size()); ++i)
	{
		StepFish(shoal.fish[i], shoal.target, seconds);
		// inside the bait's radius in (x, z)
		if (bait != nullptr)
		{
			const float dx = shoal.fish[i].position.x - bait->position.x;
			const float dz = shoal.fish[i].position.z - bait->position.z;
			move.inside += dx * dx + dz * dz < bait->radius * bait->radius ? 1 : 0;
		}
	}
	return move;
}

void openblack::ecs::SplashWater(const glm::vec3& point)
{
	Splash().splash = point;
}

std::optional<glm::vec3> openblack::ecs::PendingSplash()
{
	return Splash().splash;
}

float openblack::ecs::FishFarmFoodAfterTurn(float food, uint32_t turn, const GFishFarmInfo& info)
{
	// (openblack guard) an info of 0 turns never grows: the original would divide by 0
	const uint32_t every = info.numGameTurnsAfterWhichFoodIsIncreased;
	if (every != 0 && turn % every == 0)
	{
		food += 1.0f;
	}
	// below 0 -> 0, else above full -> full: on every turn
	if (food < 0.0f)
	{
		return 0.0f;
	}
	const float full = fish_farms::FoodValue(info, FoodType::Any);
	return full < food ? full : food;
}

void openblack::ecs::ProcessFishFarmsTurn(uint32_t turn)
{
	const auto& info = Locator::infoConstants::value().fishFarm;
	Locator::entitiesRegistry::value().Each<FishFarm>([turn, &info](FishFarm& farm) {
		farm.food = FishFarmFoodAfterTurn(farm.food, turn, info);
		if (farm.shoal.has_value())
		{
			farm.shoal->fullness = farm.food / fish_farms::FoodValue(info, FoodType::Any);
		}
	});
}

bool openblack::ecs::IsOkToCreateFishFarmAt(const glm::vec3& point)
{
	// coastal, then no FISH_FARM on the point's map cell: a fish farm is only in its own cell (it is inserted at its
	// position only)
	return sea_cells::IsCoastal(point) && map_cells::FindType(map_coords::CellOf(point), ObjectType::FishFarm) == entt::null;
}

std::optional<entt::entity> openblack::ecs::FindFishFarmAt(const glm::vec3& point)
{
	std::optional<entt::entity> found;
	Locator::entitiesRegistry::value().Each<const FishFarm>(
	    [&](entt::entity entity, const FishFarm& farm) {
		    // the shoals with a bait (the fish puzzle's) are skipped
		    if (found || !farm.shoal.has_value() || farm.shoal->bait != entt::null)
		    {
			    return;
		    }
		    if (HasShownFishNear(*farm.shoal, farm.VisibleFish(), point))
		    {
			    found = entity;
		    }
	    },
	    entt::exclude<Unavailable>);
	return found;
}

uint32_t openblack::ecs::RemoveFishFarmFood(entt::entity farm, uint32_t amount)
{
	// the farm takes a signed amount: the hand's amounts are small and positive, the same values
	return static_cast<uint32_t>(fish_farms::RemoveFood(farm, static_cast<int32_t>(amount)));
}

bool openblack::ecs::StepFishBait(FishBait& bait, uint32_t inside, float seconds)
{
	// the net's part under the water is drawn once per shoal of the bait: its update too
	AdvanceFishPlot(bait.net, seconds);
	bait.inside += inside;
	if (bait.inside < bait.need || bait.done)
	{
		return false;
	}
	// the game time step, whole milliseconds
	bait.timerMs += static_cast<uint32_t>(seconds * 1000.0f);
	if (bait.timerMs < bait.holdMs)
	{
		return false;
	}
	bait.done = true;
	bait.net.closing = true;
	return true;
}

void openblack::ecs::UpdateFishShoals(float seconds, const glm::vec3& camera)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto splash = Splash().splash;
	Splash().splash.reset();
	// no fish inside any bait yet this frame
	registry.Each<FishBait>([](FishBait& bait) { bait.inside = 0; });
	// moves the shoal and returns how many of its fish are inside its bait (0 when the bait is done: the
	// shoal is no longer moved nor drawn)
	const auto updateShoal = [&](FishFarm& farm, const FishBait* bait) -> uint32_t {
		auto& shoal = *farm.shoal;
		const size_t shown = std::min(farm.VisibleFish(), shoal.fish.size());
		shoal.shown = shown;
		if (bait != nullptr && bait->done)
		{
			shoal.visible = false;
			return 0;
		}
		const auto alpha = ShoalAlpha(glm::dot(shoal.centre - camera, shoal.centre - camera));
		shoal.visible = alpha.has_value();
		if (!shoal.visible)
		{
			return 0;
		}
		shoal.alpha = *alpha;
		// with the CRT draws
		const auto move = MoveShoal(shoal, shown, seconds, splash, bait, CrtRandom);
		if (move.fled > 0 && openblack::debug_env::TraceLogs(Trace(), "game"))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish trace: splash, {} fish flee", move.fled);
		}
		return move.inside;
	};
	// the game time step, whole milliseconds
	registry.Each<FishFarm>([&](FishFarm& farm) {
		if (!farm.shoal.has_value())
		{
			return;
		}
		auto* bait = farm.shoal->bait != entt::null ? registry.TryGet<FishBait>(farm.shoal->bait) : nullptr;
		const uint32_t inside = updateShoal(farm, bait);
		if (bait == nullptr)
		{
			return;
		}
		// the frame's whole game time: only the fish's step is capped
		if (!StepFishBait(*bait, inside, seconds))
		{
			return;
		}
		if (openblack::debug_env::TraceLogs(Trace(), "game"))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish puzzle: net closed ({}/{} inside for {} ms)", bait->inside,
			                   bait->need, bait->timerMs);
		}
		// every fish of every shoal of that bait (all 15, shown or not) leaves a white ring: growth 2, rate 1, cell 0x30
		const auto baitEntity = farm.shoal->bait;
		registry.Each<const FishFarm>([baitEntity](const FishFarm& other) {
			if (!other.shoal.has_value() || other.shoal->bait != baitEntity)
			{
				return;
			}
			for (const auto& fish : other.shoal->fish)
			{
				const water_rings::Ring ring {.position = fish.position, .growth = 2.0f, .argb = 0xFFFFFFFFu};
				Locator::waterRingSystem::value().Add(ring);
			}
		});
	});
	// a bait without all its fish inside starts its 500 ms again
	registry.Each<FishBait>([](FishBait& bait) {
		if (bait.inside < bait.need)
		{
			bait.timerMs = 0;
		}
		if (Trace() && !bait.done)
		{
			auto& last = FishShoalsDebugHooksData().lastInside;
			if (bait.inside != last)
			{
				last = bait.inside;
				if (auto logger = spdlog::get("game"); logger != nullptr)
				{
					SPDLOG_LOGGER_INFO(logger, "Fish puzzle: inside {}/{}", bait.inside, bait.need);
				}
			}
		}
	});
}
