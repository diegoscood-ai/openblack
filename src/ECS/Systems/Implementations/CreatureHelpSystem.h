/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <utility>

#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "Help/ScriptControl.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureHelpSystem final: public CreatureHelpSystemInterface
{
public:
	/// Says its messages through the game's script VM
	CreatureHelpSystem() = default;
	/// Says its messages through this one instead (a test's recorder)
	explicit CreatureHelpSystem(help::script_control::Vm vm)
	    : _vm(std::move(vm))
	{
	}

	void Raise(entt::entity creature, const creature_help::Message& message) override;
	void ProcessTurn() override;
	void PromptKnown(entt::entity creature, creature_help::Prompt prompt) override;
	void Fainting(entt::entity creature) override;

	void SetHelpOn(bool on) override { _helpOn = on; }
	[[nodiscard]] bool IsHelpOn() const override { return _helpOn; }

	[[nodiscard]] const std::vector<Told>& GetTold() const override { return _told; }

private:
	/// The help's watching of one creature this turn
	void UpdateCreature(entt::entity creature);
	void Tell(entt::entity creature, const creature_help::Message& message);
	/// The script VM the messages are said through, none without one
	[[nodiscard]] std::optional<help::script_control::Vm> SpeakingVm() const;

	std::optional<help::script_control::Vm> _vm;
	bool _helpOn {true};
	/// The game turn a message about a creature was last told: one a turn at most (none in the first turn)
	uint32_t _toldTurn {0};
	std::vector<Told> _told;
};

} // namespace openblack::ecs::systems
