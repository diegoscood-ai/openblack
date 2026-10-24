/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

#include "ECS/Systems/ScriptObjectsSystemInterface.h"

namespace openblack::ecs::systems
{

/// The scripts' objects behind raffclar's interface. For now each call goes to our own logic (ECS/ScriptHeld.h), whose
/// places are the components::ScriptHeld of the entities themselves
class ScriptObjectsSystem final: public ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override;
	void EnterNative(uint32_t native) override;
	entt::entity Fetch(entt::entity object) override;
	void AddReference(entt::entity object) override;
	void RemoveReference(entt::entity object) override;
	void ReleaseFromScript(entt::entity object) override;
	void Replace(entt::entity from, entt::entity to) override;
	void Reset() override;
	void ClearForNewLand() override;
	void ReleaseUnreferenced() override;
	bool Disband(entt::entity container) override;
	[[nodiscard]] std::vector<ScriptObjectPlace> Places() const override;
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }

private:
	/// Whether the native the scripts last entered takes control of the things it is given
	bool _takesControl {false};
};

} // namespace openblack::ecs::systems
