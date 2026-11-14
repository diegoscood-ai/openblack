/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include "Enums.h"

/// What the player's hand takes as it picks something up or scoops from a source: whether the place-in-hand takes the
/// thing, the first handful a pile or a field gives, what each turn of scooping takes, the scooping sound and how each
/// kind of thing is held.
///
/// Pure functions, tested on their own; the hand's system gathers the facts and applies them.
namespace openblack::hand_pick_up
{

/// The checks the place-in-hand makes of a thing, in the game's order
enum class PlaceInHandCheck : uint8_t
{
	/// The thing can be acted on (not being deleted; a building once it is started)
	Interactable,
	/// The hand holds nothing yet
	HandEmpty,
	/// The thing's own kind says the hand may hold it
	Valid,
	/// No script made it unpickable
	MayBePickedUp,
	/// Nothing carries it (a tornado)
	NotCarried,
};

/// Whether the place-in-hand takes a thing: every check passes. Each check is asked only while the ones before it pass,
/// in the game's order, so `ask` is not asked about a thing that is already refused
template <typename Ask>
[[nodiscard]] bool PlaceInHandTakes(Ask&& ask)
{
	return ask(PlaceInHandCheck::Interactable) && ask(PlaceInHandCheck::HandEmpty) && ask(PlaceInHandCheck::Valid) &&
	       ask(PlaceInHandCheck::MayBePickedUp) && ask(PlaceInHandCheck::NotCarried);
}

/// Whether a grab that has waited its time takes the thing: only inside the player's influence, and only when no script
/// made it unpickable. Otherwise nothing is picked up
[[nodiscard]] bool GrabTakes(bool inInfluence, bool cannotBePickedUp) noexcept;

/// The hand's own pile a source of a resource fills: the hand's wood for wood, the hand's food for anything else
[[nodiscard]] PotInfo HandPileFor(ResourceType resource) noexcept;
/// The first handful from a pile or a store: what the hand's pile takes at first, or what the source has when that is
/// less. Nothing is taken from an empty source
[[nodiscard]] uint32_t FirstHandful(uint32_t sourceHas, uint32_t takenAtFirst) noexcept;

/// A field can be scooped from while it has grown and has more than one unit of food
[[nodiscard]] bool FieldValidForLockedSelect(float growth, float food) noexcept;
/// The first handful from a field: what the hand takes from a field, or the field's food when that is less, as a whole
/// number cut towards zero, and halved (cut towards zero) when the crop is ripe
[[nodiscard]] int32_t FieldFirstHandful(float takenWithHand, float food, bool ripe) noexcept;

/// A field put straight into the hand, not scooped (a press the scooping refused): a new handful of the hand's food of
/// the field's hand amount cut towards zero, whatever food the field has. The field is then asked for that much
[[nodiscard]] int32_t FieldStraightHandful(float takenWithHand) noexcept;
/// A fish farm put straight into the hand: a new handful of the hand's food of that food's own food value cut towards
/// zero, while the farm's food value is above 0, and none otherwise. The farm loses no fish
[[nodiscard]] std::optional<int32_t> FishFarmStraightHandful(float handFoodValue, float farmFoodValue) noexcept;

/// A turn of scooping from a field: how far the scoop has ramped up (0 to 1 over 60 turns) and what it takes
struct FieldScoop
{
	float t;
	int32_t amount;
};
/// A turn of scooping from a field after some turns of it: 8 + 62 t^2 (t = turns / 60, at most 1) or the field's food
/// when that is less, cut towards zero, no more than the room left under 20000 in the hand, then halved (cut towards
/// zero) when the crop is ripe
[[nodiscard]] FieldScoop FieldTurnScoop(uint32_t turns, float food, uint32_t held, bool ripe) noexcept;

/// What a turn of scooping from a pile takes: the turn's amount, no more than the source has nor than the room left in
/// the handful under the most it can hold (no room when it holds that much or more)
[[nodiscard]] uint32_t PileScoopTaken(uint32_t amount, uint32_t sourceHas, uint32_t mostHeld, uint32_t held) noexcept;

/// The scooping sound: the wood sample for the hand's wood, the food sample for anything else (piles, fields, fish)
inline constexpr int32_t k_PickUpFoodSample = 44;
inline constexpr int32_t k_PickUpWoodSample = 98;
[[nodiscard]] int32_t PickUpSoundSample(bool wood) noexcept;
/// The scooping sound's pitch in percent: 60 + 180 x the scoop's ramp squared, cut towards zero
[[nodiscard]] int32_t PickUpSoundPitch(float rampSquared) noexcept;

/// The kinds of thing as the hand's hold tells them apart, the first that fits in this order
enum class HoldClass : uint8_t
{
	/// A standing tree or a dead tree
	Tree,
	/// A lump of poo, held on the palm
	Poo,
	/// A mobile object, a pot or a one-shot orb
	SideObject,
	/// A villager or an animal
	Living,
	/// A static that can be moved (fences, totems, stones, toys)
	MobileStatic,
	/// Anything else
	Object,
};

/// How a thing is held in the hand: its hold, the radius that opens the hand and its lowering (times its height, how
/// far it hangs below the hand)
struct HoldParameters
{
	HoldType type {HoldType::Above};
	float radius {0.0f};
	float lowering {0.0f};
};
/// How a thing that is not a miracle's seed is held, from its kind, its static's row (for a mobile static) and its
/// height and radius across the ground: on the palm by default (radius three quarters of its height, no lowering); a
/// tree by the tree grip (a fifth of its radius, 0.1); a lump of poo on the palm, a little above the hand point (-0.3); a
/// mobile object, pot or orb from the side (its radius, 0.7); a
/// villager or an animal by the villager grip (its radius, 0.65); the gate totems and weeping stones from the side
/// (0.7), the first singing stone from the side (0.4) and the cuddly toy and the skittle from the side (no lowering)
[[nodiscard]] HoldParameters HoldParametersOf(HoldClass kind, MobileStaticInfo staticType, float height,
                                              float radius2D) noexcept;

} // namespace openblack::hand_pick_up
