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

#include <optional>
#include <span>
#include <utility>

#include "Camera/Camera.h"
#include "Camera/CameraHelp.h"
#include "Camera/CameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Creature/CreatureHandRules.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fields.h"
#include "ECS/FishShoals.h"
#include "ECS/HeldApply.h"
#include "ECS/Life.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PuzzleTotemHand.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ShowNeeds.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/HandToolTipRules.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/Workshops.h"
#include "GameClock.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
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
#include "Worship/WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
// the BINDABLE_ACTION of the right button (the action button), and the align bits of the icons' arrows
using hand_tooltips::k_ActionRight;
using hand_tooltips::k_ArrowsAll;
using hand_tooltips::k_ArrowsUpDown;

void Submit(const hand_tooltips::ToolTip& tip)
{
	help::tooltips::Submit(tip.text, tip.action, tip.align, tip.force, tip.numbers);
}

/// The hints the player's camera gives where the cursor is, as it reads the mouse; none while a script's camera or a
/// camera path has the view, which give no hints
uint32_t PlayerCameraTricons()
{
	const bool cameraReadsPlayer =
	    Locator::camera::has_value() && !script_camera::Drives() &&
	    !(Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera());
	return cameraReadsPlayer ? Locator::camera::value().GetModel().GetHandCues().tricons : 0;
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

/// What the building texts read of an abode (not a feature), a citadel part (the temple's heart, a worship site), a
/// village totem or a town centre's spell icon; none for anything else
std::optional<hand_tooltips::BuildingFacts> BuildingFactsOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || registry.AllOf<ecs::components::Feature>(object))
	{
		return std::nullopt;
	}
	// the building's player must be the local one, or none: an abode is its town's, or the neutral player's without a
	// town; a field is its town's, or nobody's without one; a citadel part is its citadel's; a village totem is its
	// town centre's town's, or the neutral player's without one
	std::optional<bool> own;
	const auto ownerIsLocal = [&registry](entt::entity town) -> std::optional<bool> {
		const auto* data = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
		if (data == nullptr)
		{
			return std::nullopt;
		}
		return data->owner == PlayerNames::PLAYER_ONE;
	};
	if (ecs::fields::IsField(object))
	{
		own = ownerIsLocal(ecs::fields::TownOf(object)).value_or(true);
	}
	else if (registry.AllOf<Abode>(object))
	{
		own = ownerIsLocal(ecs::abode_villagers::TownOf(object)).value_or(false);
	}
	else if (registry.AllOf<CitadelPartBuild>(object))
	{
		if (const auto* temple = registry.TryGet<const Temple>(object); temple != nullptr)
		{
			own = temple->owner == PlayerNames::PLAYER_ONE;
		}
		else if (const auto* site = registry.TryGet<const WorshipSite>(object); site != nullptr)
		{
			own = site->player == PlayerNames::PLAYER_ONE;
		}
	}
	else if (const auto* statue = registry.TryGet<const VillageTotem>(object); statue != nullptr)
	{
		const bool centre = statue->townCentre != entt::null && registry.Valid(statue->townCentre);
		own = ownerIsLocal(centre ? ecs::abode_villagers::TownOf(statue->townCentre) : entt::null).value_or(false);
	}
	else if (registry.AllOf<SpellIcon>(object))
	{
		// a spell icon with a worship site's icon (a worship site's own, or a town centre's whose town's site has one of
		// its seed) skips the building texts
		const auto* townIcon = registry.TryGet<const TownCentreSpellIcon>(object);
		std::optional<bool> townIsLocal;
		if (townIcon != nullptr)
		{
			townIsLocal = ownerIsLocal(townIcon->town);
		}
		own = hand_tooltips::SpellIconOwnOrNoPlayer(worship::icon::WorshipIconOf(object) != entt::null, townIcon != nullptr,
		                                            townIsLocal);
	}
	if (!own)
	{
		return std::nullopt;
	}
	const auto site = ecs::abodes::GetBuildingSite(object);
	// a village totem and a spell icon are repaired while their life is whole, and a totem keeps its own percent built
	const auto* totem = registry.TryGet<const VillageTotem>(object);
	const bool repairedByLife = totem != nullptr || registry.AllOf<SpellIcon>(object);
	return hand_tooltips::BuildingFacts {
	    .built = ecs::abodes::IsBuilt(object),
	    .repaired = repairedByLife ? !(ecs::abodes::GetPercentRepaired(object) < 1.0f) : ecs::abodes::IsRepaired(object),
	    .ownOrNoPlayer = *own,
	    .pitchAllowed = (camera_help::GetEnabledFeatures() & camera_help::Bit(camera_help::Feature::Pitch)) != 0,
	    .percentBuilt = totem != nullptr ? totem->percentBuilt : ecs::abodes::GetPercentBuilt(object),
	    .percentRepaired = ecs::abodes::GetPercentRepaired(object),
	    .siteWood = site != entt::null ? std::optional(ecs::building_sites::GetWoodNeededToBuild(site)) : std::nullopt,
	};
}

