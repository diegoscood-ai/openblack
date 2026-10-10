/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <vector>

#include "ECS/Systems/GestureSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The gesture system over the magic gestures module, which keeps its state in the hand magic state
class GestureSystem final: public GestureSystemInterface
{
public:
	/// What the system runs: once a frame, with the frame's game time, and when a land is loaded
	struct Passes
	{
		std::function<void(float seconds)> frame;
		std::function<void()> newLand;
	};

	/// With the magic gestures module's passes
	GestureSystem();
	/// With other passes (tests)
	explicit GestureSystem(Passes passes);

	// GestureEventsInterface
	[[nodiscard]] std::vector<GestureEvent> TakeEvents() override;
	void Inject(const GestureEvent& event) override;
	/// A new land: drops the waiting events, then runs the new-land pass (the gestures' state and the test stroke)
	void Reset() override;

	// GestureSystemInterface
	void Update(const Frame& frame) override;
	void ForgetPath() override;
	[[nodiscard]] float GetScreenAspect() const override;
	[[nodiscard]] float GetCircleSecondsLeft() const override;
	[[nodiscard]] bool IsGesturing() const override;

private:
	Passes _passes;
	std::vector<GestureEvent> _events;
};

} // namespace openblack::ecs::systems
