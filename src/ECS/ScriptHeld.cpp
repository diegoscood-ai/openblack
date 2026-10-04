/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptHeld.h"

#include <cstdlib>

#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ScriptTimer.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

namespace openblack::ecs::script_held
{
using components::ScriptHeld;

namespace
{
bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_ANIMAL_TRACE") != nullptr;
	return trace;
}

ScriptHeld* SlotOf(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) ? registry.TryGet<ScriptHeld>(thing) : nullptr;
}

/// GScript::ReleaseControlFromScript(thing, slot, 0) (0x70D540) -> ReleaseScriptThingIntoTheGame (0x70F600)
void ReleaseControlFromScript(entt::entity thing, ScriptHeld& slot)
{
	if (!slot.controlledByScript)
	{
		// not controlled: only a script container (Flock::IsScriptContainer) gives back its members' references
		// (DecreaseContainerContentsScriptReference 0x6EFD60). openblack's CHL makes no script flock yet
		// (FLOCK_CREATE / FLOCK_ATTACH not implemented).
		return;
	}
	// ReleaseScriptThingIntoTheGame: SetControlledByScript(0); IsDeletedWhenReleasedFromScript (0x4021C0) is 0 for
	// every class but ScriptTimer (0x561300 = 1), and a script highlight (vt +0x48C) the script created
	// (IsCreatedByScript 0x70D440, the slot's +0xC) is deleted (ToBeDeleted(0) 0x70F670): a CREATE_HIGHLIGHT scroll goes
	// with its last reference unless RELEASE_FROM_SCRIPT released it first.
	// A created script container (a CREATE_FLOCK flock) would be deleted (ToBeDeleted 0x70D5AC): none in openblack yet.
	slot.controlledByScript = false;
	auto& registry = Locator::entitiesRegistry::value();
	if (slot.createdByScript && script_highlight::IsHighlight(thing))
	{
		ecs::ToBeDeleted(thing);
		return;
	}
	if (script_timer::IsTimer(thing))
	{
		// 0x70F61E..0x70F670: "Deleting thing not created by script" when the slot's +0xC is clear (not stopping),
		// then ToBeDeleted(0) (vt +0xC) and nothing else
		if (!slot.createdByScript)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Deleting thing not created by script");
		}
		ecs::ToBeDeleted(thing);
		return;
	}
	// then by GetScriptObjectType (jump table 0x70F754 / 0x70F76C): an animal (type 6 -> case 2, 0x70F6B1):
	// SetScriptState(this, 0x20 WANDER) and fn_0041AA00. The villagers' Villager::ReleaseFromScript (0x7531D0) and the
	// other types are not ported (no villager is put into a script state by openblack's CHL).
	if (registry.AllOf<components::Animal>(thing))
	{
		animal_ai::ReleaseFromScript(thing);
	}
}
} // namespace

void AddScriptThing(entt::entity thing, bool createdByScript)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return; // "Adding Null script thing"
	}
	// a thing in a script already has its slot (FindScriptGameThing); else a free slot: fn_0070D870 sets +0xC and
	// clears the count. Not ported: with createdByScript the Object's mesh (+0x40) vt+0x98(0) and +0xA |= 0x40.
	if (auto* slot = registry.TryGet<ScriptHeld>(thing); slot != nullptr && slot->inScript)
	{
		return;
	}
	auto& slot = registry.AllOf<ScriptHeld>(thing) ? registry.Get<ScriptHeld>(thing) : registry.Assign<ScriptHeld>(thing);
	slot.createdByScript = createdByScript;
	slot.references = 0;
	slot.hasSlot = true;
}

void IncrementReference(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return;
	}
	// the original's script values are slot numbers handed out by AddScriptGameThing; openblack's are entities and its
	// CHL finders (CALL, GET_...) do not call AddScriptThing, so a thing without a slot gets one here as the finders
	// would have given it (created 0) [approximated: the slot is taken at the first reference, not by the finder]
	auto* slot = registry.TryGet<ScriptHeld>(thing);
	if (slot == nullptr)
	{
		slot = &registry.Assign<ScriptHeld>(thing);
	}
	slot->hasSlot = true;
	if (slot->references == 0xFF)
	{
		return;
	}
	if (slot->references == 0)
	{
		// SetInScript(1); SetControlledByScript(+0x25 & 4 already || slot +0xC)
		slot->inScript = true;
		slot->controlledByScript = slot->controlledByScript || slot->createdByScript;
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: entity {} in script (controlled {})", static_cast<uint32_t>(thing),
			                   slot->controlledByScript);
		}
	}
	slot->inScript = true;
	++slot->references;
}

void DecrementReference(entt::entity thing)
{
	if (auto* slot = SlotOf(thing); slot != nullptr && slot->references != 0)
	{
		--slot->references;
	}
}

void Process()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> released;
	registry.Each<const ScriptHeld>([&released](entt::entity thing, const ScriptHeld& slot) {
		if (slot.hasSlot && slot.references == 0)
		{
			released.push_back(thing);
		}
	});
	for (const auto thing : released)
	{
		// an earlier release may have changed the registry
		auto* held = registry.Valid(thing) ? registry.TryGet<ScriptHeld>(thing) : nullptr;
		if (held == nullptr)
		{
			continue;
		}
		auto& slot = *held;
		const bool wasInScript = slot.inScript;
		ReleaseControlFromScript(thing, slot);
		if (Trace() && wasInScript)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: entity {} released", static_cast<uint32_t>(thing));
		}
		// SetInScript(0), then the slot is freed (fn_0070D800: ClearThingOnly + count 0); ControlledByScript is clear
		// now (fn_0070D480 0x70D504: "Thing should be released! PANIC")
		if (registry.Valid(thing))
		{
			registry.Remove<ScriptHeld>(thing);
		}
	}
}

bool IsInScript(entt::entity thing)
{
	const auto* slot = SlotOf(thing);
	return slot != nullptr && slot->inScript;
}

bool IsControlledByScript(entt::entity thing)
{
	const auto* slot = SlotOf(thing);
	return slot != nullptr && slot->controlledByScript;
}

void SetControlledByScript(entt::entity thing, bool controlled)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return;
	}
	if (auto* slot = registry.TryGet<ScriptHeld>(thing); slot != nullptr)
	{
		slot->controlledByScript = controlled;
	}
	else if (controlled)
	{
		auto& bits = registry.Assign<ScriptHeld>(thing);
		bits.controlledByScript = true;
		bits.hasSlot = false;
	}
}

bool CannotBeEaten(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<components::CannotBeEaten>(thing);
}

void SetCannotBeEaten(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing != entt::null && registry.Valid(thing) && !registry.AllOf<components::CannotBeEaten>(thing))
	{
		registry.Assign<components::CannotBeEaten>(thing);
	}
}

bool MayTarget(entt::entity hunter, entt::entity target)
{
	// if (IsInScript() && target +0x24 & 0x400) ok; else if (target +0x24 & 0x400) refuse
	return !IsControlledByScript(target) || IsInScript(hunter);
}

} // namespace openblack::ecs::script_held
