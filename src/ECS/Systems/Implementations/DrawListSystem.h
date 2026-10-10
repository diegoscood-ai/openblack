/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "ECS/Systems/DrawListSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class EventManager;
}

namespace openblack::ecs::systems
{
/// The object draw list kept for the whole game. Made with an event manager, it hears the rebuild requests through it
/// for as long as it lives
class DrawListSystem final: public DrawListSystemInterface
{
public:
	explicit DrawListSystem(EventManager* eventManager = nullptr);
	DrawListSystem(const DrawListSystem&) = delete;
	DrawListSystem& operator=(const DrawListSystem&) = delete;
	DrawListSystem(DrawListSystem&&) = delete;
	DrawListSystem& operator=(DrawListSystem&&) = delete;
	~DrawListSystem() override = default;

	void OnRebuildRequested(uint8_t count) override;
	void OnClearMap() override;
	void Update(const DrawListFrame& frame, const DrawListInputs& inputs, const draw_list::Consumer& consumer) override;

	[[nodiscard]] size_t Count() const override;
	[[nodiscard]] uint8_t RebuildCount() const override;
	[[nodiscard]] uint32_t RebuildTurn() const override;
	[[nodiscard]] bool LastPassFull() const override;
	[[nodiscard]] bool LandFlag() const override;
	[[nodiscard]] std::span<const uint16_t> VisibleBlocks() const override;
	[[nodiscard]] std::span<const entt::entity> Entries() const override;
	[[nodiscard]] draw_list::Active ActiveAt(size_t index) const override;
	[[nodiscard]] std::optional<draw_list::Active> ActiveOf(entt::entity object) const override;
	[[nodiscard]] std::span<const entt::entity> DrawnThisFrame() const override;

private:
	draw_list::RebuildState _rebuild;
	draw_list::List _list;
	/// Each land block's state, in the land's order; empty until the first Update of a land
	std::vector<draw_list::BlockState> _blocks;
	/// The lines the blocks' level of detail is chosen against: placed when a land's blocks start (below detail index
	/// 4 they keep the values they had), then rebuilt from the camera every frame
	draw_list::LodLines _lodLines {draw_list::k_StartLodLines};
	/// Each block's place on the block grid, in the land's order, and the grid's lookup of one more than each block's
	/// index (0 for none), as the seam pass reads them. Made when a land's blocks start
	std::vector<glm::ivec2> _blockCoords;
	std::array<uint16_t, draw_list::k_BlockLookupSize> _blockLookup {};
	std::vector<uint16_t> _visible;
	/// The objects whose Draw the last Update called, in call order
	std::vector<entt::entity> _drawn;
	/// The blocks' table slots and the table's arrays, as the rebuild reads them
	std::vector<uint16_t> _slotOfBlock;
	std::array<std::span<const entt::entity>, draw_list::k_BlockSlots> _slots {};
	bool _lastPassFull {false};
	bool _landFlag {false};
	/// Lives as long as this: the event handler does nothing once it has gone
	std::shared_ptr<char> _alive;
};
} // namespace openblack::ecs::systems
