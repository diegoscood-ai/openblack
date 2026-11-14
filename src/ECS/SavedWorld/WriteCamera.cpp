/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game camera in a saved game's world file (docs/wiki/engine/saved-games.md#the-saves-in-play): a source of one
// object, the camera the game object's tail names. A loaded camera is written from the camera of the world: its
// zoomers, its mode's seconds and its current mode; the rest as the load kept it.

#include <cstdint>

#include <array>
#include <bit>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <GameTailRecords.h>
#include <SavedRecords.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/LoadCamera.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Weather/Atmos.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::CameraRecord;

namespace
{
/// The squared distance under which the camera's position is moved off its focus before its ray to the land
constexpr float k_NearlyAtFocus = 0.001f;

/// The one camera
std::vector<uint64_t> CameraKeys(const ecs::Registry& /*registry*/)
{
	return {ecs::saved_camera::k_CameraId};
}

/// The camera's zoomers: the script camera's while its script mode or a dual camera is the current mode, as they move
/// the camera's own zoomers then, else the camera of the world's; and the script camera's field of view
ecs::saved_camera::Zoomers ZoomersOfTheWorld(const Camera& camera)
{
	const auto& script = script_camera::Get();
	if (script_camera::HasMode())
	{
		return {.origin = script.position, .focus = script.focus, .fov = script.fov};
	}
	return {.origin = camera.GetOriginZoomer(), .focus = camera.GetFocusZoomer(), .fov = script.fov};
}

/// The camera's current mode as a save writes it: the script's mode, a follow, with its view and the thing its position
/// follows (when it still is one); the player's follow with the camera (Creature Mode's, or one a load made again), a
/// follow of its thing with its view and style; any other mode, the player's, a dual camera's, the creature
/// interaction's or the citadel's, saves nothing of its own
gamesave::CameraModePart CurrentMode(WriteContext& context)
{
	if (game_clock::IsInsideCitadel())
	{
		return {};
	}
	const auto& registry = context.GetRegistry();
	if (script_camera::ScriptModeCurrent())
	{
		const auto& script = script_camera::Get();
		const auto thing = script.positionThing;
		return ecs::saved_camera::FollowPartOf({
		    .heading = script.heading,
		    .pitch = script.pitch,
		    .distance = script.distance,
		    .timeFactor = script.timeFactor,
		    .behind = script.behind,
		    .thing = thing != entt::null && registry.Valid(thing) ? context.Reference(thing) : 0,
		});
	}
	// a dual camera or the creature interaction's camera is the current mode over the others
	if (script_camera::HasMode() || script_camera::Covered() || !Locator::creatureModeSystem::has_value())
	{
		return {};
	}
	const auto& creatureMode = Locator::creatureModeSystem::value();
	const auto view = creatureMode.GetView();
	const auto style = creatureMode.GetStyle();
	const auto followed = creatureMode.GetCreature();
	if (!view.has_value() || !style.has_value() || !followed.has_value() || !registry.Valid(*followed))
	{
		return {};
	}
	return ecs::saved_camera::FollowPartOf(ecs::saved_camera::PlayersFollow(*view, *style, context.Reference(*followed)));
}

/// The island camera's zoomers: the camera of the world's outside the citadel; inside, the ones the citadel keeps as the
/// player left them going in, as the rooms' camera does not move the island's. None without a camera
std::optional<ecs::saved_camera::Zoomers> LandCameraZoomers()
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	if (!game_clock::IsInsideCitadel())
	{
		return ZoomersOfTheWorld(Locator::camera::value());
	}
	const auto outside =
	    Locator::temple::has_value() ? Locator::temple::value().GetOutsideCamera() : std::optional<TempleOutsideCamera> {};
	if (!outside.has_value())
	{
		return std::nullopt;
	}
	return ecs::saved_camera::Zoomers {.origin = outside->origin, .focus = outside->focus, .fov = script_camera::Get().fov};
}

/// Where the camera is drawn on the map: x and z truncated to the map's units, its height above its own cell's altitude
/// (not the ground between the cell's corners), or above 0 off the map or where no block of land is
gamesave::MapCoords DrawnAt(const LandIslandInterface* land, glm::vec3 drawn)
{
	gamesave::MapCoords coords {.x = map_coords::ToFixed(drawn.x), .z = map_coords::ToFixed(drawn.z), .altitude = drawn.y};
	if (land == nullptr)
	{
		return coords;
	}
	const glm::u16vec2 cell {map_coords::CellOf(coords.x), map_coords::CellOf(coords.z)};
	if (cell.x < land->GetCellsPerSide() && cell.y < land->GetCellsPerSide() && land->HasBlockAt(cell))
	{
		coords.altitude =
		    drawn.y - static_cast<float>(land->GetCellAltitude(land->GetCell(cell))) * LandIslandInterface::k_HeightUnit;
	}
	return coords;
}

