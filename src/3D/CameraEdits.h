/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <optional>

#include <EDTFile.h>
#include <entt/core/hashed_string.hpp>
#include <glm/vec3.hpp>

namespace openblack::camera_edits
{

/// The camera editor's file in the resource cache
inline constexpr entt::hashed_string k_FileId = entt::hashed_string("camera/edits");

/// The scripts' numbered cameras and tracks, loaded once from Data/camera.edt; nothing without the file
[[nodiscard]] std::shared_ptr<const edt::EDTFile> Load();

/// The numbered camera, nothing when the file has none of that number
[[nodiscard]] std::optional<edt::EDTCamera> FindCamera(int32_t number);

/// What the scripts read of a numbered camera: its first three floats as the position, the next three as the focus;
/// (0, 0, 0) when there is no such camera
[[nodiscard]] glm::vec3 CameraPosition(const std::optional<edt::EDTCamera>& camera);
[[nodiscard]] glm::vec3 CameraFocus(const std::optional<edt::EDTCamera>& camera);

/// The numbered track, kept alive with the file; empty when the file has none of that number
[[nodiscard]] std::shared_ptr<const edt::EDTTrack> FindTrack(int32_t number);

} // namespace openblack::camera_edits
