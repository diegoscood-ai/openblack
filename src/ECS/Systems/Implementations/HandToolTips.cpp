/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's tooltips: the interface's hand state (fn_005D7E40 -> GInterface +0x3AC, the table 0xD18278 and
// fn_005D7F20) and the tooltip function of each state (the table 0xBF1C10, fn_005D78D0), which submit to
// help::tooltips once per turn. Research: dev\documentacion\hand\tooltips\README.md and README_part2.md. Wiki:
// docs/bw1-notes/hand-and-interface.md, "Tooltips".

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include "ECS/Components/Field.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Tree.h"
#include "ECS/FishShoals.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Rocks.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/ThingFlags.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
// BINDABLE_ACTION 1 (LoadDefaults: the left button) and 2 (the right button, the action button)
constexpr int32_t k_ActionLeft = 1;
constexpr int32_t k_ActionRight = 2;
// the align bits of the icons' arrows (CameraHelp::DrawKeyOrMouse 0x447EA0): 0x300 up and down, 0xF00 all four
constexpr uint32_t k_ArrowsUpDown = 0x300;
constexpr uint32_t k_ArrowsAll = 0xF00;
} // namespace

int32_t HandSystem::InterfaceHandState() const noexcept
{
	// fn_005D7E40 with the action state (GInterface +0x44) openblack's hand is in; (not ported) 25 for an inactive
	// interface and 30 inside the citadel
	if (_gripPoint)
	{
		// action state 2 LANDSCAPE LOCK (StartLandscapeGrip 0x5D1F81), whatever the hand holds: 0x5D8090 gives 20 Grip
		// Landscape. (not ported) 22 Zoom Landscape with both buttons (+0x40 & 1 and EnabledFeatures & 4)
		return 20;
	}
	if (IsHoldingSeed())
	{
		return 5; // fn_005D7F20: something in the hand, a spell seed: Has Magic
	}
	if (_releaseArmed)
	{
		return 23; // action state 12 IN THROW: row 23, In Throw
	}
	if (_pickSource && _held)
	{
		return 14; // action state 4 IN LOCKED SELECT, 0x5D80C0 with an object (no creature): Select Lock
	}
	if (_held)
	{
		return 24; // fn_005D7F20: something in the hand: Object In Hand
	}
	// a tug is action state 13 WAIT FOR PLUCK with nothing placed in the hand yet (CHand::PickUp 0x46DC30 only writes
	// +0x4904 / +0x4908): fn_005D7F20's object branch. (inferred) the tree stays the collide object (+0x400) during it
	// action state 2 LANDSCAPE LOCK, 0x5D8090: (not ported) its 20 / 22 need the grab and action bits of GInterface +0x38
	// / +0x39; the fallback is fn_005D7F20, which with nothing under the hand is 3
	// fn_005D7F20: the object under the hand (GInterface +0x400)
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> object = _tug ? _tug : _hovered;
	if (!object && _cursorObject && ecs::IsAvailable(*_cursorObject) && hand_tap::Find(*_cursorObject) != nullptr)
	{
		object = _cursorObject; // a tap-only object (an abode) is the interface's object too
	}
	if (!object || !ecs::IsAvailable(*object))
	{
		return 3;
	}
	// (not ported) the leash (+0x410) 32 and the bubble (+0x408) 28; not IsInteractable (vt 0x190) 3: Abode::IsInteractable
	// 0x407200 (Abode, Field, StoragePit) needs vt 0x880, not read
	// out of the influence with InterfaceMustBeInInfluenceForInteraction (vt 0x714): 18
	if (!InInfluence())
	{
		return 18;
	}
	// ValidForLockedSelectProcess (vt 0x6CC): a pile that is not the hand's own type (0x66E4F0), a field with growth and
	// food (0x5299E0): 13. (approximate) 3 on the frame the grab-land button is pressed (GInterface byte +0x38 and +0x39
	// & 1, 0x5D7FD3): the tooltips run once a turn here
	const auto source = PotInfoOf(*object);
	const bool pile = source != PotInfo::_COUNT && source != PotInfo::HandWood && source != PotInfo::HandFood;
	const auto* field = registry.TryGet<const Field>(*object);
	if (pile || (field != nullptr && field->growth > 0.0f && field->food > 1.0f))
	{
		return 13;
	}
	// ValidForPlaceInHand (vt 0x6FC) and not IsCannotBePickedUp (vt 0x180): 9
	if (ValidForPlaceInHand(*object) && !ecs::thing_flags::IsCannotBePickedUp(*object))
	{
		return 9;
	}
	return 18;
}

