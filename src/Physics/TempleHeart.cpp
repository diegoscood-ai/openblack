/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHeart.h"

#include <cmath>

#include <optional>

namespace openblack::physics::temple_heart
{

Target Choose(std::span<const Town> towns)
{
	std::optional<entt::entity> first;
	for (const auto& town : towns)
	{
		for (const auto& building : town.buildings)
		{
			if (!building.available || !(building.life > 0.0f) || !(building.built > 0.0f) || building.field ||
			    building.footballPitch)
			{
				continue;
			}
			if (building.life > k_SoundLife)
			{
				return {.kind = TargetKind::Building, .entity = building.entity};
			}
			if (!first.has_value())
			{
				first = building.entity;
			}
		}
	}
	if (first.has_value())
	{
		return {.kind = TargetKind::Building, .entity = *first};
	}
	for (const auto& town : towns)
	{
		for (const auto& villager : town.homeless)
		{
			if (villager.available)
			{
				return {.kind = TargetKind::Villager, .entity = villager.entity};
			}
		}
	}
	return {};
}

float Harm(glm::vec3 velocity, float mass)
{
	// The speed, z and y first, then the momentum and the harm, a float a step
	const float zz = velocity.z * velocity.z;
	const float yy = velocity.y * velocity.y;
	const float xx = velocity.x * velocity.x;
	const float zy = zz + yy;
	const float squared = zy + xx;
	const float speed = std::sqrt(squared);
	const float momentum = speed * mass;
	const float harm = momentum * k_HarmPerMomentum;
	return k_MostHarm < harm ? k_MostHarm : harm;
}

uint32_t BeamInterval(uint32_t millisecondsPerTurn)
{
	// Turns a second as a whole number, times two, cut down to a whole number
	constexpr double k_Seconds = 2.0;
	return static_cast<uint32_t>(static_cast<double>(1000u / millisecondsPerTurn) * k_Seconds);
}

bool BeamAtTargetDue(Beam& beam, entt::entity target, uint32_t turn, uint32_t interval)
{
	if (target != beam.target)
	{
		beam.target = target;
		beam.turn = 0;
	}
	if (beam.target == entt::null || !(interval + beam.turn < turn))
	{
		return false;
	}
	beam.turn = turn;
	return true;
}

StrikeOutcome Outcome(const Strike& strike)
{
	if (!strike.struck)
	{
		return StrikeOutcome::Nothing;
	}
	switch (strike.target)
	{
	case TargetKind::Building:
		return StrikeOutcome::BuildingStruck;
	case TargetKind::Villager:
		return strike.villagerInPhysics ? StrikeOutcome::Nothing : StrikeOutcome::VillagerFlung;
	case TargetKind::Heart:
		break;
	}
	return strike.hitterAvailable && strike.breaksBuildings ? StrikeOutcome::HeartHarmed : StrikeOutcome::Nothing;
}

std::optional<float> DamageEffectLife(float life)
{
	if (!(life > 0.0f))
	{
		return 0.0f;
	}
	if (!(life < 1.0f))
	{
		return std::nullopt;
	}
	return life;
}

namespace
{
/// The wait between a hurt heart's beams in seconds, a float a step
float DamageWait(float life)
{
	const float difference = k_DamageWaitFull - k_DamageWaitNone;
	const float part = difference * life;
	return part + k_DamageWaitNone;
}
} // namespace

uint32_t DamageInterval(uint32_t millisecondsPerTurn, float life)
{
	// The wait times the whole turns a second, as a float, cut down to whole turns
	const float turns = DamageWait(life) * static_cast<float>(1000u / millisecondsPerTurn);
	return static_cast<uint32_t>(turns);
}

DamageBeamLook DamageLookAt(float life)
{
	const float difference = k_DamageAlphaFull - k_DamageAlphaNone;
	const float part = difference * life;
	const float alpha = part + k_DamageAlphaNone;
	return {.life = DamageWait(life), .alpha = static_cast<uint8_t>(static_cast<int32_t>(alpha))};
}

bool DamageBeamDue(uint32_t& lastTurn, uint32_t turn, uint32_t interval)
{
	if (!(interval + lastTurn < turn))
	{
		return false;
	}
	lastTurn = turn;
	return true;
}

bool DamageBeamSounds(float life)
{
	return !(life > k_DamageSoundLife);
}

bool BeamAtItselfDue(Beam& beam, uint32_t turn, uint32_t interval)
{
	if (beam.target != entt::null)
	{
		beam.target = entt::null;
		beam.turn = 0;
	}
	if (!(interval + beam.turn < turn))
	{
		return false;
	}
	beam.turn = turn;
	return true;
}

} // namespace openblack::physics::temple_heart