/// A town centre's or a village totem's town's 0xECC then 0xEFC; none when the object is neither, or has no town
std::optional<std::array<hand_tooltips::ToolTip, 2>> TownToolTipsOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.TryGet<const Abode>(object);
	const auto* totem = registry.TryGet<const VillageTotem>(object);
	const auto centre =
	    hand_tooltips::TownCentreForToolTips(object, abode != nullptr && abode->type == AbodeNumber::TownCentre,
	                                         totem != nullptr, totem != nullptr ? totem->townCentre : entt::null);
	if (!centre || *centre == entt::null || !registry.Valid(*centre))
	{
		return std::nullopt;
	}
	const auto town = ecs::abode_villagers::TownOf(*centre);
	if (town == entt::null || !registry.AllOf<Town>(town))
	{
		return std::nullopt;
	}
	const auto& data = registry.Get<const Town>(town);
	// the belief in me minus the most belief in another player, x 1000; the adults + children, and the adult and child
	// places as they stand (the original's places move as abodes join or leave the town, not at its turn)
	const float lead = (ecs::town_belief::GetBeliefInPlayer(data.belief, PlayerNames::PLAYER_ONE) -
	                    ecs::town_belief::GetMaxBeliefMeNotIncluded(data.belief, PlayerNames::PLAYER_ONE)) *
	                   1000.0f;
	const auto room = ecs::town_stats::RoomOf(town);
	return hand_tooltips::TownToolTips(lead, data.stats.adults + data.stats.children, room.adultPlaces + room.childPlaces);
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
	if (const auto tips = TownToolTipsOf(object))
	{
		for (const auto& tip : *tips)
		{
			Submit(tip);
		}
	}
	return true;
}
/// The leash's tooltip, which the land, pick-up, locked-select and over-object tooltips ask first: with the player's
/// leash on, over the thing it is tied to "Detach Leash"; untied, over a thing it may be tied to "Attach Leash"; untied
/// over nothing "Bring Creature", which also counts the help's focus-leash event. Whatever it submits, the caller goes
/// on with its own tooltips: the help system's rules keep the one that took over
void SubmitLeashToolTip(entt::entity object)
{
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	const auto& leash = Locator::leashSystem::value();
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	hand_tooltips::LeashFacts facts {.hasObject = object != entt::null && registry.Valid(object)};
	if (const auto creature = leash.PlayersCreature(PlayerNames::PLAYER_ONE); creature.has_value())
	{
		facts.hasCreature = true;
		facts.leashOn = leash.IsLeashed(*creature);
		const auto tiedTo = facts.leashOn ? leash.TiedTo(*creature) : std::nullopt;
		facts.tied = tiedTo.has_value();
		facts.overTiedObject = tiedTo.has_value() && *tiedTo == object;
		facts.objectIsCreature = object == *creature;
		// a creature with no mind of its own counts as grown up, as the leash takes it
		const auto* mind = registry.TryGet<const CreatureMindState>(*creature);
		facts.creaturePhase = mind != nullptr ? mind->developmentPhase : CreatureMindState::k_FullyGrownUp;
	}
	if (facts.hasObject)
	{
		facts.validAsInterfaceTarget = !registry.AllOf<ScriptHighlight>(object);
		facts.orbOrSpellIcon = registry.AnyOf<OneOffSpellSeed, SpellIcon>(object);
	}
	const auto kind = hand_tooltips::LeashToolTipOf(facts);
	if (const auto tip = hand_tooltips::LeashToolTip(kind))
	{
		Submit(*tip);
	}
	if (kind == hand_tooltips::LeashToolTipKind::BringCreature)
	{
		help_profile::Trigger(help_profile::Event::FocusLeash);
	}
}
} // namespace

