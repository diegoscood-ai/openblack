/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

/// What the scripts set on the player's camera mode: the camera zone of SET_CAMERA_ZONE (the exclusions and the force
/// field), GET_INCLUSION_DISTANCE and SET_FIXED_CAM_ROTATION (ForceRotateAboutPoint). The script camera
/// (Camera/ScriptCamera.h) is not affected by any of them.
///
/// The zones a file sets are kept by cameraZoneSystem (camera_zones::Zones); the fence's crossing test is
/// camera_zones::CrossFence. openblack's player camera (DefaultWorldCameraModel) keeps inside the fence and writes the
/// inclusion distance; it does not use the other zones yet
namespace openblack::player_camera
{

/// The zone file's segment
constexpr const char* k_ZoneSegment = "cameraexc";
/// SET_CAMERA_ZONE reads ".\Data\Zones\<name>"
constexpr const char* k_ZoneFolder = "Zones";
/// Both zone limits after a reset
constexpr float k_DefaultZoneLimit = 500.0f;
/// The original's force field array has room for 1024 points
constexpr size_t k_MaxForceFieldPoints = 1024;
/// The size of an exclusion record ReadZoneFile reads (else the records are skipped)
constexpr int32_t k_ExclusionRecordSize = 0x28;
/// The inclusion distance: FLT_MAX at start and when the player camera resets; the player camera's update writes 1e10
/// while there is no force field
constexpr float k_NoInclusionDistance = std::numeric_limits<float>::max();
constexpr float k_NoForceFieldDistance = 1.0e10f;

/// A camera exclusion record (0x28 bytes): kept as read. None of the nine zone files of Data\Zones has one (all have
/// count 0)
struct Exclusion
{
	std::array<uint8_t, 0x28> bytes {}; ///< the record; its first field (next) is the list's
};

/// What a zone file sets (ReadZoneFile, in file order)
struct Zone
{
	int32_t header = 0;                         ///< the first int32, read into a local and overwritten (1 in all nine files)
	int32_t useExclusions = 1;                  ///< The exclusions and height limits apply (a script's zone always sets it)
	int32_t drawForceField = 0;                 ///< camera_zones::CrossFence only checks with it on
	int32_t useMaxAltitude = 0;                 ///< maxAltitude caps the camera, else 30000
	int32_t useHeightAboveLand = 0;             ///< heightAboveLand caps the camera above the land, else 30000
	float maxAltitude = k_DefaultZoneLimit;     ///< The highest altitude the camera may reach
	float heightAboveLand = k_DefaultZoneLimit; ///< The highest the camera may go above the land
	std::vector<glm::vec3> forceFieldPoints; ///< The inclusion polygon (x, z), with heights
	std::vector<Exclusion> exclusions;       ///< the exclusion list
};

/// The point the player camera is forced to turn about (ForceRotateAboutPoint)
struct FixedRotation
{
	bool on = false;
	glm::vec3 point {0.0f};
};

struct State
{
	float inclusionDistance = k_NoInclusionDistance;
	FixedRotation fixedRotation;
	/// Not original: how many times the fence has put the player's camera back (the camera debug panel shows it)
	uint32_t fenceHits = 0;
};

State& Get();

/// A zone file as it is read, over a reset zone: what it sets, whether the header was there to read and whether the
/// whole file was read
struct ZoneFileRead
{
	Zone zone;
	bool headerRead = false;
	bool whole = false;
};
/// SET_CAMERA_ZONE's file (CameraZoneLoader keeps it in the resources, cameraZoneSystem the zones it sets), on the
/// bytes of a whole Lionhead segment file ("LiOnHeAd", then segments of a 32-byte name, a u32 size and the data): from
/// segment "cameraexc" the int32 header, useExclusions, drawForceField, useMaxAltitude, useHeightAboveLand, the floats
/// maxAltitude, heightAboveLand, the point count and the points (12 bytes each), the record count and the record size; records
/// of 0x28 bytes become exclusions, any other size skips them. Not whole when the file or the segment is not there or is short
/// (what was read before stays)
[[nodiscard]] ZoneFileRead ReadZoneFile(const std::vector<uint8_t>& bytes);

/// Turns the fixed rotation on about *point, or off for null. The player's camera then turns about that point (its
/// update copies the point over its turning centre); reinitialising the camera clears it. Not read by
/// DefaultWorldCameraModel yet
void ForceRotateAboutPoint(const std::optional<glm::vec3>& point);

/// Not original: everything as at start (a new game, the tests)
void Reset();

} // namespace openblack::player_camera
