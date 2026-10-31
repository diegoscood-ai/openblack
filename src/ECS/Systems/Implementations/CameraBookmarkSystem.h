/*******************************************************************************
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
#include <memory>

#include "ECS/Systems/CameraBookmarkSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class CameraBookmarkSystem final: public CameraBookmarkSystemInterface
{
public:
	bool Initialize() override;
	void Update(uint32_t frameGameMs, bool scriptWideScreen) override;
	[[nodiscard]] const bookmark_pulse::Pulse& GetPulse() const override { return _pulse; }

	[[nodiscard]] const std::array<entt::entity, 8>& GetBookmarks() const override { return _bookmarks; }

	void SetBookmark(uint8_t index, const map_coords::MapCoords& position, std::optional<entt::entity> target,
	                 const glm::vec3& cameraOrigin) override;
	[[nodiscard]] bool IsSet(uint8_t index) const override;
	[[nodiscard]] std::optional<BookmarkFlight> FlightTo(uint8_t index) const override;
	[[nodiscard]] std::optional<BookmarkFlight> JumpTo(uint8_t index, const BookmarkJumpFrom& from) override;
	[[nodiscard]] std::optional<BookmarkFlight> ToggleFollow(uint8_t index, const BookmarkJumpFrom& from) override;
	[[nodiscard]] std::optional<uint8_t> GetFollowed() const override { return _followed; }
	[[nodiscard]] uint8_t GetCurrent() const override { return _current; }
	[[nodiscard]] std::optional<BookmarkFlight> ProcessTurn(bool playersCamera, bool cameraInputCounted,
	                                                        const BookmarkJumpFrom& from) override;
	void ClearBookmark(uint8_t index) override;
	void SetEnabled(bool enabled) override { _enabled = enabled; }
	[[nodiscard]] bool IsEnabled() const override { return _enabled; }

private:
	std::array<entt::entity, 8> _bookmarks;
	bool _enabled {true};
	/// One clock for every bookmark, kept from land to land
	bookmark_pulse::Pulse _pulse;
	/// The bookmark the camera follows, flying to it each turn
	std::optional<uint8_t> _followed;
	/// The last bookmark dropped or jumped to
	uint8_t _current {0};
	/// The view before the last jump: the interface status's place then, and the camera's offset from its land point
	struct BeforeJump
	{
		std::optional<map_coords::MapCoords> position;
		glm::vec3 cameraOffset {0.0f};
	};
	BeforeJump _beforeJump;
};
} // namespace openblack::ecs::systems
