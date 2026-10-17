/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's tooltips: the interface's hand state and the tooltip function of each state, which submit to
// help::tooltips once per turn. Wiki: docs/wiki/interface/tooltips.md#tooltips.

#define LOCATOR_IMPLEMENTATIONS

#include <utility>

#include "Creature/CreatureHandRules.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/FishShoals.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/HandToolTipRules.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/Workshops.h"
#include "GameClock.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "Worship/Citadel.h"
#include "Worship/LeashPosts.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
// BINDABLE_ACTION 1 (LoadDefaults: the left button) and 2 (the right button, the action button)
constexpr int32_t k_ActionLeft = 1;
constexpr int32_t k_ActionRight = 2;
// the align bits of the icons' arrows: 0x300 up and down, 0xF00 all four
constexpr uint32_t k_ArrowsUpDown = 0x300;
constexpr uint32_t k_ArrowsAll = 0xF00;

void Submit(const hand_tooltips::ToolTip& tip)
{
	help::tooltips::Submit(tip.text, tip.action, tip.align, tip.force, tip.numbers);
}

/// What the hand is over, as the interface takes it: a village totem's plinth and the icon on it are both the totem
entt::entity AsInterfaceObject(entt::entity picked)
{
	if (Locator::villageTotemSystem::has_value())
	{
		if (const auto totem = Locator::villageTotemSystem::value().TotemOf(picked); totem.has_value())
		{
			return *totem;
		}
	}
	return picked;
}

/// A town centre or a village totem: its town's 0xECC then 0xEFC (nothing when it has no town). False for any other
/// object
bool SubmitTownToolTips(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.TryGet<const Abode>(object);
	const auto* totem = registry.TryGet<const VillageTotem>(object);
	const auto centre =
	    hand_tooltips::TownCentreForToolTips(object, abode != nullptr && abode->type == AbodeNumber::TownCentre,
	                                         totem != nullptr, totem != nullptr ? totem->townCentre : entt::null);
	if (!centre)
	{
		return false;
	}
	if (*centre == entt::null || !registry.Valid(*centre))
	{
		return true;
	}
	if (const auto town = ecs::abode_villagers::TownOf(*centre); town != entt::null && registry.AllOf<Town>(town))
	{
		const auto& data = registry.Get<const Town>(town);
		// the belief in me minus the most belief in another player, x 1000; the adults + children, and the adult and
		// child places
		const float lead = (ecs::town_belief::GetBeliefInPlayer(data.belief, PlayerNames::PLAYER_ONE) -
		                    ecs::town_belief::GetMaxBeliefMeNotIncluded(data.belief, PlayerNames::PLAYER_ONE)) *
		                   1000.0f;
		for (const auto& tip : hand_tooltips::TownToolTips(lead, data.stats.adults + data.stats.children,
		                                                   data.stats.adultPlaces + data.stats.childPlaces))
		{
			Submit(tip);
		}
	}
	return true;
}
} // namespace

