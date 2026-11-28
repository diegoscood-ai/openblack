/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

#include "ECS/Systems/LandSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class LandSystem final: public LandSystemInterface
{
public:
	void SetView(LandViewInterface* view) override { _view = view; }
	void PrepareNewLand(bool playgroundStart, land_load::Resets resets) override;
	void ResetRegistryForNewLand() override;
	void StartNewLand(std::string_view land, bool startsAfresh, bool newGame) override;
	[[nodiscard]] bool ClearingMap() const override { return _clearingMap; }

private:
	[[nodiscard]] LandViewInterface& View() { return _view != nullptr ? *_view : _noView; }

	LandViewInterface* _view {nullptr};
	bool _clearingMap {false};
	NoLandView _noView;
};

} // namespace openblack::ecs::systems
