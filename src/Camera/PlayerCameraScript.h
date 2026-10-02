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

/// What the scripts set on the player's camera mode (CameraModeNew3) of runblack.exe W120: the camera zone of
/// SET_CAMERA_ZONE (CameraExclusion and the CameraModeNew3 force field globals), GET_INCLUSION_DISTANCE and
/// SET_FIXED_CAM_ROTATION (CameraModeNew3::ForceRotateAboutPoint). The script camera (Camera/ScriptCamera.h) is not
/// affected by any of them.
///
/// Only the data and the pure CameraExclusion::InsideInclusion 0x455E20 are ported: openblack's player camera
/// (DefaultWorldCameraModel) reads none of this yet (docs: dev\_scratch\asistente\patch3\research3.md, plan C / D)
namespace openblack::player_camera
{

/// The zone file's segment (LHFile::OpenSegment 0x455395)
constexpr const char* k_ZoneSegment = "cameraexc"; // 0x9CE6F4
/// SET_CAMERA_ZONE 0x6ED8F7: ".\Data\Zones\%s" (0xC0C248)
constexpr const char* k_ZoneFolder = "Zones";
/// ResetExclusionFile 0x455348 / 0x455352: [0x9CE6AC] and [0x9CE6A8] = 500 (0x43FA0000)
constexpr float k_DefaultZoneLimit = 500.0f;
/// CameraModeNew3::ForceFieldPoints [0xC5B130] .. ForceFieldPointCount [0xC5E130]: room for 0x3000 / 12 = 1024 points
constexpr size_t k_MaxForceFieldPoints = 1024;
/// LoadExclusionFile 0x45546F: the size of a CameraExclusion record it reads (else the records are skipped)
constexpr int32_t k_ExclusionRecordSize = 0x28;
/// InsideInclusion: [0x8C7BD8] = 1e-8 (squared distances), [0x8C79D8] = 9.9999997e-05 (a double)
constexpr float k_InclusionSquaredEpsilon = 1e-08f;
constexpr double k_InclusionEpsilon = 9.9999997473787516e-05;
/// InsideInclusion 0x455EA2 / 0x455EAF: the best ray parameters start at 1e20 (0x60AD78EC)
constexpr float k_InclusionFar = 1.0e20f;
/// The inclusion distance [0xC5E13C]: FLT_MAX ([0x8C7BB0]) at start (0x4548D0) and in ResetCameraModeNew3 0x460B2E;
/// CameraModeNew3::Update writes 1e10 (0x501502F9, 0x45FCE1) while there is no force field
constexpr float k_NoInclusionDistance = std::numeric_limits<float>::max();
constexpr float k_NoForceFieldDistance = 1.0e10f;

/// A CameraExclusion record (0x28 bytes, bw1-decomp Black/CameraExclusion.h): kept as read. None of the nine zone files of
/// Data\Zones has one (all have count 0)
struct Exclusion
{
	uint32_t id = 0;                    ///< +0x08, the file's id (1 from SET_CAMERA_ZONE)
	std::array<uint8_t, 0x28> bytes {}; ///< the record; +0 (next) is the list's
};

/// The globals of CameraExclusion and CameraModeNew3 a zone file sets (LoadExclusionFile 0x455370, in file order)
struct Zone
{
	int32_t header = 0;         ///< the first int32, read into a local and overwritten (1 in all nine files)
	int32_t flag9CE6B0 = 1;     ///< [0x9CE6B0] (meaning not read; CameraModeNew3::GetAltitude 0x459C7B reads it)
	int32_t drawForceField = 0; ///< CameraModeNew3::DrawForceField [0xC5E144]: InsideInclusion only checks with it on
	int32_t flagC5E14C = 0;     ///< [0xC5E14C] (meaning not read; CameraModeNew3::Update 0x45C28C)
	int32_t flagC5E148 = 0;     ///< [0xC5E148] (idem, 0x45C2B5)
	float limit9CE6AC = k_DefaultZoneLimit;  ///< [0x9CE6AC] (idem, 0x45C29B)
	float limit9CE6A8 = k_DefaultZoneLimit;  ///< [0x9CE6A8] (idem, 0x45C2BC)
	std::vector<glm::vec3> forceFieldPoints; ///< ForceFieldPoints [0xC5B130]: the inclusion polygon (x, z), with heights
	std::vector<Exclusion> exclusions;       ///< the CameraExclusion list [0xC5E160]
};

/// CameraModeNew3 +0x7C / +0x88 (ForceRotateAboutPoint 0x457330)
struct FixedRotation
{
	bool on = false;        ///< +0x88
	glm::vec3 point {0.0f}; ///< +0x7C
};

struct State
{
	Zone zone;
	float inclusionDistance = k_NoInclusionDistance; ///< [0xC5E13C]
	FixedRotation fixedRotation;
};

State& Get();

/// CameraExclusion::ResetExclusionFile(id) 0x455320: RemoveByID(id) 0x454A40 (the records of that id), [0x9CE6B0] = 1,
/// DrawForceField = [0xC5E14C] = [0xC5E148] = 0, [0x9CE6AC] = [0x9CE6A8] = 500, ForceFieldPointCount = 0
void ResetExclusionFile(uint32_t id);

/// CameraExclusion::LoadExclusionFile(file, id) 0x455370 on the bytes of a whole Lionhead segment file ("LiOnHeAd", then
/// segments of a 32-byte name, a u32 size and the data): ResetExclusionFile(id), then from segment "cameraexc" the int32
/// header, [0x9CE6B0], DrawForceField, [0xC5E14C], [0xC5E148], the floats [0x9CE6AC], [0x9CE6A8], the point count and the
/// points (12 bytes each), the record count and the record size; records of 0x28 bytes become CameraExclusions of `id`
/// (fn_00454960, +0 and +8 kept, 0x4554CF..0x455510), any other size skips them (0x45547C..0x4554B9). False when the
/// file or the segment is not there or is short (inferido: LHFile::GetSegmentData's own checks were not read; what was
/// read before stays)
bool LoadExclusionFile(const std::vector<uint8_t>& bytes, uint32_t id);

/// SET_CAMERA_ZONE 0x6ED890: ResetExclusionFile(1), ".\Data\Zones\<name>" opened (LHReleasedFile::Open 0x7BD730) and
/// LoadExclusionFile(file, 1), then [0x9CE6B0] = 1 and DrawForceField = 1 (0x6ED936..0x6ED940) whatever the file says;
/// a file that does not open -> "Couldn't load zone file-%s" (0xC0C22C) with the zone left reset. Returns false then
bool SetCameraZone(const std::string& name);

/// CameraExclusion::InsideInclusion(p, dir, hit, normal) 0x455E20: true without the force field or with fewer than 3
/// points; else the parity of the polygon's edges crossed by the ray p + t dir, t > 0, in x / z (an edge from each point
/// to the one before, the last before the first). True at once when p is on a point (squared distance < 1e-8) or on an
/// edge (|t| < 1e-4). `hit` gets the nearest crossing ahead (else behind, else p), then the nearest polygon point if
/// that one is nearer in x / z; `normal` the crossed edge's (e.z, 0, -e.x), e = the point before - the point
[[nodiscard]] bool InsideInclusion(const Zone& zone, const glm::vec3& p, const glm::vec3& dir, glm::vec3* hit,
                                   glm::vec3* normal);

/// CameraModeNew3::ForceRotateAboutPoint(point) 0x457330: +0x88 = 1 and +0x7C = *point, or +0x88 = 0 for null. The
/// player's mode then turns about that point (CameraModeNew3::Update copies +0x7C over +0x12C at 0x45AB00 and 0x460135);
/// Reinitialise 0x458B22 (and 0x458D23, 0x45E8D6) clears it. Not read by DefaultWorldCameraModel yet
void ForceRotateAboutPoint(const std::optional<glm::vec3>& point);

/// Not original: everything as at start (a new game, the tests)
void Reset();

} // namespace openblack::player_camera
