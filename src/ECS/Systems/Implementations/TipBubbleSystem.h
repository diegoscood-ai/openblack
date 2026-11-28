/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TipBubbleSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Our did-you-know bubble (help::Bubble) behind the tip bubble slot
class TipBubbleSystem final: public TipBubbleSystemInterface
{
public:
	void Show(entt::entity sign, uint32_t text) override;
	void Hide() override;
	[[nodiscard]] entt::entity GetSign() const override { return _sign; }
	[[nodiscard]] uint32_t GetText() const override { return _text; }

	void ProcessTurn() override;
	/// The turn's test once its answer is known: the sign gone or off the screen closes the bubble, else its time to
	/// show goes back to 3.0. Nothing while closed
	void ProcessSign(bool shown);
	void UpdateFrame(float gameMilliseconds) override;
	void Hover() override;

	[[nodiscard]] bool IsUp() const override;
	[[nodiscard]] float GetDisplayTime() const override { return _bubble.Life(); }
	[[nodiscard]] std::optional<glm::vec3> GetAnchor() const override;
	[[nodiscard]] help::Bubble& GetBubble() override { return _bubble; }

	void Reset() override;

private:
	entt::entity _sign {entt::null};
	uint32_t _text {0};
	/// Its life is kept from one opening to the next: the first bubble of a game shows only from the next turn
	help::Bubble _bubble;
};

} // namespace openblack::ecs::systems
