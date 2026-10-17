/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
struct GFishFarmInfo;
} // namespace openblack

namespace openblack::ecs
{
namespace components
{
struct Fish;
struct FishBait;
struct FishShoal;
} // namespace components

/// A draw between two bounds, for the shoals: the game's CRT random, or a test's fake
using FishRandom = std::function<float(float low, float high)>;

/// For each of the 15 fish: size 0.8..1.2, within +-5 (x, z) and -1..0 (y) of the centre, a random heading, speed
/// 0.5..1.5 and turn rate speed x about pi / 5 x (1 +- 0.1); the target is the centre
void InitFishShoal(components::FishShoal& shoal, const glm::vec3& centre);
/// InitFishShoal with the draws taken from `random`, ten per fish in the same order
void InitFishShoal(components::FishShoal& shoal, const glm::vec3& centre, const FishRandom& random);

/// The shoal's centre round a new farm: rings of radius 2, 4 ... below 50, 32 directions each; the first direction
/// that is open sea on two rings running gives the centre, on the second of them. None when there is no such sea
[[nodiscard]] std::optional<glm::vec2> FindShoalCentre(glm::vec2 farm, const std::function<bool(glm::vec2)>& isOpenSea);

/// The shoal's opacity at this squared distance from the camera: none beyond 300 units, full up to 200, then fading,
/// with the byte wrapping round once it has faded out, as the original stores it
[[nodiscard]] std::optional<uint8_t> ShoalAlpha(float distanceSquared);

/// The shoal's wandering point: the timer runs down by `elapsed` seconds, and below 0 the point moves to somewhere
/// within the range of the centre (z drawn first, then x), for half the distance it moved, in seconds
void RetargetShoal(components::FishShoal& shoal, float elapsed, const FishRandom& random);

/// A splash scares the shoal: its point goes 2 units from the centre at a drawn angle, and each of the first `shown`
/// fish within 8 units of the splash flees straight away from it for 2 s. Returns how many fled
size_t ScareShoal(components::FishShoal& shoal, size_t shown, const glm::vec3& splash, const FishRandom& random);

/// One fish's step for a frame of `seconds`: its fright runs down by all of it, then its animation frame, its swim and
/// its turn towards the target move by at most 0.1 s
void StepFish(components::Fish& fish, const glm::vec3& target, float seconds);

/// What a shoal's frame did: the fish that fled a splash, and the shown fish inside its bait's radius
struct ShoalMove
{
	size_t fled {0};
	uint32_t inside {0};
};

/// One frame of `seconds` of a shoal in sight: its wandering point, a splash's scare, then the step of its first `shown`
/// fish (the others stay where they are), counting those inside the bait's radius (x, z) when it has a bait
[[nodiscard]] ShoalMove MoveShoal(components::FishShoal& shoal, size_t shown, float seconds,
                                  const std::optional<glm::vec3>& splash, const components::FishBait* bait,
                                  const FishRandom& random);

/// A bait's frame, once per shoal swimming for it: its net moves by the frame's whole `seconds` (not capped), the shoal's fish
/// `inside` are added, and while all it needs are inside its hold time runs; true when it is done this frame (the net then
/// closes)
bool StepFishBait(components::FishBait& bait, uint32_t inside, float seconds);

/// Is one of the first `shown` fish within 2 units (x, z) of the point
[[nodiscard]] bool HasShownFishNear(const components::FishShoal& shoal, size_t shown, const glm::vec3& point);

/// Something hit the water here this frame (the hand gripping the sea, an object falling in).
/// Every shoal within 300 units of the camera reacts in the next UpdateFishShoals.
void SplashWater(const glm::vec3& point);

/// A farm's food after its turn: 1 more every numGameTurnsAfterWhichFoodIsIncreased turns of its info, then kept within
/// 0 and its full stock (its food value for any food) on every turn
[[nodiscard]] float FishFarmFoodAfterTurn(float food, uint32_t turn, const GFishFarmInfo& info);

/// Once per game turn, each farm: its food after the turn (FishFarmFoodAfterTurn with the game's fish farm info), then
/// its shoal's fullness = food / its full stock
void ProcessFishFarmsTurn(uint32_t turn);

/// The fish farm with a shown fish within 2 units (x, z) of the point
[[nodiscard]] std::optional<entt::entity> FindFishFarmAt(const glm::vec3& point);

/// Coastal (a land cell on the coast line) and no fish farm already in that map cell. Nothing else (town, depth,
/// distance). Only scripts create fish farms today; this is the rule for placing one by hand.
[[nodiscard]] bool IsOkToCreateFishFarmAt(const glm::vec3& point);

/// All of `amount` if the farm has it, else what is left; the stock drops accordingly
uint32_t RemoveFishFarmFood(entt::entity farm, uint32_t amount);

/// Moves the fish of every fish farm shoal by `seconds` of game time and works out each shoal's visibility and alpha
/// from the camera distance. For the shoals with a bait (the fish puzzle) it also runs the bait's rule: the fish
/// inside, the net, and when it is done a ring per fish.
void UpdateFishShoals(float seconds, const glm::vec3& camera);

} // namespace openblack::ecs
