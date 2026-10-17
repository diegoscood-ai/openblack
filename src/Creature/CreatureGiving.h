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
#include <numbers>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"
#include "Enums.h"

/// The player's hand giving what it holds to a creature: the hand offers it while over a creature that would take it,
/// turning it over towards the creature; a press of the Action button gives it; the creature turns to the player,
/// reaches out and takes it from the hand, which is drawn to meet the creature's hand; then the creature decides by its
/// desires what to do with what it was given.
namespace openblack::creature_giving
{
/// Over a creature that would take what the hand holds, the hand offers it for this long after leaving the creature
constexpr int32_t k_OfferMs = 1100;
/// The held thing turns back once less than this much of the offer is left, unless the creature is reaching for it
constexpr int32_t k_OfferTurnsBackMs = 300;
/// How long the held thing takes to turn over, and back
constexpr float k_OfferTurnSeconds = 0.4f;
/// Within this many milliseconds of the moment the creature takes it, the hand is drawn to the creature's hand, this
/// much of the way for each millisecond nearer
constexpr float k_DrawMs = 500.0f;
constexpr float k_DrawPerMs = 0.002f;
/// Moving the hand further than this many times the creature's size from where it should be calls the giving off
constexpr float k_CallOffSizes = 15.0f;
/// The animation the creature takes a thing from the hand with
constexpr size_t k_TakeAnimation = 86;
/// The desires weighed for what to do with a thing given, below which none counts
constexpr float k_LeastDesire = 0.1f;
/// The creature's help raised when it couldn't pick up what it is offered: a stacked miscellaneous message of this
/// number
constexpr uint32_t k_CantPickUpHelp = 7;

/// The offer, counted down on the frame's real time
struct Offer
{
	int32_t ms {0};
	/// The hand is over a creature that would take what it holds: it offers it for the full time again
	void Renew() { ms = k_OfferMs; }
	/// A frame without a creature that would take it: the offer runs down, unless the creature is already reaching for
	/// the thing. Whether it ran out this frame.
	bool RunDown(int32_t frameMs, bool reaching);
	[[nodiscard]] bool Active() const { return ms > 0; }
};

/// How far round the held thing turns while it is offered: the palm turns over, a thing held at the side or by the
/// fingers a quarter turn, the other way for the right hand; none when the offer is nearly over and the creature isn't
/// reaching. None when the hold keeps its last turn (magic, grain, fingers).
[[nodiscard]] std::optional<float> OfferTurn(HoldType hold, bool rightHanded, const Offer& offer, bool reaching);

/// Whether a thing can be given to a creature
enum class Verdict : uint8_t
{
	Give,
	/// Not to this creature: another god's not allied to it, a spell seed, or a creature without a player
	No,
	/// The creature couldn't pick it up; its help says so
	CantPickUp,
};
struct GiveFacts
{
	bool creatureHasPlayer {false};
	bool giverHasPlayer {false};
	/// The giver is the creature's own player, or allied to it
	bool friendly {false};
	bool spellSeed {false};
	bool creatureCanPickUp {false};
};
[[nodiscard]] Verdict MayGive(const GiveFacts& facts);

/// Where the hand is drawn while the creature reaches, before it rises for what it holds: within half a second of the
/// moment of taking, from the creature's hand towards where the hand should be, the further the more milliseconds from
/// that moment, and lowered by the rest of the way times how far what it holds hangs below it. None further away.
[[nodiscard]] std::optional<glm::vec3> DrawToCreature(glm::vec3 creatureHand, glm::vec3 target, float msFromTake, float hang);

/// Whether the hand has gone too far from the creature reaching for what it holds
[[nodiscard]] bool CallsOff(glm::vec3 hand, glm::vec3 target, float creatureSize);

/// What the hand keeps of its giving from frame to frame, while it holds something to give
struct HandGiving
{
	/// The creature it offers what it holds to while the offer lasts, and the creature it gave it to while that creature
	/// reaches for it
	entt::entity offeredTo {entt::null};
	Offer offer;
	entt::entity givenTo {entt::null};
	/// How far round the offered thing is turned
	Zoomer turn;
	/// Where the hand was drawn last frame
	std::optional<glm::vec3> lastHand;
	/// The button is held down since a press while the hand held the thing: held long enough it gives the thing. The
	/// press's sample time and turn
	bool givePress {false};
	uint32_t pressMs {0};
	uint32_t pressTurn {0};
	/// The turn the giving was sent in, until the creature has it to take: the turn after takes it
	std::optional<uint32_t> sentTurn;

