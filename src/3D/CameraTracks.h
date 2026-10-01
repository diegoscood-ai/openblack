/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

namespace openblack
{

/// The scripted cameras and camera tracks of Data\camera.edt (docs/bw1-notes/camera-tracks.md). The file is a
/// Lionhead segment file ("LiOnHeAd", then segments of a 32-byte name, a u32 size and the data), opened once by
/// GCameraEditor::CreateSegFile 0x445530 (the LHFile at 0xC59CF4) and read by name with LHFile::GetSegment 0x7BDDD0:
/// "EDITOR" (the editor's names, not read by the game), "Cam%d" (CameraBin, 32 bytes) and "Track%d".

/// Segment "Cam%d", copied as is by GCameraEditor::LoadCameraFromHD 0x446FE0: CONVERT_CAMERA_POSITION (0x6ED200)
/// pushes the floats 0..2, CONVERT_CAMERA_FOCUS (0x6ED270) the floats 3..5.
struct CameraBin
{
	glm::vec3 position;
	glm::vec3 focus;
	std::array<float, 2> unknown; ///< 0 and -1 in all 556 cameras of the file; not read by those two functions
};

/// An LH3DWay: a cubic Bezier path of `count` points, copied from the file with its u16 size in front and fixed up
/// by LH3DWay::AdjustPtr 0x844570 (the four array pointers at +0x14..+0x20 are rebuilt from +0x24).
struct CameraWay
{
	uint16_t size;     ///< +0x00, the bytes of the way = 0x24 + 44 * count
	uint16_t unknown2; ///< +0x02, 0x63 in every track
	float unknown8;    ///< +0x08, 0.26 in every track
	/// +0x0C, rebuilt by AdjustPtr: Σ over the segments with times[i] != times[i + 1] of
	/// dt * (speeds[i] - a * t0 + a / 2) with t0 = times[i] * 0.001, dt = times[i + 1] * 0.001 - t0,
	/// a = (speeds[i + 1] - speeds[i]) / dt; 0.1 if that is 0. Not read by the walk path.
	float length;
	int32_t duration;                                ///< +0x10, the samples (milliseconds) = ftol(times.back())
	std::vector<glm::vec3> points;                   ///< +0x14 -> +0x24: count points
	std::vector<std::array<glm::vec3, 2>> handles;   ///< +0x18: the two inner Bezier handles of segment i (after i)
	std::vector<float> times;                        ///< +0x1C: the time of each point, in ms
	std::vector<float> speeds;                       ///< +0x20: the speed at each point, units per second

	/// fn_008439C0(segment, t, &out): the Bezier (points[s], handles[s][0], handles[s][1], points[s + 1]) at t, with
	/// the original's order of the sums (x differs from y and z).
	[[nodiscard]] glm::vec3 Bezier(uint32_t segment, float t) const;
};

/// Segment "Track%d", read by ScriptedCamera::Create 0x447060 into a ScriptedCamera (12 bytes): +0 the first u32,
/// +4 a Running on the first way (the camera's position), +8 a Running on the second way (its focus).
struct CameraTrack
{
	uint32_t unknown0; ///< 0 in every track
	CameraWay position;
	CameraWay focus;
};

/// LH3DWay::Running (0x20C bytes, ctor 0x843ED0): the current segment (+0), an arc-length table of that segment
/// (+0x04..+0x200) and the last Bezier parameter (+0x204) of the way (+0x208).
class CameraWayRunner
{
public:
	explicit CameraWayRunner(const CameraWay& way); // 0x843ED0: segment 0, t 0, fn_00843F00

	/// fn_00844280(sample, &out): the point at `sample` ms. The segment is the first i with times[i + 1] >= sample
	/// (a new one rebuilds the table); sample >= duration gives the last point (segment and t unchanged), sample <= 0
	/// the first one (t = 0). In between the distance along the chord of the segment grows with a constant
	/// acceleration from speeds[i] such that the whole chord is covered at times[i + 1]; the fraction of the chord is
	/// the Bezier parameter (the table lookup gives it back, see the .cpp).
	glm::vec3 Get(int32_t sample);

	[[nodiscard]] uint32_t Segment() const { return _segment; }
	[[nodiscard]] float Parameter() const { return _t; }

private:
	void BuildTable(); // fn_00843F00

	const CameraWay* _way;
	uint32_t _segment {0};              ///< +0x000
	std::array<float, 128> _table {};   ///< +0x004: 127 * arc length / total at t = i / 127, [127] = 1.0 (+0x200)
	float _t {0.0f};                    ///< +0x204
};

/// ScriptedCamera::Create 0x447060: the track `number` ("Track%d"), nullptr if the file or segment is missing
/// ("Cannot load track No %d"). The tracks are kept once loaded.
std::shared_ptr<const CameraTrack> LoadCameraTrack(int32_t number);

/// GCameraEditor::LoadCameraFromHD 0x446FE0: the camera `number` ("Cam%d"); the original leaves the caller's buffer
/// as it was when the segment is missing, here std::nullopt.
std::optional<CameraBin> LoadCameraBin(int32_t number);

} // namespace openblack
