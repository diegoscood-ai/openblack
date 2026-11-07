/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>

#include "Enums.h"
#include "Help/ToolTips.h"

// The decisions of the hand's tooltips (HandSystem::SubmitToolTips) that need no game state: what a branch submits,
// from what the caller read off the object. Wiki: docs/wiki/interface/tooltips.md#tooltips.
namespace openblack::ecs::systems::hand_tooltips
{

/// The BINDABLE_ACTION whose button the tooltip's icon shows: none, the left button, the right (action) button
constexpr int32_t k_ActionNone = -1;
constexpr int32_t k_ActionLeft = 1;
constexpr int32_t k_ActionRight = 2;
/// The zoom-in control (the mouse wheel by default), whose picture "Zoom Out" shows
constexpr int32_t k_ActionZoomIn = 4;
/// The align bits of the icon's arrows: up and down, and all four
constexpr uint32_t k_ArrowsUpDown = 0x300;
constexpr uint32_t k_ArrowsAll = 0xF00;

/// One submit: help::tooltips::Submit(text, action, align, force, numbers)
struct ToolTip
{
	uint32_t text {0};
	int32_t action {k_ActionNone};
	uint32_t align {0};
	bool force {false};
	help::tooltips::Numbers numbers {};
};

/// What a branch shows: nothing, the land's tooltips, or its own tooltip
enum class Shows
{
	Nothing,
	Land,
	Own,
};

struct Choice
{
	Shows shows {Shows::Nothing};
	ToolTip tip {}; ///< With Shows::Own
};

/// What the land texts read: the camera's hints where the cursor is (camera_drag::tricon, none while another camera
/// than the player's has the view), the camera controls the scripts let the player use (camera_drag::feature), and
/// whether the hand is over the water in the influence with a shown fish within reach
struct LandFacts
{
	uint32_t tricons {0};
	uint32_t features {0};
	bool fishInReach {false};
	/// The view of the player's camera (all false and 0 while another camera has it): the line through the middle of
	/// the screen meets the land, the camera's distance from that point (never below 10), and how steeply it looks down
	bool centreOnLand {false};
	float centreDistance {0.0f};
	float pitch {0.0f};
};

/// "Zoom Out" is offered with the land at the middle of the screen nearer than this, and the camera looking down more
/// steeply than k_ZoomOutPitch
constexpr float k_ZoomOutDistance = 15.0f;
constexpr float k_ZoomOutPitch = 0.55f;

/// The land texts, in the original's order: "Pitch" (0xE76) on the left button with the up and down arrows where a
/// drag tilts the camera, then "Rotate" (0xE77) with all four arrows where it turns it, each only while the scripts let
/// the player do it; then 0xE73 "Pick Up" on the action button for a fish; then "Zoom Out" (0xE8B) on the zoom-in
/// control with no arrows, near the land and looking down, while the scripts let the player zoom; else "Move" (0xE7E)
/// on the left button with all four arrows
[[nodiscard]] ToolTip LandToolTip(const LandFacts& facts);

/// A seed in the hand: out of the influence the land's tooltips; with no seed nothing. Over an object, the seed valid
/// to apply to it gives 0xE81 "Cast" on the action button, else the land's (the land under it is not tried). Over the
/// land, the seed valid to apply there gives 0xEF1 for the storms, the tornado and the shields, 0xE81 for any other
/// magic, else the land's
[[nodiscard]] Choice SeedToolTip(bool inInfluence, bool hasSeed, bool hasTarget, bool validToObject, bool validToMap,
                                 MagicType magic);

/// What the hand over a thing it can pick up shows first: the land's tooltips while the script's interface
/// interaction limits the interface (its flags hold 2, bit 1), else 0xE80 "Catch" on the action button over a thing in the
/// physics at an altitude of at least nothing; none goes on to the thing's own tooltips
[[nodiscard]] std::optional<Choice> CanPickUpFirst(uint8_t interfaceFlags, bool inPhysics, float altitude);

/// Over a thing the hand can pick up: 0xE73 "Pick Up" on the action button, but a scaffold says its value instead,
/// 0xEE7 "Scaffold Value", with the value as a whole number
[[nodiscard]] ToolTip PickUpToolTip(std::optional<uint32_t> scaffoldValue);

/// Over a thing the hand can tap: its own tap text, 0xEF7 "Tap to Break" for a rock and 0xEF8 "Tap to Open" for a
/// reward chest, else 0xE7A "Tap", on the action button
[[nodiscard]] ToolTip TapToolTip(bool rock, bool rewardChest);

/// Over a pile or a field the hand may lock its select on, its own text on the action button: a food pile 0xEFD "Food
/// Amount" and a wood pile 0xEFE "Wood Amount", each with what the pile answers for its resource (a store's pile its
/// store's whole total), widened straight to a double; a field 0xE73 "Pick Up"
[[nodiscard]] ToolTip PileToolTip(bool food, uint32_t amount);
[[nodiscard]] ToolTip FieldToolTip();

/// Over a one-shot orb that can be picked up: only its own pick-up text, with no button, forced
[[nodiscard]] ToolTip OrbPickUpToolTip(uint32_t text);

/// A spell icon's charge as its text shows it: store / required x 100, all in floats; none when it is 0 or not a number,
/// when the text is the miracle's name alone
[[nodiscard]] std::optional<double> SpellIconChargePercent(float store, float required);
/// Over a spell icon in the influence: its seed's tooltip, with no button, forced, and its charge after it when it has
/// one (a seed with no tooltip shows nothing, as help::tooltips::Submit drops a text before k_First)
[[nodiscard]] ToolTip SpellIconToolTip(uint32_t seedToolTip, std::optional<double> chargePercent);

/// How far a workshop's scaffold has got, as its text shows it: 100 - turns left x 100 / the turns a scaffold takes,
/// the turns times 100 in whole numbers and the rest in floats, cut to a whole number and taken modulo 100, so an
/// idle workshop (no turns left) shows 0
[[nodiscard]] int32_t WorkshopProductionPercent(int32_t turnsLeft, float turnsToProduce);
/// Over a workshop in the influence: only a built one shows 0xEF3, with no button; a repaired one writes how far its
/// scaffold has got into it, a damaged one shows it as it is written
[[nodiscard]] std::optional<ToolTip> WorkshopToolTip(bool built, bool repaired, int32_t turnsLeft, float turnsToProduce);

/// What the building texts read of a building: an abode or a citadel part (the temple's heart, a worship site)
struct BuildingFacts
{
	bool built {true};
	bool repaired {true};
	/// The building is the local player's, or of no player at all
	bool ownOrNoPlayer {false};
	/// The scripts let the player tilt the camera (camera_drag::feature::k_Pitch)
	bool pitchAllowed {false};
	float percentBuilt {0.0f};
	/// Its life
	float percentRepaired {0.0f};
	/// The wood its building site still wants, none without a site
	std::optional<float> siteWood;
};

/// Over a building that is not whole (not built, or built and not repaired), the local player's or of no player, while
/// the scripts let the player tilt the camera, in or out of the influence: 0xEFB "Wood Required" with the wood its site
/// wants (0 when it is not above 0), or with no site in the words and number of 0xEF4; then 0xEF4 "Building
/// Completed" with how far it is built, or once built how far it is repaired, times 100; both with no button, taking
/// turns. None otherwise, and the hand goes on to the building's other texts
[[nodiscard]] std::optional<std::array<ToolTip, 2>> BuildingToolTips(const BuildingFacts& facts);

/// The building texts' player test for a spell icon: none for one with a worship site's icon (a worship site's own, or
/// a town centre's whose town's site has an icon of the same seed), which skips the building texts; a town centre's
/// is its town's player's (`townIsLocal`), or of no player without a town; any other is the neutral player's
[[nodiscard]] std::optional<bool> SpellIconOwnOrNoPlayer(bool hasWorshipIcon, bool townCentreIcon,
                                                         std::optional<bool> townIsLocal);

/// Over a storage pit: only a built one shows 0xEF9 "Food Stored: %3.0f Wood: %3.0f", with no button, its food (as
/// a float) then its wood (the whole number as it is)
[[nodiscard]] std::optional<ToolTip> StoragePitToolTip(bool built, uint32_t food, uint32_t wood);

/// Over the altar of a worship site in the influence: 0xEE9 "Prayer Energy: %3.0f", with no button, the prayer power
/// that its site stores, read as a float
[[nodiscard]] ToolTip PrayerEnergyToolTip(float stored);

/// Over a need sign, in or out of the influence, by its row of the show needs info: the altar's hunger sign 0xECE and
/// its rest sign 0xEFF, each with how full it shows now in hundredths; its hiding sign keeps the plain 0xE73 with no
/// number; a workshop's sign 0xEFB with the wood its workshop still wants. With no button
[[nodiscard]] ToolTip NeedsSignToolTip(uint8_t infoIndex, float shown, uint32_t woodWanted);

/// Over a town desire flag that shows: 0xE88 "Need" in the flag's own words (0xECE on, one for each desire), with no
/// button, its town's raw desire in hundredths, never more than 100; none for a desire without its own words
[[nodiscard]] std::optional<ToolTip> TownDesireFlagToolTip(TownDesireInfo desire, float rawDesire);

/// The town centre whose town's texts an object shows: a town centre its own; a village totem its town centre's
/// (entt::null when it has none); nullopt for any other object
[[nodiscard]] std::optional<entt::entity> TownCentreForToolTips(entt::entity object, bool isTownCentre, bool isTotem,
                                                                entt::entity totemsTownCentre);

/// Over a town centre: 0xECC with the lead in believers (0xEE0 "Believers Needed" when it is below 0), then 0xEFC
/// with the population and "/" the places, both with no button
[[nodiscard]] std::array<ToolTip, 2> TownToolTips(float lead, uint32_t population, uint32_t places);

/// Holding a thing over a creature that would take it as a gift: 0xE87 "Give" on the action button
[[nodiscard]] ToolTip GiveToolTip();

/// Holding a scaffold over another scaffold it may join: 0xEA5 "Combine Scaffolds" on the action button
[[nodiscard]] ToolTip CombineScaffoldsToolTip();

/// What a held object shows when it is to be dropped: its own drop text (a tree's "Plant", a scaffold's), 0 for none
[[nodiscard]] uint32_t HeldDropText(bool tree, std::optional<uint32_t> scaffoldDrop);

/// Holding something with nothing to give or apply it to: its own drop text (`ownDrop`, 0 for none) or 0xEEE "Drop",
/// then 0xE74 "Throw" with all four arrows, both on the action button, taking turns
[[nodiscard]] std::array<ToolTip, 2> DropToolTips(uint32_t ownDrop);

/// What the hand's texts read with an object in it. The caller reads them in the original's order and leaves the rest
/// at their defaults: with both scaffolds only whether they join; otherwise the gift, and only without one the apply
struct HeldFacts
{
	/// The hand is in the player's influence
	bool inInfluence {false};
	/// An object is in the hand and one the hand may act on is under it
	bool overTarget {false};
	/// Both are scaffolds, and the held one may join the other
	bool bothScaffolds {false};
	bool scaffoldsJoin {false};
	/// The object under the hand would take the held one as a gift
	bool takesGift {false};
	/// The held object may be applied to the object under the hand
	bool validToApply {false};
	/// The object under the hand is a worship site's altar, where what is applied is sacrificed
	bool overSacrificeAltar {false};
	/// The held object's own drop text (HeldDropText), 0 for none
	uint32_t ownDrop {0};
};

/// What the hand shows with an object in it: the land's tooltips, or one or two texts of its own, the first `count`
struct HeldChoice
{
	Shows shows {Shows::Nothing};
	std::array<ToolTip, 2> tips {};
	size_t count {0};
};

/// The hand's texts with an object in it, in the original's order: out of the influence the land's; over a target,
/// two scaffolds that join "Combine Scaffolds" and two that do not nothing but the drop texts, else a creature that
/// takes the gift "Give", else a target the held object applies to "Sacrifice" on a worship site's altar or "Supply";
/// then its own drop text or "Drop", and
/// "Throw" (DropToolTips)
[[nodiscard]] HeldChoice HeldToolTips(const HeldFacts& facts);

/// Over a temple, in or out of the influence: 0xEE8 "Temple Health", its life times 100 (in floats), with no button
[[nodiscard]] ToolTip TempleHealthToolTip(float life);

/// Over a creature that can be interacted with: 0xE85 "Interact", with no button
[[nodiscard]] ToolTip CreatureInteractToolTip();

/// Over an object with an empty hand, out of the influence: the land's tooltips, unless interaction with the object
/// does not need the influence (a script highlight), which goes on to its own text
[[nodiscard]] bool OverObjectShowsLand(bool inInfluence, bool mustBeInInfluence);

/// Over a script highlight (a challenge scroll or a did-you-know sign), in or out of the influence: a lit one shows
/// 0xEA0 "Reminder", one not lit yet 0xE83 "Activate", both on the action button and not forced
[[nodiscard]] ToolTip ScriptHighlightToolTip(bool active);

/// What the leash's tooltip reads: the player's leash on (held in this hand) and tied, the object under the hand and
/// whether it is the one tied to, the player's creature, its development phase, and what the object is
struct LeashFacts
{
	bool leashOn {false};
	bool tied {false};
	bool hasObject {false};
	bool overTiedObject {false};
	bool hasCreature {false};
	bool objectIsCreature {false};
	uint32_t creaturePhase {0};
	/// Every object but a script highlight may be the interface's target
	bool validAsInterfaceTarget {true};
	/// A one-shot orb or a spell icon, which the leash's tooltip leaves alone
	bool orbOrSpellIcon {false};
};
/// The leash's tooltip, which every land, pick-up, locked-select and over-object tooltip asks first
enum class LeashToolTipKind : uint8_t
{
	None,
	/// Tied, over the thing tied to: "Detach Leash"
	Detach,
	/// Not tied, over a thing it may be tied to, the creature grown past its first stages: "Attach Leash"
	Attach,
	/// Not tied, over nothing: "Bring Creature", which also counts the help's focus-leash event
	BringCreature,
};
[[nodiscard]] LeashToolTipKind LeashToolTipOf(const LeashFacts& facts);
/// Its text on the action button: 0xEA3, 0xEA4 or 0xEA2; nothing for None
[[nodiscard]] std::optional<ToolTip> LeashToolTip(LeashToolTipKind kind);

/// The hand state of the land grip: 22 Zoom Landscape with the action button held too and the zoom allowed (the
/// camera's feature 4), else 20 Grip Landscape
[[nodiscard]] int32_t LandGripHandState(bool actionHeld, uint32_t features);

/// Gripping the land with the action button held too: 0xEFA "Zoom", on the zoom control's key or button (action 6),
/// with the up and down arrows
[[nodiscard]] ToolTip ZoomLandscapeToolTip();

/// Over a help bubble on the screen, or dragging one: 0xE84 "Scroll" on the action button, with the up and down arrows
[[nodiscard]] ToolTip BubbleToolTip();

/// Whether the hand's state (HAND_STATES, HandSystem::RequiredHandState) shows the camera's helper icons by the hand:
/// the empty hand (1) and the camera's (2); holding something (4) or a spell seed (8) unless the locked select it holds
/// is scooping; never the hidden hand, the tug, the totem, the creature, an animation or the temple
[[nodiscard]] bool ShowsCameraIcons(int32_t handState, bool scooping);

/// Whether the object under an empty hand holds the camera's red cross back for a while: one the hand may act on here,
/// interactable and either in the influence or not needing it
[[nodiscard]] bool HoldsCrossBack(bool interactable, bool inInfluence, bool mustBeInInfluence);

/// What the interface knows of the object under an empty hand
struct ObjectFacts
{
	bool interactable {false};      ///< Available (an abode: partly built)
	bool inInfluence {false};       ///< The hand is in the player's influence
	bool mustBeInInfluence {false}; ///< Interaction with it needs the influence (every object but a script highlight)
	bool validForLockedSelect {false};
	bool validForPlaceInHand {false};
	bool cannotBePickedUp {false};
};

/// The hand state over an object, with nothing in the hand, in the original's order: not interactable 3; out of the
/// influence when it needs it 18; valid for a locked select 13; valid to place in the hand and not cannot-be-picked-up
/// 9; else 18. (not ported) the locked apply's 15 / 16 between 13 and 9
[[nodiscard]] int32_t ObjectHandState(const ObjectFacts& facts);

/// What the object under the cursor is, for whether the hand takes it as its object when nothing else is under the
/// hand
struct CursorObjectFacts
{
	bool totemPart {false}; ///< a village totem's plinth or icon, taken as the totem
	bool tappable {false};  ///< a class with a tap
	bool creature {false};
	bool needsSign {false};
	bool desireFlag {false};  ///< a town's desire flag
	bool altar {false};       ///< a worship site's altar
	bool templeHeart {false}; ///< a temple's heart, the temple itself
	bool worshipSite {false}; ///< a worship site, built or being built
};
/// The interface's object is the drawn object under the cursor: the kinds whose hand texts are ported
[[nodiscard]] bool IsInterfaceObject(const CursorObjectFacts& facts);

/// The amount in the hand ("Amount", forced) is shown on every game turn of a scoop (of a pile, a field or a fish farm)
/// and on the one turn after it ends, while the hand holds a handful of food or wood
[[nodiscard]] constexpr bool ShowsAmountInHand(bool scooping, bool wasScooping, bool holdsHandful)
{
	return (scooping || wasScooping) && holdsHandful;
}

} // namespace openblack::ecs::systems::hand_tooltips