void HandSystem::SubmitLandToolTips() const noexcept
{
	// fn_005D6980. (not ported) the leash first (CalculateLeashToolTip 0x5D67F0); the camera's tricons 0xE76 «Inclinar»
	// (Tricon +0x90 & 2) and 0xE77 «Rotar» (& 1), set by CameraModeNew3::UpdateTricons 0x459230 from the mouse near the
	// screen's centre: openblack has no tricons
	// over the water in the influence, a shown fish within 2 units (fn_00824B10 on the FishFarm list): 0xE73 «Recoger»
	if (_interactionPoint && !IsLand(*_interactionPoint) && InInfluence() && ecs::FindFishFarmAt(*_interactionPoint))
	{
		help::tooltips::Submit(0xE73, k_ActionRight, 0, false);
		return;
	}
	// (not ported) 0xE8B «Alejar» with the screen's centre on the land, the heading distance < 15 and the pitch > 0.55
	// (CameraModeNew3 +0xC8, +0x2E4, CalculatePitch; HelpProfile::Trigger(0x2C) first)
	help::tooltips::Submit(0xE7E, k_ActionLeft, k_ArrowsAll, false); // «Mover»
}

void HandSystem::SubmitToolTips() noexcept
{
	// fn_005D78D0: the row of the hand state in the table 0xBF1C10
	const auto& registry = Locator::entitiesRegistry::value();
	const int32_t state = InterfaceHandState();
	_interfaceHandState = state;
	const auto object = [&]() -> entt::entity {
		if (_hovered && ecs::IsAvailable(*_hovered))
		{
			return *_hovered;
		}
		return _cursorObject && ecs::IsAvailable(*_cursorObject) ? *_cursorObject : entt::null;
	}();
	switch (state)
	{
	case 3:  // Normal
	case 20: // Grip Landscape (its row's align 0xF00 is overwritten by fn_005D6980)
		SubmitLandToolTips();
		break;
	case 5: // Has Magic, fn_005D6C10. (pending) the seed's ValidToApplyThisToObject / ToMapCoord (0xE81 «Lanzar», 0xEF1)
		if (!InInfluence())
		{
			SubmitLandToolTips();
		}
		break;
	case 9: // Can Pick up, fn_005D6D70
	{
		// (not ported) the interface flag +0x28 & 2 (the land tooltips instead)
		if (physics::PhysicsObjects::Find(object) != nullptr)
		{
			// a flying object (Flags +0x24 & 0x40 and altitude >= 0, 0x5D6DA3): 0xE80 «Atrapar». (inferred) openblack's
			// physics list stands for the flag 0x40
			help::tooltips::Submit(0xE80, k_ActionRight, 0, false);
			break;
		}
		// IsCannotBePickedUp: nothing (0x5D6DC6). (not ported) the leash (CalculateLeashToolTip 0x5D67F0)
		if (ecs::thing_flags::IsCannotBePickedUp(object))
		{
			break;
		}
		// a one-shot orb: only its own GetOverwritePickUpToolTip (0x72AC50), forced (0x5D6E61), and nothing else.
		// (pending) its MagicEffectInfo +0x110 / the table 0xD9D7FC
		if (registry.AllOf<OneOffSpellSeed>(object))
		{
			break;
		}
		// GetOverwritePickUpToolTip (vt 0x184) is 0 for the other ported classes: 0xE73 «Recoger»
		help::tooltips::Submit(0xE73, k_ActionRight, 0, false);
		// a second submit when it can also be tapped: GetOverwriteTapToolTip (vt 0x19C) or 0xE7A «Golpear»; Rock 0x6E7A60
		// gives 0xEF7 «Golpear para Romper»
		const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
		if (hand_tap::ValidToTap(object, is))
		{
			help::tooltips::Submit(Rocks::IsRock(object) ? 0xEF7 : 0xE7A, k_ActionRight, 0, false);
		}
		break;
	}
	case 13: // Can Select Lock, fn_005D77C0
	{
		// (not ported) the leash first; a TownCentre or a TotemStatue: 0xECC / 0xEFC (the believers, the population)
		// locked by someone (Flags & 0x10, 0x5D7867): the land tooltips; openblack locks only the hand's own source
		if (_pickSource && *_pickSource == object)
		{
			SubmitLandToolTips();
			break;
		}
		// not locked: GetOverwriteInteractableToolTip (vt 0x194), else 0xE85 «Interactuar»
		if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr)
		{
			// Pot 0x66F540: IsPileFood (vt 0x480) ? 0xEFD «Cantidad Comida» : 0xEFE «Cantidad Madera», the builder's
			// GetResource
			const bool food = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).resourceType ==
			                  ResourceType::Food;
			help::tooltips::Submit(food ? 0xEFD : 0xEFE, k_ActionRight, 0, false, static_cast<float>(pot->amount));
		}
		else if (registry.AllOf<Field>(object))
		{
			help::tooltips::Submit(0xE73, k_ActionRight, 0, false); // Field 0x52A000
		}
		else
		{
			help::tooltips::Submit(0xE85, k_ActionRight, 0, false);
		}
		break;
	}
	case 14: // Select Lock: the row's 2 / 0xE85 / 0x300
		help::tooltips::Submit(0xE85, k_ActionRight, k_ArrowsUpDown, false);
		break;
	case 18: // Over Object, 0x5D7190
	{
		// (pending) a storage pit's 0xEF9 «Comida Almacenada: %3.0f Madera: %3.0f» (two numbers), the buildings' and the
		// town's texts (0xEFB, 0xEF4, 0xEE8, 0xECC, 0xEFC, 0xEF3, 0xECB, 0xEE9, 0xE88)
		// tappable and not IsCannotBePickedUp (0x5D7649 / 0x5D7657): GetOverwriteTapToolTip or 0xE7A «Golpear»
		const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
		if (InInfluence() && hand_tap::ValidToTap(object, is) && !ecs::thing_flags::IsCannotBePickedUp(object))
		{
			// Rock 0x6E7A60: 0xEF7; SpellIcon 0x726420 (pending: its info's +0x184) and the abodes keep 0xE7A
			help::tooltips::Submit(Rocks::IsRock(object) ? 0xEF7 : 0xE7A, k_ActionRight, 0, false);
			break;
		}
		SubmitLandToolTips();
		break;
	}
	case 24: // Object In Hand, fn_005D6F40
	{
		// (not ported) IsQueryIcon 0xE7C; out of the influence: the land tooltips
		if (!InInfluence())
		{
			SubmitLandToolTips();
			break;
		}
		const auto held = _held ? *_held : entt::null;
		// the target under the hand: (not ported) the scaffolds 0xEA5, a creature to give to 0xE87;
		// ValidToApplyThisToObject (vt 0x71C): 0xE8E (0xE8D on a sacrifice altar, not ported)
		if (held != entt::null && _cursorObject && ecs::IsAvailable(*_cursorObject) && HeldValidToApplyTo(*_cursorObject))
		{
			help::tooltips::Submit(0xE8E, k_ActionRight, 0, false);
			break;
		}
		// GetOverwriteDropToolTip (vt 0x198): Tree 0x74B790 0xEEF «Plantar», else 0xEEE «Soltar»; then 0xE74 «Lanzar»
		const bool tree = held != entt::null && registry.AllOf<Tree>(held);
		help::tooltips::Submit(tree ? 0xEEF : 0xEEE, k_ActionRight, 0, false);
		help::tooltips::Submit(0xE74, k_ActionRight, k_ArrowsAll, false);
		break;
	}
	default: // 23 In Throw and the rows with no text
		break;
	}
}