	/// The offer and the giving are over
	void Forget();
	/// The hand holds nothing to give: all is as before it held anything
	void Clear();
	[[nodiscard]] bool Reaching() const { return givenTo != entt::null; }
	/// The creature it offers what it holds to, while it does; the creature reaching for it once given
	[[nodiscard]] std::optional<entt::entity> Offered() const;
};

/// What the hand finds in a frame it holds something to give
struct OfferFacts
{
	/// The creature it gave the thing to is still about taking it: turning to the player or reaching for it
	bool stillTaking {false};
	/// A thing under the hand (not what it holds), and whether what it holds can be given to it: none when it isn't a
	/// creature
	std::optional<entt::entity> under;
	std::optional<Verdict> verdict;
	/// The frame's real time
	int32_t frameMs {0};
	HoldType hold {HoldType::None};
	bool rightHanded {false};
};
/// Each frame the hand holds something: the giving is over once the creature no longer takes it; the offer to a
/// creature under the hand that would take it lasts its full time again, otherwise it runs down; and the held thing
/// turns over to offer it, and back. Whether the creature under the hand couldn't pick the thing up, which its help is
/// to say.
bool KeepOffer(HandGiving& giving, const OfferFacts& facts);

/// The creature a press gives to: the one the hand offers to while the offer lasts and it is still there, else the
/// thing under the hand; none once given
[[nodiscard]] std::optional<entt::entity> GiveTarget(const HandGiving& giving, bool offeredStillThere,
                                                     std::optional<entt::entity> under);
/// It was given to the creature, which reaches for it until it has it or gives up
void Given(HandGiving& giving, entt::entity creature);

/// What the hand finds in a frame the creature reaches for what it was given
struct ReachFacts
{
	/// Where the hand should be, before it rises for what it holds
	glm::vec3 target {0.0f};
	/// Where the creature's hand takes the thing, turned to face the hand as it was last drawn; none when it can't be
	/// told
	std::optional<glm::vec3> takePoint;
	float creatureSize {1.0f};
	/// What the creature reaches for and how many milliseconds its animation is from taking it, none before it reaches
	/// out
	std::optional<entt::entity> taking;
	float msFromTake {0.0f};
	entt::entity held {entt::null};
	/// How far what the hand holds hangs below it
	float hang {0.0f};
};
struct Reach
{
	/// Where the hand is drawn, before it rises for what it holds
	glm::vec3 hand {0.0f};
	/// The hand went too far from the creature's hand: the creature is to give up taking the thing
	bool calledOff {false};
};
/// While the creature reaches, the hand meets its hand at the moment it takes the thing; moved too far from the
/// creature's hand, the giving is called off
[[nodiscard]] Reach ReachFrame(HandGiving& giving, const ReachFacts& facts);

/// What a creature does with a thing it was given
enum class Use : uint8_t
{
	Examine,
	Eat,
	Throw,
	/// What it would do led to it on the leash
	AsLed,
};
/// The desires it weighs, in the game's order: curiosity examines, hunger eats, wanting to play with the player throws
struct Weighed
{
	float desire {0.0f};
	float usefulness {0.0f};
};
struct GiftFacts
{
	Weighed curiosity;
	Weighed hunger;
	Weighed playWithPlayer;
	/// It has learnt it can't eat things of this kind
	bool cantEatKind {false};
	/// It has been given a thing of this kind before
	bool givenKindBefore {false};
};
/// The use chosen, and whether it is the first of the kind it was given with no desire for it, which it is to remember
struct Choice
{
	Use use {Use::Examine};
	bool remembersKind {false};
};
/// The desire with the best score (its value times how useful the thing is to it) wins, hunger only for a kind it can
/// eat. With no winner, or no desire at least a tenth strong, the first thing of a kind is examined and later ones are
/// acted on as when led to them.
[[nodiscard]] Choice ChooseUse(const GiftFacts& facts);
} // namespace openblack::creature_giving