/// The point of the land the island camera looks at: the land's ray from its position (moved 1 back in x and up in y
/// when it is almost at its focus) through its focus, at the land's height there; none when the ray meets no land
std::optional<gamesave::Point> LookedAt(const LandIslandInterface& land, const ecs::saved_camera::Zoomers& zoomers,
                                        glm::vec3 drawn)
{
	auto position = zoomers.origin.GetCurrentValue();
	const auto focus = zoomers.focus.GetCurrentValue();
	const auto apart = position - focus;
	if (apart.x * apart.x + apart.y * apart.y + apart.z * apart.z < k_NearlyAtFocus)
	{
		position.x -= 1.0f;
		position.y += 1.0f;
	}
	glm::vec2 hit {0.0f};
	if (!land.RayCast(position, focus, hit, drawn))
	{
		return std::nullopt;
	}
	return gamesave::Point {hit.x, land.GetHeightAt(hit), hit.y};
}

/// A camera made since the load (a new game's): see saved_camera::NewRecord. Outside the citadel from the camera of
/// the world. Inside, the island camera's zoomers are the ones the citadel keeps and the point it looks at is the one it
/// looked at going in, as the game no longer casts its ray there; where it is drawn, whether it moved and the weather at
/// it are the rooms' camera's, as the game takes the drawn camera, the camera drawn the frame before held 1 m over the
/// land under it
std::optional<gamesave::SavedRecord> WriteNewCamera(WriteContext& context)
{
	const auto zoomers = LandCameraZoomers();
	if (!ecs::saved_camera::NewCameraWritable() || !zoomers.has_value())
	{
		context.Fail("a camera made since the load is written from the camera of the world and the land, and inside the "
		             "citadel from the island camera it keeps: there is none");
		return std::nullopt;
	}
	const auto& camera = Locator::camera::value();
	const auto& land = Locator::terrainSystem::value();
	const auto& script = script_camera::Get();
	const bool inside = game_clock::IsInsideCitadel();
	// the camera drawn: the camera of the world's outside; inside, the rooms' camera of the frame before
	const auto drawn = inside ? script.drawnBefore : camera.GetOrigin();
	const auto lookedAt = LookedAt(land, *zoomers, inside ? zoomers->origin.GetCurrentValue() : drawn);
	if (!lookedAt.has_value())
	{
		context.Fail("a camera made since the load that looks at no land is not written: the point it last looked at is "
		             "not held");
		return std::nullopt;
	}
	const auto held = inside ? ecs::saved_camera::HeldOverLand(drawn, land.GetHeightAt(glm::vec2(drawn.x, drawn.z))) : drawn;
	return ecs::saved_camera::NewRecord({
	    .zoomers = *zoomers,
	    .modeSeconds = script.modeSeconds,
	    .frameMs = script.frameMs,
	    .scriptHoldsSpeed =
	        Locator::scriptControlSystem::has_value() && !Locator::scriptControlSystem::value().MaySetGameSpeed(0),
	    .drawnMoved = inside ? held.y != drawn.y : script.drawnMoved,
	    .drawnAt = DrawnAt(&land, drawn),
	    .lookedAt = *lookedAt,
	    .weather = std::bit_cast<std::array<uint8_t, 8>>(weather::atmos::GetWeatherSmooth(held, true)),
	    .mode = CurrentMode(context),
	});
}

/// The camera: a loaded one with the island camera's zoomers (LandCameraZoomers), the seconds since its mode last
/// changed and its current mode, the rest as the load kept it; one made since, from the camera of the world
/// (WriteNewCamera)
std::optional<gamesave::SavedRecord> WriteCamera(WriteContext& context, uint64_t id)
{
	const auto kept = context.KeptRecordOf(SourceKey {.type = CameraRecord::k_Type, .id = id});
	const auto* loaded = kept.has_value() ? std::get_if<CameraRecord>(&*kept) : nullptr;
	if (loaded == nullptr)
	{
		return WriteNewCamera(context);
	}
	auto record = *loaded;
	if (const auto zoomers = LandCameraZoomers(); zoomers.has_value())
	{
		ecs::saved_camera::WriteLive(record, *zoomers, script_camera::Get().modeSeconds);
	}
	record.mode = CurrentMode(context);
	return record;
}
} // namespace

bool openblack::ecs::saved_camera::NewCameraWritable()
{
	if (!Locator::camera::has_value() || !Locator::terrainSystem::has_value())
	{
		return false;
	}
	// inside the citadel, only with the island camera the citadel keeps
	return !game_clock::IsInsideCitadel() ||
	       (Locator::temple::has_value() && Locator::temple::value().GetOutsideCamera().has_value());
}

void openblack::ecs::saved_world_write::RegisterCameraWriters(ClassWriters& writers)
{
	writers.RegisterSource(CameraRecord::k_Type, CameraKeys, WriteCamera);
}

void openblack::ecs::saved_world_write::KeepCamera(const gamesave::WorldRead& world, LoadedObjects& loaded)
{
	if (ecs::saved_camera::CameraOf(world) != nullptr)
	{
		loaded.try_emplace(ecs::saved_camera::CameraNumber(world),
		                   LoadedObject {.type = CameraRecord::k_Type, .id = ecs::saved_camera::k_CameraId});
	}
}
