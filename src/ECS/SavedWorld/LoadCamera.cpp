/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadCamera.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <variant>

#include <SavedRecords.h>
#include <WorldReader.h>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Events/CameraEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/SavedZoomer.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using gamesave::CameraRecord;

namespace
{
constexpr size_t k_ZoomerBytes = 48;
/// The camera is the first of the two objects the game object's tail names, before the climate
constexpr size_t k_TailCamera = 0;

/// The three zoomers of a Zoomer3 from 144 bytes, x, y then z
Zoomer3 Zoomer3Of(const std::array<uint8_t, 3 * k_ZoomerBytes>& bytes)
{
	Zoomer3 zoomers;
	for (size_t axis = 0; axis < zoomers.axis.size(); ++axis)
	{
		std::array<uint8_t, k_ZoomerBytes> one {};
		std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(axis * k_ZoomerBytes), k_ZoomerBytes, one.begin());
		zoomers.axis.at(axis) = saved_zoomer::ZoomerOf(one);
	}
	return zoomers;
}

/// The 144 bytes of a Zoomer3, x, y then z
std::array<uint8_t, 3 * k_ZoomerBytes> BytesOf(const Zoomer3& zoomers)
{
	std::array<uint8_t, 3 * k_ZoomerBytes> bytes {};
	for (size_t axis = 0; axis < zoomers.axis.size(); ++axis)
	{
		const auto one = saved_zoomer::BytesOf(zoomers.axis.at(axis));
		std::copy(one.begin(), one.end(), bytes.begin() + static_cast<std::ptrdiff_t>(axis * k_ZoomerBytes));
	}
	return bytes;
}
} // namespace

gamesave::ObjectNumber saved_camera::CameraNumber(const gamesave::WorldRead& world)
{
	if (world.objects.empty() || !world.objects.front().has_value())
	{
		return 0;
	}
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.front());
	return game != nullptr ? game->tail.objects.at(k_TailCamera).number : 0;
}

const CameraRecord* saved_camera::CameraOf(const gamesave::WorldRead& world)
{
	const auto number = CameraNumber(world);
	if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
	{
		return nullptr;
	}
	return std::get_if<CameraRecord>(&*world.objects[number - 1]);
}

saved_camera::Zoomers saved_camera::ZoomersOf(const CameraRecord& record)
{
	return {.origin = Zoomer3Of(record.second), .focus = Zoomer3Of(record.first), .fov = saved_zoomer::ZoomerOf(record.zoomer)};
}

void saved_camera::WriteLive(CameraRecord& record, const Zoomers& zoomers, float modeSeconds)
{
	record.words.at(0) = std::bit_cast<uint32_t>(modeSeconds);
	record.first = BytesOf(zoomers.focus);
	record.second = BytesOf(zoomers.origin);
	record.zoomer = saved_zoomer::BytesOf(zoomers.fov);
}

CameraRecord saved_camera::NewRecord(const Live& live)
{
	CameraRecord record;
	record.position.position = live.drawnAt;
	record.point = live.lookedAt;
	WriteLive(record, live.zoomers, live.modeSeconds);
	record.words.at(1) = std::bit_cast<uint32_t>(script_camera::FrameSecondsOf(live.frameMs));
	record.words.at(2) = live.scriptHoldsSpeed ? 1 : 0;
	record.words.at(3) = live.drawnMoved ? 1 : 0;
	record.eight = live.weather;
	record.mode = live.mode;
	return record;
}

gamesave::CameraModePart saved_camera::FollowPartOf(const Follow& follow)
{
	gamesave::CameraModePart part;
	part.mode = k_FollowMode;
	part.words = {std::bit_cast<uint32_t>(follow.heading),
	              std::bit_cast<uint32_t>(follow.pitch),
	              std::bit_cast<uint32_t>(follow.distance),
	              std::bit_cast<uint32_t>(follow.timeFactor),
	              follow.behind ? 1U : 0U,
	              follow.offsets ? 1U : 0U};
	part.object.number = follow.thing;
	part.lastWord = follow.clearView ? 1U : 0U;
	return part;
}

