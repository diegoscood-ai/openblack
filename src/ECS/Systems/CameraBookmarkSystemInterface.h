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
#include <chrono>
#include <memory>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

class CameraBookmarkSystemInterface
{
public:
	virtual bool Initialize() = 0;
	virtual void Update(const std::chrono::microseconds& dt) const = 0;
	[[nodiscard]] virtual const std::array<entt::entity, 8>& GetBookmarks() const = 0;
	/// When flying to a bookmark, the bookmark position becomes the camera focus
	/// and the saved camera origin is restored. The original focus is lost.
	virtual void SetBookmark(uint8_t index, const glm::vec3& position, const glm::vec3& savedCameraOrigin) const = 0;
	virtual void ClearBookmark(uint8_t index) const = 0;
	/// [0x9CD384] (InitStaticsValues 0x54A81E = 1; PLAY_JC_SPECIAL 14 / 15 0x708F5A / 0x708F66; CleanGameForScriptReboot
	/// 0x6EBD04 = 1; saved by Bookmark::SaveAll 0x43A49C, pending with the saved games): the bookmark keys
	/// (GGame::ProcessKey 0x63F46B) and the bookmarks' drawing (fn_00439B90 from fn_005E5CD0 0x5E6180; pending: openblack
	/// draws the CameraBookmark entities with the other models) only while it is set
	virtual void SetEnabled(bool enabled) = 0;
	[[nodiscard]] virtual bool IsEnabled() const = 0;
};

} // namespace openblack::ecs::systems
