/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "Gui/CinemaBars.h"
#include "Gui/ScriptFade.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Owns the script fade and the cinema bars, at their start values when made
class CinematicDirectorSystem final: public CinematicDirectorSystemInterface
{
public:
	void FadeTo(uint8_t red, uint8_t green, uint8_t blue, float seconds) override;
	void FadeBackToNormal(float seconds) override;
	[[nodiscard]] bool IsFadeFinished() const override;
	[[nodiscard]] uint32_t GetFadeColour() const override;
	void SetFadeColour(uint32_t argb) override;

	void SetWideScreen(bool on, float transitionSeconds) override;
	void SnapWideScreen() override;
	[[nodiscard]] bool IsWideScreenOn() const override;
	[[nodiscard]] bool IsWideScreenTransitionFinished() const override;
	[[nodiscard]] float GetWideScreenFraction() const override;

	void ProcessTurn() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;

	void SetCloseClipping(bool close) override { _closeClipping = close; }
	[[nodiscard]] bool IsCloseClipping() const override { return _closeClipping; }

private:
	gui::ScriptFade _fade;
	gui::CinemaBars _bars;
	bool _closeClipping {false};
};

} // namespace openblack::ecs::systems
