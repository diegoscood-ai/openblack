/******************************************************************************
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

#include "3D/MapCoords.h"

namespace openblack::virtual_influence
{

/// How far round the hand, in metres, the player keeps their influence at full strength; less as it wanes
constexpr float k_RadiusPerFraction = 10.0f;

/// What the hand keeps of its player's influence once it reaches past the border: a strength from 0 to 1 that wanes
/// the further the hand goes from the last place it was in influence and the longer it stays out, and comes back a
/// tenth a turn while the hand is in influence again. Outside, a hum plays at the strength's share of its pitch.
/// It belongs to the hand's interface status, made once with it and kept from one land to the next.
struct State
{
	/// The last place the hand was in its player's influence. None yet (and no hum) while its x is 0, as the game
	/// tests it
	map_coords::MapCoords anchor {};
	/// Where the hand was at the last game turn it was looked at while switched on
	map_coords::MapCoords turnHand {};
	/// The game turn the hand was last in influence
	uint32_t lastInsideTurn {0};
	/// The strength left, 0 to 1
	float fraction {1.0f};
	/// The strength when the hum began
	float soundFraction {1.0f};
	/// The hum has been started and not stopped
	bool soundStarted {false};
	/// Turned off by a script: it neither changes nor sounds
	bool disabled {false};
	/// Where the mana path the hand lets out leads back to, worked out on the first frame it shows after the hand was
	/// last in influence. Not worked out yet while its x is 0
	map_coords::MapCoords manaPathStart {};
};

/// The milliseconds of the frames shown, weighed by the strength, towards the next spark of the mana path. One count
/// for the whole game, whichever hand lets the sparks out
struct ManaPathClock
{
	float emission {0.0f};
};

/// The citadel's settings of the game's information tables
struct Settings
{
	/// How far, in metres, the strength reaches before the player's influence power lengthens it
	float maxDistance;
	/// How many turns out the strength lasts before the player's influence power lengthens it
	uint32_t maxTurns;
	/// Chants of worship that make the strength last twice as long
	float chantsToDouble;
};

/// What a turn finds about the hand
struct TurnInputs
{
	/// The hand's place as its interface status has it at the turn
	map_coords::MapCoords hand;
	uint32_t turn;
	/// The hand is inside an anti-influence ring of its player
	bool handShielded;
	/// The hand is in its player's own influence, not counting this
	bool handInInfluence;
	/// The player's influence power: the reach of their citadel, towns and influence rings
	float influencePower;
	/// The chants spared by the player's worship sites
	float chants;
};

/// A script switches it on or off; switched off it loses all its strength
void SetEnabled(State& state, bool enabled);

/// The strength as the questions use it: never more than full
[[nodiscard]] float Strength(const State& state);

/// Whether a place counts as in the player's influence through what the hand keeps: within the strength's share of ten
/// metres of where it is measured from, between map positions, and then as much as the strength. None past that, or
/// with no strength left (switching it off takes the strength away).
[[nodiscard]] std::optional<float> Grant(const State& state, const map_coords::MapCoords& from,
                                         const map_coords::MapCoords& place);

/// Where a question is measured from: the interface's action point for the player's own interface, when the question
/// is the interface's or the game is not paused; the hand's place at the turn otherwise
[[nodiscard]] bool MeasuresFromActionPoint(bool localInterface, bool interfaceQuestion, bool paused);

/// Each turn, unless switched off: the hand's place is noted; in influence the hand marks the place and gets a tenth of
/// its strength back; out of it the strength drops by how far it is from that place and how long it has been out, a
/// strength under a tenth going at once; and inside an anti-influence ring it has none.
void ProcessTurn(State& state, const TurnInputs& inputs, const Settings& settings);

/// Whether the hum plays this frame: the hand has strength left and is out of its player's influence, unshielded, while
/// the last place it was in is still in influence and unshielded
[[nodiscard]] bool HumPlays(const State& state, bool handShielded, bool handInInfluence, bool anchorShielded,
                            bool anchorInInfluence);

/// Where the mana path leads back to: halfway from the hand to the last place it was in influence, as map positions
/// rounded to the nearest step (a half to the even one), and the altitude halfway between theirs
[[nodiscard]] map_coords::MapCoords ManaPathStart(const map_coords::MapCoords& hand, const map_coords::MapCoords& anchor);

/// A frame shown: its milliseconds weighed by the strength count towards a spark, one once they pass ten. When one is
/// due, the share of 256 its colour is scaled by: the strength out of 255.
[[nodiscard]] std::optional<uint8_t> EmitManaPath(ManaPathClock& clock, float strength, float frameMilliseconds);

/// A colour 0xAARRGGBB with each of its four channels scaled by a share of 256
[[nodiscard]] uint32_t ScaleColour(uint32_t argb, uint8_t scale);

/// The hum's pitch in percent: its strength's share of its normal pitch, held to what it began at
[[nodiscard]] uint32_t HumPitchPercent(const State& state);

} // namespace openblack::virtual_influence
