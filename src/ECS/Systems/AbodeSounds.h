/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <chrono>
#include <limits>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Audio/AnimEffectKeys.h"
#include "Audio/Game/BankTables.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The sounds Black & White's buildings make by themselves.
///
/// A windmill hums for as long as it works: a looping sound tag on it from the moment it is made, unless it is made
/// unavailable, which ends the turn it stops working (one made as a building site never hums, even once built). Most
/// buildings also sound the life around them: every frame one is drawn it rolls the local stream, and about once in
/// k_AmbienceTime of game time, if the camera is within k_AmbienceReach of it, it plays the editor bank's sound of the
/// time of day (dogs by day and at dusk, cockerels at dawn, nothing at night), owned by the building and following it.
/// The worship totem, the football pitch, the spell dispenser and the fields are drawn their own way and have no such
/// sound; neither have the citadel's parts, which are not buildings of this kind.
struct AbodeSounds
{
	/// The windmill's hum, from the InGame bank: a tag on the windmill that plays again whenever the sample is not
	/// playing (mode 2), looping for ever, in 3D, on a channel without track
	static constexpr int32_t k_WindmillSample = 13;
	static constexpr int32_t k_WindmillMode = 2;
	static constexpr int32_t k_WindmillLoops = -1;
	static constexpr bool k_WindmillTrack = false;
	static constexpr bool k_WindmillIs3D = true;
	static constexpr audio::SfxBank k_WindmillBank = audio::SfxBank::InGame;

	/// On average, a building rolls its ambient sound once in this much game time
	static constexpr std::chrono::duration<float, std::milli> k_AmbienceTime {80000.0f};
	/// How close the camera is for a building's ambient sound to play (strictly closer)
	static constexpr float k_AmbienceReach = 200.0f;
	/// Below this sky type it is day
	static constexpr float k_DaySkyType = 0.7f;
	/// From this sky type on it is night; in between it is dawn before noon and dusk after
	static constexpr float k_NightSkyType = 1.3f;
	static constexpr float k_Noon = 12.0f;
	/// The ambient sound plays from the editor bank, on a channel that follows the building
	static constexpr audio::SfxBank k_AmbienceBank = audio::SfxBank::Editor;
	static constexpr bool k_AmbienceTrack = true;

	/// Whether a building of this type gets the windmill's hum when it is made: only a windmill, and not one made
	/// unavailable
	[[nodiscard]] static constexpr bool HasWindmillHum(AbodeType type, bool unavailable) noexcept
	{
		return type == AbodeType::Windmill && !unavailable;
	}

	/// The windmill's hum started on a windmill just made: its tag, which nobody removes (it ends with the windmill's
	/// work)
	static SoundTagSystemInterface::TagId StartWindmillHum(SoundTagSystemInterface& tags, entt::entity windmill)
	{
		return tags.Create(windmill, glm::vec3(0.0f), k_WindmillSample, k_WindmillTrack, k_WindmillMode, k_WindmillLoops, false,
		                   k_WindmillIs3D, k_WindmillBank, 0);
	}

	/// Whether a building of this type plays the ambient sound of the time of day while it is drawn
	[[nodiscard]] static constexpr bool HasAmbience(AbodeType type) noexcept
	{
		return type != AbodeType::Totem && type != AbodeType::FootballPitch && type != AbodeType::SpellDispenser &&
		       type != AbodeType::Field && type != AbodeType::Citadel;
	}

	/// Every building drawn rolls the local stream for this every frame, before the camera's distance is known, and
	/// plays when the roll comes up 1: about once in k_AmbienceTime. The game time is the frame's whole milliseconds,
	/// and the quotient is truncated. A paused frame's quotient is infinite, which the conversion to an integer turns
	/// into the lowest int32, so the roll is then over 2^31.
	[[nodiscard]] static int32_t AmbienceChance(std::chrono::duration<float, std::milli> gameTime) noexcept
	{
		return gameTime.count() == 0.0f ? std::numeric_limits<int32_t>::min() : static_cast<int32_t>(k_AmbienceTime / gameTime);
	}

	/// The distance from the camera to the building's sound position, summed z, y, then x as the game does
	[[nodiscard]] static float AmbienceDistance(const glm::vec3& position, const glm::vec3& camera) noexcept
	{
		const float dx = position.x - camera.x;
		const float dy = position.y - camera.y;
		const float dz = position.z - camera.z;
		const float zy = dz * dz + dy * dy;
		return std::sqrt(zy + dx * dx);
	}

	/// Whether a building at this distance from the camera plays its ambient sound
	[[nodiscard]] static constexpr bool IsWithinAmbienceReach(float distance) noexcept { return distance < k_AmbienceReach; }

	/// The ambient sound of the time of day: the sky type and the hour of the visual clock
	[[nodiscard]] static constexpr audio::AnimEffectKeys AmbienceKeys(float skyType, float visualHour) noexcept
	{
		using audio::SoundAction;
		auto action = SoundAction::AbodeNight;
		if (skyType < k_DaySkyType)
		{
			action = SoundAction::AbodeDay;
		}
		else if (skyType < k_NightSkyType)
		{
			action = visualHour < k_Noon ? SoundAction::AbodeDawn : SoundAction::AbodeDusk;
		}
		return {.object = audio::SoundObject::Abode, .action = action};
	}
};

} // namespace openblack::ecs::systems