HandSystem::InterfaceHand HandSystem::InterfaceHandState() const noexcept
{
	// an interface that is not active: 25; inside the citadel: 30; else the row of the action state openblack's hand
	// is in
	if (!interface_active::IsActive())
	{
		return {.state = 25};
	}
	if (game_clock::IsInsideCitadel())
	{
		return {.state = 30};
	}
	if (_screenGrip)
	{
		// action state 17 BUBBLE GRIP: row 17's hand state, Drag Bubble; the row holds the cross back
		return {.state = 0x1D, .holdsCrossBack = true};
	}
	if (_gripPoint)
	{
		// action state 2 LANDSCAPE LOCK, whatever the hand holds: 20 Grip Landscape, or 22 Zoom Landscape with the action
		// button held too and the zoom allowed
		return {.state =
		            hand_tooltips::LandGripHandState(_actionHeld, static_cast<uint32_t>(camera_help::GetEnabledFeatures()))};
	}
	if (IsHoldingSeed())
	{
		return {.state = 5}; // something in the hand, a spell seed: Has Magic
	}
	if (_releaseArmed)
	{
		return {.state = 23}; // action state 12 IN THROW: row 23, In Throw
	}
	if (_creatureLock)
	{
		// the locked select on a creature: 19 Creature Interaction while held, 18 while it starts and ends. (pending) a
		// field of the creature that gives 14 instead
		// (action state 3 WAIT LOCKED SELECT's row holds the cross back)
		const bool held = _creatureLockState.phase == hand_creature::LockPhase::Held;
		return {.state = held ? 0x13 : 18, .holdsCrossBack = !held};
	}
	if (_lockedSelectAction && ecs::puzzle_totem_hand::IsPuzzleTotem(*_lockedSelectAction))
	{
		// a puzzle totem the same, by its own hold
		const bool held = ecs::puzzle_totem_hand::IsHeld(*_lockedSelectAction);
		return {.state = held ? 14 : 18, .holdsCrossBack = !held};
	}
	if (_lockedSelectAction && Locator::villageTotemSystem::has_value() &&
	    std::as_const(Locator::entitiesRegistry::value()).Valid(*_lockedSelectAction) &&
	    std::as_const(Locator::entitiesRegistry::value()).AllOf<VillageTotem>(*_lockedSelectAction))
	{
		// the locked select on a village totem: action state 4 IN LOCKED SELECT once the hand holds it, row 14 Select
		// Lock (with nothing in the hand, the TOTEM hand state); action state 3 WAIT LOCKED SELECT while it starts, 18
		const bool held = Locator::villageTotemSystem::value().IsGripped(*_lockedSelectAction);
		return {.state = held ? 14 : 18, .holdsCrossBack = !held};
	}
	if (_pickSource && _held)
	{
		return {.state = 14}; // action state 4 IN LOCKED SELECT with an object (no creature): Select Lock
	}
	if (_held)
	{
		return {.state = 24}; // something in the hand: Object In Hand
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
		// a tap-only object (an abode), a creature, a village totem (its plinth or its icon), a need sign, a town's desire
		// flag, a worship site or its altar or a temple's heart is the interface's object too
		const auto picked = AsInterfaceObject(*_cursorObject);
		if (hand_tooltips::IsInterfaceObject({
		        .totemPart = picked != *_cursorObject,
		        .tappable = hand_tap::Find(picked) != nullptr,
		        .creature = registry.AllOf<Creature>(picked),
		        .needsSign = registry.AllOf<ShowNeedsVisuals>(picked),
		        .desireFlag = registry.AllOf<TownDesireFlag>(picked),
		        .altar = registry.AllOf<WorshipTotem>(picked),
		        .templeHeart = registry.AllOf<CitadelHeart>(picked),
		        .worshipSite = registry.AllOf<WorshipSite>(picked),
		    }))
		{
			object = picked;
		}
	}
	if (!object || !ecs::IsAvailable(*object))
	{
		// nothing under the hand but a screen object: 28 Over Bubble. (not ported) its extra help call
		// nothing under the hand but a screen object holds the cross back
		return {.state = _screenObject ? 0x1C : 3, .holdsCrossBack = _screenObject.has_value()};
	}
	if (registry.AllOf<Creature>(*object))
	{
		// a creature is neither a pile nor a field and never goes into the hand: 18, in or out of the influence
		// (inferred) a creature counts as interactable and needing the influence, so it holds the cross back in it
		return {.state = 18, .holdsCrossBack = InInfluence()};
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
	const bool interactable = Interactable(*object);
	const bool inInfluence = InInfluence();
	// only a script highlight may be used out of the influence
	const bool mustBeInInfluence = !registry.AllOf<ScriptHighlight>(*object);
	// an object the hand may act on here holds the cross back
	const int32_t state = hand_tooltips::ObjectHandState({
	    .interactable = interactable,
	    .inInfluence = inInfluence,
	    .mustBeInInfluence = mustBeInInfluence,
	    .validForLockedSelect = pile || (field != nullptr && field->growth > 0.0f && field->food > 1.0f) || totem,
	    .validForPlaceInHand = ValidForPlaceInHand(*object),
	    .cannotBePickedUp = ecs::object_flags::IsCannotBePickedUp(*object),
	});
	return {.state = state, .holdsCrossBack = hand_tooltips::HoldsCrossBack(interactable, inInfluence, mustBeInInfluence)};
}

void HandSystem::SubmitLandToolTips(entt::entity object) const noexcept
{
	// the leash first. Then the camera's hints, as far as the scripts let the player use the camera; over the water in
	// the influence, a shown fish within 2 units; near the land and looking down, "Zoom Out"; else "Move"
	SubmitLeashToolTip(object);
	const auto view = help_profile::PlayerCameraView();
	Submit(hand_tooltips::LandToolTip({
	    .tricons = PlayerCameraTricons(),
	    .features = static_cast<uint32_t>(camera_help::GetEnabledFeatures()),
	    .fishInReach = _interactionPoint && !IsLand(*_interactionPoint) && InInfluence() &&
	                   ecs::FindFishFarmAt(*_interactionPoint).has_value(),
	    .centreOnLand = view.has_value() && view->screenCentreOnLand,
	    .centreDistance = view.has_value() ? view->headingDistance : 0.0f,
	    .pitch = view.has_value() ? view->pitch : 0.0f,
	}));
}

entt::entity HandSystem::ToolTipObject() const noexcept
{
	if (_hovered && ecs::IsAvailable(*_hovered))
	{
		return AsInterfaceObject(*_hovered);
	}
	return _cursorObject && ecs::IsAvailable(*_cursorObject) ? AsInterfaceObject(*_cursorObject) : entt::null;
}

std::optional<help::tooltips::Numbers> HandSystem::ToolTipNumbers(uint32_t text) const noexcept
{
	// this turn's object: a town's texts from a town centre or a village totem with a town, a storage pit's from a
	// storage pit, built or not
	const auto object = ToolTipObject();
	if (object == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	if (text == 0xEF9)
	{
		if (!Locator::entitiesRegistry::value().AllOf<StoragePit>(object))
		{
			return std::nullopt;
		}
		const auto tip = hand_tooltips::StoragePitToolTip(true, ecs::StoragePitStore::GetResource(object, ResourceType::Food),
		                                                  ecs::StoragePitStore::GetResource(object, ResourceType::Wood));
		return tip ? std::optional(tip->numbers) : std::nullopt;
	}
	const auto tips = TownToolTipsOf(object);
	if (!tips)
	{
		return std::nullopt;
	}
	return text == 0xECC ? (*tips)[0].numbers : (*tips)[1].numbers;
}

void HandSystem::SubmitToolTips() noexcept
{
	// the row of the hand state in the tooltip table, as InterfaceHandState left it this turn
	const auto& registry = Locator::entitiesRegistry::value();
	const int32_t state = _interfaceHandState;
	const auto object = ToolTipObject();
	switch (state)
	{
	case 3:  // Normal
	case 20: // Grip Landscape (its row's align 0xF00 is overwritten by the land tooltips)
		SubmitLandToolTips(object);
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
			SubmitLandToolTips(object);
		}
		else if (choice.shows == hand_tooltips::Shows::Own)
		{
			Submit(choice.tip);
		}
		break;
	}
	case 9: // Can Pick up
	{
		// the land tooltips while the script's interface interaction limits the interface; then an object in the
		// physics (InPhysics: a moving body, a carried object or one left hanging where a hand let go of it) at an
		// altitude of at least nothing: 0xE80 "Catch"
		const bool inPhysics = registry.Valid(object) && registry.AllOf<ecs::components::InPhysics>(object);
		const auto* place = inPhysics ? registry.TryGet<const ecs::components::Transform>(object) : nullptr;
		const auto first =
		    hand_tooltips::CanPickUpFirst(interface_active::GetFlags(), inPhysics, place != nullptr ? place->position.y : 0.0f);
		if (first.has_value())
		{
			if (first->shows == hand_tooltips::Shows::Land)
			{
				SubmitLandToolTips(object);
			}
			else
			{
				Submit(first->tip);
			}
			break;
		}
		// cannot be picked up: nothing; then the leash
		if (ecs::object_flags::IsCannotBePickedUp(object))
		{
			break;
		}
		SubmitLeashToolTip(object);
		// a one-shot orb: only its own pick-up tooltip, forced, and nothing else
		if (const auto* orb = registry.TryGet<const OneOffSpellSeed>(object); orb != nullptr)
		{
			const auto text = magic::OneOffPickUpToolTip(Locator::infoConstants::value(), orb->seedType, orb->powerUp);
			Submit(hand_tooltips::OrbPickUpToolTip(static_cast<uint32_t>(text)));
			break;
		}
		// 0xE73 "Pick Up"; a scaffold, the one class that has its own, says its value
		Submit(hand_tooltips::PickUpToolTip(ecs::scaffolds::IsScaffold(object) ? std::optional(ecs::scaffolds::GetValue(object))
		                                                                       : std::nullopt));
		// a second submit when it can also be tapped: its own tap tooltip or 0xE7A "Tap"; a rock gives 0xEF7 "Tap to
		// Break"
		const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
		if (hand_tap::ValidToTap(object, is))
		{
			Submit(hand_tooltips::TapToolTip(object_physics::IsRockObject(object), registry.AllOf<Reward>(object)));
		}
		break;
	}
	case 13: // Can Select Lock
	{
		// the leash first. A town centre or a village totem: its town's 0xECC / 0xEFC (the believers, the population)
		SubmitLeashToolTip(object);
		if (SubmitTownToolTips(object))
		{
			break;
		}
		// locked by someone: the land tooltips; openblack locks only the hand's own source
		if (_pickSource && *_pickSource == object)
		{
			SubmitLandToolTips(object);
			break;
		}
		// not locked: its own interact tooltip, else 0xE85 "Interact"
		if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr)
		{
			// a pot: a food pile ? 0xEFD "Food Amount" : 0xEFE "Wood Amount", with the pile's resource
			const bool food =
			    Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).resourceType == ResourceType::Food;
			Submit(hand_tooltips::PileToolTip(food, pot->amount));
		}
		else if (registry.AllOf<Field>(object))
		{
			Submit(hand_tooltips::FieldToolTip());
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
		// the leash first. A building that is not whole, the local player's: the wood it wants and how far it is built,
		// in or out of the influence
		SubmitLeashToolTip(object);
		if (const auto facts = BuildingFactsOf(object))
		{
			if (const auto tips = hand_tooltips::BuildingToolTips(*facts))
			{
				for (const auto& tip : *tips)
				{
					Submit(tip);
				}
				break;
			}
		}
		// a need sign, in or out of the influence: its row's text. A temple: its health. A town centre or a village
		// totem: its town's 0xECC then 0xEFC with no button, before the influence test
		if (const auto* sign = registry.TryGet<const ShowNeedsVisuals>(object); sign != nullptr)
		{
			const bool workshop = sign->infoIndex == ecs::show_needs::k_WorkshopInfo && ecs::workshops::IsWorkshop(sign->owner);
			Submit(hand_tooltips::NeedsSignToolTip(sign->infoIndex, sign->height.value,
			                                       workshop ? ecs::workshops::WoodWanted(sign->owner) : 0));
			break;
		}
		if (registry.AllOf<CitadelHeart>(object))
		{
			Submit(hand_tooltips::TempleHealthToolTip(ecs::life::LifeOf(object)));
			break;
		}
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
				SubmitLandToolTips(object);
			}
			break;
		}
		// out of the influence: the land tooltips, except over a script highlight, which is used out of it too
		const auto* highlight = registry.TryGet<const ScriptHighlight>(object);
		if (hand_tooltips::OverObjectShowsLand(InInfluence(), highlight == nullptr))
		{
			SubmitLandToolTips(object);
			break;
		}
		// a spell icon (a worship site's or a town centre's): its seed's tooltip, forced, with the charge of the worship
		// site's icon of that seed after it, and nothing else (none when the seed has no tooltip)
		if (const auto* icon = registry.TryGet<const SpellIcon>(object); icon != nullptr)
		{
			const auto& seed = Locator::infoConstants::value().spellSeed.at(static_cast<size_t>(icon->seedType));
			std::optional<double> charge;
			if (const auto worshipIcon = worship::icon::WorshipIconOf(object); worshipIcon != entt::null)
			{
				charge = hand_tooltips::SpellIconChargePercent(registry.Get<const WorshipSpellIcon>(worshipIcon).chantStore,
				                                               worship::icon::GetChantRequired(worshipIcon));
			}
			Submit(hand_tooltips::SpellIconToolTip(static_cast<uint32_t>(seed.tooltip), charge));
			break;
		}
		// a workshop: only a built one, 0xEF3, with how far its scaffold has got when it is repaired; one being built goes
		// on as any other object
		if (ecs::workshops::IsWorkshop(object))
		{
			const auto* info = ecs::abodes::InfoOf(object);
			const auto* workshop = registry.TryGet<const Workshop>(object);
			if (const auto tip =
			        hand_tooltips::WorkshopToolTip(ecs::abodes::IsBuilt(object), ecs::abodes::IsRepaired(object),
			                                       workshop != nullptr ? workshop->countdown : 0,
			                                       info != nullptr ? info->timeEachMobileObjectTakesToProduce : 0.0f))
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
		// the altar of a worship site: 0xEE9 with the prayer power its site stores. An altar is always made with its site;
		// one without would show nothing of its own
		if (const auto* altar = registry.TryGet<const WorshipTotem>(object); altar != nullptr)
		{
			const auto* site = registry.Valid(altar->site) ? registry.TryGet<const WorshipSite>(altar->site) : nullptr;
			if (site != nullptr)
			{
				Submit(hand_tooltips::PrayerEnergyToolTip(site->battery));
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
		// a script highlight (a challenge scroll or a did-you-know sign): 0xEA0 "Reminder" once lit, else 0xE83
		// "Activate", whether or not it can be tapped
		if (highlight != nullptr)
		{
			Submit(hand_tooltips::ScriptHighlightToolTip(highlight->active));
			break;
		}
		// tappable and not cannot-be-picked-up: its own tap tooltip or 0xE7A "Tap"
		const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
		if (InInfluence() && hand_tap::ValidToTap(object, is) && !ecs::object_flags::IsCannotBePickedUp(object))
		{
			// a rock: 0xEF7; the abodes keep 0xE7A
			Submit(hand_tooltips::TapToolTip(object_physics::IsRockObject(object), registry.AllOf<Reward>(object)));
			break;
		}
		SubmitLandToolTips(object);
		break;
	}
	case 24: // Object In Hand
	{
		// the facts in the original's order: with two scaffolds only whether they join, else the gift and, without one,
		// the apply; then the held object's own drop text
		const auto held = _held ? *_held : entt::null;
		hand_tooltips::HeldFacts facts {
		    .inInfluence = InInfluence(),
		    .overTarget = held != entt::null && _cursorObject && ecs::IsAvailable(*_cursorObject),
		};
		if (facts.inInfluence && facts.overTarget)
		{
			facts.bothScaffolds = ecs::scaffolds::IsScaffold(*_cursorObject) && ecs::scaffolds::IsScaffold(held);
			if (facts.bothScaffolds)
			{
				facts.scaffoldsJoin = ecs::scaffolds::ValidToApplyThisToObject(held, *_cursorObject);
			}
			else
			{
				facts.takesGift = GiveVerdict(*_cursorObject, held) == creature_giving::Verdict::Give;
				facts.validToApply = !facts.takesGift && HeldValidToApplyTo(*_cursorObject);
				facts.overSacrificeAltar = ecs::held_apply::IsSacrificeAltar(*_cursorObject);
			}
		}
		if (held != entt::null)
		{
			const auto scaffoldDrop = ecs::scaffolds::IsScaffold(held)
			                              ? std::optional(ecs::scaffolds::GetOverwriteDropToolTip(held))
			                              : std::optional<uint32_t> {};
			facts.ownDrop = hand_tooltips::HeldDropText(registry.AllOf<Tree>(held), scaffoldDrop);
		}
		const auto choice = hand_tooltips::HeldToolTips(facts);
		if (choice.shows == hand_tooltips::Shows::Land)
		{
			SubmitLandToolTips(object);
			break;
		}
		for (const auto& tip : std::span(choice.tips).first(choice.count))
		{
			Submit(tip);
		}
		break;
	}
	case 22: // Zoom Landscape: the row's 6 / 0xEFA / 0x300
		Submit(hand_tooltips::ZoomLandscapeToolTip());
		break;
	case 0x1C: // Over Bubble and Bubble Grip: the rows' 2 / 0xE84 / 0x300
	case 0x1D:
		Submit(hand_tooltips::BubbleToolTip());
		break;
	default: // 23 In Throw and the rows with no text
		break;
	}
}
