/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnRules.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "Body.h"

using namespace openblack;
using namespace openblack::physics;

namespace
{
// the A (hitter) and B (hit) codes of each collision sound type
constexpr std::array<int32_t, 33> k_HitterCodes = {21, 24, 20, 20, 20, 20, 20, 19, 19, 19, 19, 20, 25, 22, 22, 22, 22,
                                                   22, 30, 25, 35, 31, 24, 23, 24, 25, 25, 21, 21, 21, 33, 34, 42};
constexpr std::array<int32_t, 33> k_HitCodes = {11, 14, 10, 10, 10, 10, 10, 9,  9,  9,  9,  10, 15, 12, 12, 12, 12,
                                                12, 20, 15, 23, 19, 14, 13, 14, 15, 15, 16, 18, 17, 21, 22, 24};
} // namespace

float turn::Impact(glm::vec3 forceSum)
{
	const float sum2 = glm::dot(forceSum, forceSum);
	return sum2 > 0.0001f ? std::sqrt(sum2) * 0.05f : 0.0f;
}

std::optional<float> turn::CountedImpact(glm::vec3 forceSum)
{
	const float impact = Impact(forceSum);
	return impact > 0.0f ? std::optional(impact) : std::nullopt;
}

turn::HitterLookup turn::LookUpHitter(bool lastHitSet, bool listGrewInThisLoop)
{
	if (!lastHitSet)
	{
		return HitterLookup::None;
	}
	// a growth moves the list to new memory, and the body last touched is looked for by where it was
	return listGrewInThisLoop ? HitterLookup::Keep : HitterLookup::Slot;
}

float turn::GLoad(float impact, float mass)
{
	return impact / (mass * openblack::physics::k_Gravity);
}

int turn::CollisionSoundLevel(float gLoad, bool grain)
{
	if (!(gLoad >= 1.25f))
	{
		return 3;
	}
	return gLoad > 3.0f && !grain ? 1 : 2;
}

std::array<int32_t, 5> turn::CollisionSoundKey(int level, int hitterType, int hitType)
{
	const auto a = k_HitterCodes.at(static_cast<size_t>(std::clamp(hitterType, 0, 32)));
	const auto b = k_HitCodes.at(static_cast<size_t>(std::clamp(hitType, 0, 32)));
	return {level, 0, a, b, 75};
}

bool turn::WantsCollisionSound(bool resting, bool hitByBody, float impact, float mass)
{
	return !resting && (hitByBody || impact > mass * 4.905f);
}

bool turn::SoundPairs::MaySound(entt::entity a, entt::entity b) const
{
	return !(std::ranges::any_of(_pairs, [&](const Pair& p) { return (p.a == a && p.b == b) || (p.a == b && p.b == a); }) ||
	         _pairs.size() >= k_MostPairs);
}

void turn::SoundPairs::Add(entt::entity a, entt::entity b)
{
	_pairs.push_back({a, b, k_QuietTurns});
}

void turn::SoundPairs::EndTurn()
{
	for (auto& p : _pairs)
	{
		--p.turns;
	}
	std::erase_if(_pairs, [](const Pair& p) { return p.turns <= 0; });
}

bool turn::PassesCamera(float distanceSquared, float lastDistanceSquared, glm::vec3 velocity)
{
	return distanceSquared < 100.0f && lastDistanceSquared > 100.0f && glm::dot(velocity, velocity) > 400.0f;
}

bool turn::SunkOutOfSight(float centreHeight, float radius)
{
	return !(-radius < centreHeight);
}

void turn::EndOfTurn(EndOfTurnSteps& steps)
{
	for (size_t i = 0; i < steps.Count(); ++i)
	{
		steps.ClearPushes(i);
		steps.ToppleFelledTree(i);
		steps.FollowObject(i);
		if (!steps.TakeImpact(i))
		{
			continue;
		}
		steps.FindHitter(i);
		steps.CollisionSound(i);
		steps.React(i);
		steps.ConsiderDamageByThrowing(false);
		if (steps.WasHit())
		{
			steps.RecordHit();
			steps.ConsiderDamageByThrowing(true);
		}
	}
	steps.AgeSoundPairs();
}

uint32_t turn::BlendColour(uint32_t argb, uint32_t towards, int32_t amount)
{
	const auto channel = [argb, towards, amount](uint32_t shift) {
		const auto from = static_cast<int32_t>((argb >> shift) & 0xFFu);
		const auto to = static_cast<int32_t>((towards >> shift) & 0xFFu);
		// Each channel moves towards the other by the amount in 256ths, rounding down
		const int32_t moved = from + static_cast<int32_t>(std::floor(static_cast<float>((to - from) * amount) / 256.0f));
		return static_cast<uint32_t>(moved & 0xFF) << shift;
	};
	return (argb & 0xFF000000u) | channel(16) | channel(8) | channel(0);
}

float turn::DustPuffSize(PuffKind kind, float age, float size)
{
	// a drop keeps its size
	if (kind == PuffKind::Drop)
	{
		return size;
	}
	if (IsAdditive(kind))
	{
		// the fade is (2 - age) / 2, times four times the age over the first quarter second
		float fade = (2.0f - age) * 0.5f;
		if (age < 0.25f)
		{
			fade *= age * 4.0f;
		}
		// the sine is taken to more than a float's precision, and rounded once the 2 is added
		const float angle = age * 6.28318548f;
		const auto pulse = static_cast<float>(std::sin(static_cast<double>(angle)) + 2.0);
		return pulse * 0.33f * size * fade;
	}
	const float life = PuffLife(kind);
	float fade = (life - age) / life;
	const float growth = life * 0.125f;
	if (age < growth)
	{
		fade *= age / growth;
	}
	return fade * size;
}

void turn::FallDrop(glm::vec3& position, glm::vec3& velocity, float seconds)
{
	velocity.y = velocity.y - seconds * k_DropGravity;
	position.x = seconds * velocity.x + position.x;
	position.y = seconds * velocity.y + position.y;
	position.z = seconds * velocity.z + position.z;
}

glm::vec3 turn::DropLandingVelocity(float size, int32_t firstRand, int32_t secondRand)
{
	constexpr float k_Spread = 0.599999964f;
	constexpr float k_Back = 30.0f;
	const auto axis = [size](int32_t r) { return static_cast<float>(r % 100) * size * k_Spread - size * k_Back; };
	return {axis(firstRand), 0.0f, axis(secondRand)};
}

void turn::SlideDrop(glm::vec3& position, glm::vec3& velocity, float seconds)
{
	// (approximate) the power is taken in double, the original's on the FPU's single precision
	const auto slow = std::pow(0.25, static_cast<double>(seconds));
	velocity = glm::vec3(static_cast<float>(slow * velocity.x), static_cast<float>(slow * velocity.y),
	                     static_cast<float>(slow * velocity.z));
	position.x = seconds * velocity.x + position.x;
	position.y = seconds * velocity.y + position.y;
	position.z = seconds * velocity.z + position.z;
}

int32_t turn::DustPuffFrame(PuffKind kind, uint32_t variant, float age)
{
	if (IsAdditive(kind))
	{
		const auto frame = static_cast<int32_t>((variant + static_cast<uint32_t>(age * 8.0f)) & 15u);
		return kind == PuffKind::MoteReversed ? 15 - frame : frame;
	}
	return 16 + static_cast<int32_t>((variant + static_cast<uint32_t>(age * 2.0f)) & 15u);
}