int32_t HandSystem::InterfaceHandState() const noexcept
{
	// an interface that is not active: 25; inside the citadel: 30; else the row of the action state openblack's hand
	// is in
	if (!interface_active::IsActive())
	{
		return 25;
	}
	if (game_clock::IsInsideCitadel())
	{
		return 30;
	}
	if (_screenGrip)
	{
		return 0x1D; // action state 17 BUBBLE GRIP: row 17's hand state, Drag Bubble
	}
	if (_gripPoint)
	{
		// action state 2 LANDSCAPE LOCK, whatever the hand holds: 20 Grip Landscape. (not ported) 22 Zoom Landscape with
		// both buttons
		return 20;
	}
	if (IsHoldingSeed())
	{
		return 5; // something in the hand, a spell seed: Has Magic
	}
	if (_releaseArmed)
	{
		return 23; // action state 12 IN THROW: row 23, In Throw
	}
	if (_creatureLock)
	{
		// the locked select on a creature: 19 Creature Interaction while held, 18 while it starts and ends. (pending) a
		// field of the creature that gives 14 instead
		return _creatureLockState.phase == hand_creature::LockPhase::Held ? 0x13 : 18;
	}
	if (_lockedSelectAction && Locator::villageTotemSystem::has_value() &&
	    std::as_const(Locator::entitiesRegistry::value()).Valid(*_lockedSelectAction) &&
	    std::as_const(Locator::entitiesRegistry::value()).AllOf<VillageTotem>(*_lockedSelectAction))
	{
		// the locked select on a village totem: action state 4 IN LOCKED SELECT once the hand holds it, row 14 Select
		// Lock (with nothing in the hand, the TOTEM hand state); action state 3 WAIT LOCKED SELECT while it starts, 18
		return Locator::villageTotemSystem::value().IsGripped(*_lockedSelectAction) ? 14 : 18;
	}
	if (_pickSource && _held)
	{
		return 14; // action state 4 IN LOCKED SELECT with an object (no creature): Select Lock
	}
	if (_held)
	{
		return 24; // something in the hand: Object In Hand
	}
	// a tug is action state 13 WAIT FOR PLUCK with nothing placed in the hand yet: the object branch. (inferred) the
	// tree stays the object under the hand during it. Action state 2 LANDSCAPE LOCK: (not ported) its 20 / 22 need the
	// grab and action bits; the fallback is the object branch, which with nothing under the hand is 3.
	// The object under the hand:
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> object = _tug ? _tug : _hovered;
	if (object && ecs::IsAvailable(*object))
	{
		object = AsInterfaceObject(*object);
	}
	if (!object && _cursorObject && ecs::IsAvailable(*_cursorObject))
	{
		// a tap-only object (an abode), a creature or a village totem (its plinth or its icon) is the interface's
		// object too
		const auto picked = AsInterfaceObject(*_cursorObject);
		if (picked != *_cursorObject || hand_tap::Find(picked) != nullptr || registry.AllOf<Creature>(picked))
		{
			object = picked;
		}
	}
	if (!object || !ecs::IsAvailable(*object))
	{
		// nothing under the hand but a screen object: 28 Over Bubble. (not ported) its extra help call
		return _screenObject ? 0x1C : 3;
	}
	if (registry.AllOf<Creature>(*object))
	{
		// a creature is neither a pile nor a field and never goes into the hand: 18, in or out of the influence
		return 18;
	}
	// (not ported) the leash's 32 before the object. Then the original's order of tests (hand_tooltips::ObjectHandState).
	// Valid for a locked select: a pile that is not the hand's own type, a field with growth and food, a village totem
	// the player may take hold of. (approximate) 3 on the frame the grab-land button is pressed: the tooltips run once a
	// turn here
	const auto source = PotInfoOf(*object);
	const bool pile = source != PotInfo::_COUNT && source != PotInfo::HandWood && source != PotInfo::HandFood;
	const auto* field = registry.TryGet<const Field>(*object);
	const bool totem = registry.AllOf<VillageTotem>(*object) && Locator::villageTotemSystem::has_value() &&
	                   Locator::villageTotemSystem::value().ValidForLockedSelect(*object, PlayerNames::PLAYER_ONE);
	return hand_tooltips::ObjectHandState({
	    .interactable = Interactable(*object),
	    .inInfluence = InInfluence(),
	    // only a script highlight may be used out of the influence
	    .mustBeInInfluence = !registry.AllOf<ScriptHighlight>(*object),
	    .validForLockedSelect = pile || (field != nullptr && field->growth > 0.0f && field->food > 1.0f) || totem,
	    .validForPlaceInHand = ValidForPlaceInHand(*object),
	    .cannotBePickedUp = ecs::object_flags::IsCannotBePickedUp(*object),
	});
}

void HandSystem::SubmitLandToolTips() const noexcept
{
	// (not ported) the leash first; the camera's tricons 0xE76 "Tilt" and 0xE77 "Rotate", set from the mouse near the
	// screen's centre: openblack has no tricons.
	// over the water in the influence, a shown fish within 2 units: 0xE73 "Pick Up"
	if (_interactionPoint && !IsLand(*_interactionPoint) && InInfluence() && ecs::FindFishFarmAt(*_interactionPoint))
	{
		help::tooltips::Submit(0xE73, k_ActionRight, 0, false);
		return;
	}
	// (not ported) 0xE8B "Zoom Out" with the screen's centre on the land, the heading distance < 15 and the pitch > 0.55
	help::tooltips::Submit(0xE7E, k_ActionLeft, k_ArrowsAll, false); // "Move"
}

