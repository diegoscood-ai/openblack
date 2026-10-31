/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/DeadList.h"
#include "ECS/Systems/ToBeDeletedSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The dead list kept for the whole game; the entities it frees are destroyed in the entities registry
class ToBeDeletedSystem final: public ToBeDeletedSystemInterface
{
public:
	void Push(entt::entity entity) override;
	void Process(bool drain) override;
	void SetDeferred(bool deferred) override;
	[[nodiscard]] bool Deferred() const override;

private:
	DeadList<entt::entity> _deadList;
	bool _deferred {false};
};
} // namespace openblack::ecs::systems