std::optional<saved_camera::Follow> saved_camera::FollowOf(const gamesave::CameraModePart& mode)
{
	if (mode.mode != k_FollowMode)
	{
		return std::nullopt;
	}
	return Follow {
	    .heading = std::bit_cast<float>(mode.words[0]),
	    .pitch = std::bit_cast<float>(mode.words[1]),
	    .distance = std::bit_cast<float>(mode.words[2]),
	    .timeFactor = std::bit_cast<float>(mode.words[3]),
	    .behind = mode.words[4] != 0,
	    .offsets = mode.words[5] != 0,
	    .thing = mode.object.number,
	    .clearView = mode.lastWord != 0,
	};
}

saved_camera::Follow saved_camera::PlayersFollow(const creature_follow::View& view, const creature_follow::Style& style,
                                                 gamesave::ObjectNumber thing)
{
	return {.heading = view.yaw,
	        .pitch = view.pitch,
	        .distance = view.distance,
	        .timeFactor = style.timeFactor,
	        .behind = style.behind,
	        .offsets = false,
	        .thing = thing,
	        .clearView = style.clearView};
}

saved_camera::Follow saved_camera::CreatureModeFollow(const creature_follow::View& view, gamesave::ObjectNumber creature)
{
	return PlayersFollow(view, {}, creature);
}

std::optional<creature_follow::Style> saved_camera::StyleOf(const Follow& follow)
{
	if (follow.offsets)
	{
		return std::nullopt;
	}
	return creature_follow::Style {.timeFactor = follow.timeFactor, .behind = follow.behind, .clearView = follow.clearView};
}

saved_camera::LoadedMode saved_camera::LoadedModeOf(const gamesave::CameraModePart& mode)
{
	if (mode.mode == k_PlainMode || (mode.mode == k_FollowMode && mode.object.number == 0))
	{
		return LoadedMode::Player;
	}
	return mode.mode == k_FollowMode ? LoadedMode::FollowOfAThing : LoadedMode::NotMade;
}

void saved_camera::Load(const gamesave::WorldRead& world)
{
	const auto* record = CameraOf(world);
	if (record == nullptr || !Locator::camera::has_value())
	{
		return;
	}
	// a saved follow comes back as a plain follow, not as the script's mode, whatever it was saved from: so a cut
	// scene's camera does not go on after a load, and the script's later moves of it find no script's mode. A follow
	// of nothing is taken off at the first turn, the player's camera starting again; a follow of a thing is made once
	// the things are back (LoadFollow)
	if (LoadedModeOf(record->mode) == LoadedMode::NotMade)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The saved camera's mode {:#x} is not made again: the player's camera starts from its place",
		                   record->mode.mode);
	}
	const auto zoomers = ZoomersOf(*record);
	// every mode goes, as the map's clear empties the stack, and the mode's seconds start again at 0 with the new
	// player's mode, whatever the record kept
	script_camera::ClearModes();
	auto& camera = Locator::camera::value();
	camera.GetOriginZoomer() = zoomers.origin;
	camera.GetFocusZoomer() = zoomers.focus;
	script_camera::Get().fov = zoomers.fov;
	// the player's camera starts again from the loaded zoomers: nothing held, nothing on its way, no fight watched
	camera.GetModel().Restart();
	events::Publish(events::PlayerCameraRestarted {});
}

void saved_camera::LoadFollow(const gamesave::WorldRead& world, const saved_world_write::LoadedObjects& loaded)
{
	const auto* record = CameraOf(world);
	if (record == nullptr || LoadedModeOf(record->mode) != LoadedMode::FollowOfAThing ||
	    !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto follow = FollowOf(record->mode);
	const auto found = loaded.find(follow->thing);
	const auto& registry = Locator::entitiesRegistry::value();
	// a thing the load did not make leaves the follow following nothing, which the first turn takes off
	if (found == loaded.end() || !registry.Valid(found->second.entity))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The saved camera follows object {}, which the load did not make: the player's camera goes on",
		                   follow->thing);
		return;
	}
	const auto thing = found->second.entity;
	if (const auto style = StyleOf(*follow);
	    style.has_value() && Locator::creatureModeSystem::has_value() &&
	    Locator::creatureModeSystem::value().Restore(
	        thing, {.yaw = follow->heading, .pitch = follow->pitch, .distance = follow->distance}, *style))
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "The saved camera's follow of object {} is not made again: the player's camera starts from its place",
	                   follow->thing);
}