void HandSystem::SubmitToolTips() noexcept
{
	// the row of the hand state in the tooltip table, as InterfaceHandState left it this turn
	const auto& registry = Locator::entitiesRegistry::value();
	const int32_t state = _interfaceHandState;
	const auto object = [&]() -> entt::entity {
		if (_hovered && ecs::IsAvailable(*_hovered))
		{
			return AsInterfaceObject(*_hovered);
		}
		return _cursorObject && ecs::IsAvailable(*_cursorObject) ? AsInterfaceObject(*_cursorObject) : entt::null;
	}();
	switch (state)
	{
	case 3:  // Normal
	case 20: // Grip Landscape (its row's align 0xF00 is overwritten by the land tooltips)
		SubmitLandToolTips();
		break;
	case 5: // Has Magic: the seed's apply to the object under the hand, else to the land under it
	{
		const bool inInfluence = InInfluence();
		const bool hasSeed = inInfluence && IsHoldingSeed();
		const auto target = _cursorObject && ecs::IsAvailable(*_cursorObject) ? *_cursorObject : entt::null;
		bool validToObject = false;
		bool validToMap = false;
		auto magicType = MagicType::None;
		if (hasSeed)
		{
			const auto seed = *_held;
			if (target != entt::null)
			{
				validToObject = SeedValidToApplyToObject(seed, target);
			}
			else
			{
				// (0, 0, 0) with no point under the hand, as the press
				validToMap = SeedValidToApplyToMapCoord(seed, magic::ToMap(_interactionPoint.value_or(glm::vec3(0.0f))));
				// the magic of the power-up gesture being made
				magicType = magic::MagicTypeForGesture(magic::seed::InfoOf(registry.Get<const SpellSeed>(seed)),
				                                       magic::gestures::State().currentPowerUpGesture);
			}
		}
		const auto choice =
		    hand_tooltips::SeedToolTip(inInfluence, hasSeed, target != entt::null, validToObject, validToMap, magicType);
		if (choice.shows == hand_tooltips::Shows::Land)
		{
			SubmitLandToolTips();
		}
		else if (choice.shows == hand_tooltips::Shows::Own)
		{
			Submit(choice.tip);
		}
		break;
	}
	case 9: // Can Pick up
	{
		// (not ported) an interface flag (the land tooltips instead)
		if (registry.Valid(object) && registry.AllOf<ecs::components::InPhysics>(object))
		{
			// an object in the physics (InPhysics: a moving body, a carried object or one left hanging where a hand let
			// go of it; and altitude >= 0): 0xE80 "Catch"
			help::tooltips::Submit(0xE80, k_ActionRight, 0, false);
			break;
		}
		// cannot be picked up: nothing. (not ported) the leash
		if (ecs::object_flags::IsCannotBePickedUp(object))
		{
			break;
		}
		// a one-shot orb: only its own pick-up tooltip, forced, and nothing else
		if (const auto* orb = registry.TryGet<const OneOffSpellSeed>(object); orb != nullptr)
		{
			const auto text = magic::OneOffPickUpToolTip(Locator::infoConstants::value(), orb->seedType, orb->powerUp);
			Submit(hand_tooltips::OrbPickUpToolTip(static_cast<uint32_t>(text)));
			break;
		}
		// the other ported classes do not override the pick-up tooltip: 0xE73 "Pick Up"
		help::tooltips::Submit(0xE73, k_ActionRight, 0, false);
		// a second submit when it can also be tapped: its own tap tooltip or 0xE7A "Tap"; a rock gives 0xEF7 "Tap to
		// Break"
		const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
		if (hand_tap::ValidToTap(object, is))
		{
			help::tooltips::Submit(object_physics::IsRock(object) ? 0xEF7 : 0xE7A, k_ActionRight, 0, false);
		}
		break;
	}
	case 13: // Can Select Lock
	{
		// (not ported) the leash first. A town centre or a village totem: its town's 0xECC / 0xEFC (the believers, the
		// population)
		if (SubmitTownToolTips(object))
		{
			break;
		}
		// locked by someone: the land tooltips; openblack locks only the hand's own source
		if (_pickSource && *_pickSource == object)
		{
			SubmitLandToolTips();
			break;
		}
		// not locked: its own interact tooltip, else 0xE85 "Interact"
		if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr)
		{
			// a pot: a food pile ? 0xEFD "Food Amount" : 0xEFE "Wood Amount", with the pile's resource
			const bool food =
			    Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).resourceType == ResourceType::Food;
			help::tooltips::Submit(food ? 0xEFD : 0xEFE, k_ActionRight, 0, false, static_cast<float>(pot->amount));
		}
		else if (registry.AllOf<Field>(object))
		{
			help::tooltips::Submit(0xE73, k_ActionRight, 0, false);
		}
		else if (const auto* post = registry.TryGet<const LeashPost>(object); post != nullptr)
		{
			// a temple's leash post has its own, one for each leash, though no post is a select lock (over a post the hand
			// is Over Object, which shows the plain 0xE7A "Tap")
			help::tooltips::Submit(worship::leash_posts::ToolTipOf(post->index), k_ActionRight, 0, false);
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
	case 0x13: // Creature Interaction: the row's 2 / 0xE85 / 0xF00
		help::tooltips::Submit(0xE85, k_ActionRight, k_ArrowsAll, false);
		break;
	case 18: // Over Object, the can-select-lock check
	{
		// (not ported) the leash first; a site to build (0xEFB, 0xEF4), the needs visuals (0xEFB / 0xECE / 0xEFF), the
		// citadel heart 0xEE8. A town centre or a village totem: its town's 0xECC then 0xEFC with no button, before the
		// influence test
		if (SubmitTownToolTips(object))
		{
			break;
		}
		// a creature: the interact text over the player's own creature, or over another player's in the influence; a
		// creature of no player gets the land tooltips. (pending) a flag of the creature that skips it
		if (const auto* creature = registry.TryGet<const Creature>(object); creature != nullptr)
		{
			if (creature_hand::ShowsInteractTip(creature->owner == PlayerNames::PLAYER_ONE,
			                                    creature->owner != PlayerNames::NEUTRAL, InInfluence()))
			{
				Submit(hand_tooltips::CreatureInteractToolTip());
			}
			else
			{
				SubmitLandToolTips();
			}
			break;
		}
		// out of the influence: the land tooltips
		if (!InInfluence())
		{
			SubmitLandToolTips();
			break;
		}
		// a spell icon (a worship site's or a town centre's): its seed's tooltip, forced, and nothing else (none when the
		// seed has no tooltip)
		if (const auto* icon = registry.TryGet<const SpellIcon>(object); icon != nullptr)
		{
			const auto& seed = Locator::infoConstants::value().spellSeed.at(static_cast<size_t>(icon->seedType));
			Submit(hand_tooltips::SpellIconToolTip(static_cast<uint32_t>(seed.tooltip)));
			break;
		}
		// a workshop: only a built one, 0xEF3 with no numbers; one being built goes on as any other object
		if (ecs::workshops::IsWorkshop(object))
		{
			if (const auto tip = hand_tooltips::WorkshopToolTip(ecs::abodes::IsBuilt(object)))
			{
				Submit(*tip);
				break;
			}
		}
		// a temple's entrance whose heart is built: mine gives 0xECB "Enter Temple" on the action button, another
		// player's gives no tooltip at all; one whose heart is not built goes on as any other object
		if (const auto entrance = worship::citadel::EntranceToolTipFor(object, PlayerNames::PLAYER_ONE);
		    entrance != worship::citadel::EntranceToolTip::NotEntrance)
		{
			if (entrance == worship::citadel::EntranceToolTip::Enter)
			{
				help::tooltips::Submit(0xECB, k_ActionRight, 0, false);
			}
			break;
		}
		// a storage pit: only a built one, 0xEF9 with its food and wood
		if (registry.AllOf<StoragePit>(object))
		{
			if (const auto tip = hand_tooltips::StoragePitToolTip(
			        ecs::abodes::IsBuilt(object), ecs::StoragePitStore::GetResource(object, ResourceType::Food),
			        ecs::StoragePitStore::GetResource(object, ResourceType::Wood)))
			{
				Submit(*tip);
				break;
			}
		}
		// a town desire flag that shows: 0xE88 in its own words, with its town's desire
		if (registry.AllOf<TownDesireFlag>(object) && Locator::townDesireSystem::has_value())
		{
			if (const auto flag = Locator::townDesireSystem::value().GetFlagToolTip(object))
			{
				if (const auto tip = hand_tooltips::TownDesireFlagToolTip(flag->desire, flag->rawDesire))
				{
					Submit(*tip);
					break;
				}
			}
		}
		// (not ported) a sacrifice altar 0xEE9, a script highlight 0xEA0 / 0xE83.
		// tappable and not cannot-be-picked-up: its own tap tooltip or 0xE7A "Tap"
		const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
		if (InInfluence() && hand_tap::ValidToTap(object, is) && !ecs::object_flags::IsCannotBePickedUp(object))
		{
			// a rock: 0xEF7; the abodes keep 0xE7A
			help::tooltips::Submit(object_physics::IsRock(object) ? 0xEF7 : 0xE7A, k_ActionRight, 0, false);
			break;
		}
		SubmitLandToolTips();
		break;
	}
	case 24: // Object In Hand
	{
		// (not ported) a query icon 0xE7C; out of the influence: the land tooltips
		if (!InInfluence())
		{
			SubmitLandToolTips();
			break;
		}
		const auto held = _held ? *_held : entt::null;
		// the target under the hand: (not ported) the scaffolds 0xEA5, a creature to give to 0xE87; valid to apply the
		// held object to it: 0xE8E (0xE8D on a sacrifice altar, not ported)
		if (held != entt::null && _cursorObject && ecs::IsAvailable(*_cursorObject) && HeldValidToApplyTo(*_cursorObject))
		{
			help::tooltips::Submit(0xE8E, k_ActionRight, 0, false);
			break;
		}
		// its own drop tooltip: a tree 0xEEF "Plant", else 0xEEE "Drop"; then 0xE74 "Throw"
		const bool tree = held != entt::null && registry.AllOf<Tree>(held);
		help::tooltips::Submit(tree ? 0xEEF : 0xEEE, k_ActionRight, 0, false);
		help::tooltips::Submit(0xE74, k_ActionRight, k_ArrowsAll, false);
		break;
	}
	default: // 23 In Throw and the rows with no text
		break;
	}
}
