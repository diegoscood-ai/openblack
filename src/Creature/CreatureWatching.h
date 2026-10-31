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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Creature/CreatureDesires.h"
#include "Enums.h"

/// How a creature learns by watching. Grown up enough, it learns the ordinary skills it sees villagers practise (building,
/// fishing, dancing and the like) once it has watched one for a few seconds, and the miracles it sees cast once it has
/// seen each enough times: some species need to see a miracle more often than others, and the learning leash makes each
/// sighting count three times. It can also copy the player: noticing something the player did, then doing it itself,
/// then for some things wanting what the player seemed to want.
namespace openblack::creature_watching
{
/// An ordinary skill the creature can learn by watching, from the game's table
struct SkillRule
{
	std::string name;
	/// Seconds it must watch, from first seeing it
	float watchSeconds {6.0f};
	uint32_t minPhase {0};
};
/// A miracle it can learn by seeing it cast, from the game's table
struct MiracleRule
{
	std::string name;
	/// Times it must be seen, before the species' multiplier
	uint32_t timesToSee {10};
	uint32_t minPhase {8};
	/// Only a filter the computer players use choosing what to teach their creatures: it makes nothing known
	bool knownAtStart {false};
	/// The creature action it lets the creature take
	uint32_t action {0};
};

/// How often it has seen something, and when (the first sighting of a skill, the last of a miracle), in game turns; a
/// turn of 0 counts as never seen
struct Sighting
{
	uint32_t count {0};
	std::optional<uint32_t> turn;
};

struct Knowledge
{
	std::vector<Sighting> skillsSeen;
	std::vector<Sighting> miraclesSeen;
	std::vector<bool> skillsKnown;
	/// The miracles it knows about: seen at least once when old enough and knowing what each needs first
	std::vector<bool> miraclesKnown;
};
/// Nothing seen or known
[[nodiscard]] Knowledge StartKnowledge(std::span<const SkillRule> skills, std::span<const MiracleRule> miracles);

/// A miracle counts again only once more than this many turns have gone since it was last seen
constexpr uint32_t k_MiracleSightingTurns = 50;

/// What a sighting of a miracle tells the creature, which it shows
enum class LearningEvent : uint8_t
{
	/// Too young to learn the miracle yet
	TooYoung,
	/// Three quarters of the way to learning it or more
	NearlyLearnt,
	/// Seen often enough to learn it, which it is told again each time it sees it after
	Learnt,
};

/// How a sighting went: whether it is now learnt, and how far towards learning it the creature is, 0 to 1
struct Progress
{
	bool learnt {false};
	float share {0.0f};
	/// Too young to learn it, a skill already known, or a miracle whose prerequisite it does not know yet
	bool ignored {false};
	/// What it shows of the sighting, if anything, and the learning meter's new reading, if it moves
	std::optional<LearningEvent> event;
	std::optional<float> meter;
};

/// What a creature must know before seeing a miracle teaches it anything: an ordinary skill or another miracle
struct Prerequisite
{
	enum class Kind : uint8_t
	{
		Skill,
		Miracle,
	};
	Kind kind;
	size_t index;
};
/// A power-up teaches nothing until the miracle it powers up is known about (the second power-up of the explosion needs
/// the first), the storm with lightning needs the storm and the tornado that; the thirst and itch spells need the skill
/// of building. Every other miracle has none.
[[nodiscard]] std::optional<Prerequisite> MiraclePrerequisite(size_t miracle);
/// Watching a skill being practised
[[nodiscard]] Progress SeeSkill(Knowledge& knowledge, size_t skill, std::span<const SkillRule> rules, uint32_t phase,
                                uint32_t turn, float turnsPerSecond);
/// A creature learns from the villagers it sees from this stage of growing up on
constexpr uint32_t k_MinVillagerWatchPhase = 3;
/// A villager on its way to the storage pit for food shows nothing while more than this many metres from where it goes
constexpr float k_FarFromStoragePitMetres = 40.0f;
/// Whether a creature that sees a villager again may learn its skill: grown up enough, not under a script's control,
/// and with no plan, or a plan for none of its own needs and feelings (impressing, compassion, anger, playing, hunger,
/// fear, pooing, tiredness, puking, water, health, friends, warmth, cold, scratching, running from its god, rest,
/// illness, obeying another creature or stealing)
[[nodiscard]] bool WatchesVillagers(uint32_t phase, bool controlledByScript,
                                    std::optional<creature_desires::Desire> planDesire);
/// Whether a villager of a town of that player, seen again by a creature of that player free to learn from it, makes it
/// wish more to impress the villagers: a town of another player, or a neutral one
[[nodiscard]] bool RaisesWishToImpress(PlayerNames townOwner, PlayerNames creatureOwner);
/// The state whose skill a villager shows: its top state; while it waits for an animation, the state it goes on to; on
/// its way to the storage pit for food more than 40 m from where it goes, deciding what to do (which shows nothing)
[[nodiscard]] VillagerStates StateShown(VillagerStates top, VillagerStates final, float metresToGoal);
/// The times a miracle must be seen by a species, by its multiplier, not rounded: the product is rounded once to a float,
/// as the game's single-precision arithmetic does
[[nodiscard]] float TimesNeeded(uint32_t timesToSee, float speciesMultiplier);
/// Seeing a miracle cast; a sighting more than 50 turns after the last, or the first, counts the weight given (three
/// times on the learning leash), and every sighting is remembered as the last. Once it has its prerequisite and is old
/// enough, the creature knows about the miracle from its first sighting, and it goes on counting sightings after it has
/// learnt it.
[[nodiscard]] Progress SeeMiracle(Knowledge& knowledge, size_t miracle, std::span<const MiracleRule> rules, uint32_t phase,
                                  uint32_t turn, uint32_t weight, float speciesMultiplier);

/// The sightings a miracle a script teaches counts as: the times the species must see it (TimesNeeded), the fraction
/// dropped
[[nodiscard]] uint32_t TaughtSightings(uint32_t timesToSee, float speciesMultiplier);
/// Whether the creature knows a skill (kind CreatureActionLearningType::Normal, by its row) or a miracle (::Magic, by its
/// magic type); false for any other kind or a row out of range
[[nodiscard]] bool Knows(const Knowledge& knowledge, uint32_t kind, uint32_t action);
/// A script makes the creature know a skill or a miracle, or no longer know it: kind CreatureActionLearningType::Normal
/// is an ordinary skill by its row in the game's table, ::Magic a miracle by its magic type. A miracle taught also counts
/// as seen TaughtSightings times, unless `countsSightings` is false (the script's learning of everything, which only
/// makes it known about); one taken away keeps the sightings it had. Nothing else changes: not when it last saw it, not
/// how long it has watched a skill. False, and nothing changed, for any other kind or a row out of range
bool SetKnows(Knowledge& knowledge, uint32_t kind, uint32_t action, bool knows, std::span<const MiracleRule> miracles,
              float speciesMultiplier, bool countsSightings = true);

/// Something the player does that a creature might copy, from the game's table
struct MimicRule
{
	std::string name;
	/// How the deed ranks, 0 to 1: one ranked 0 is never copied, and a deed ranked lower does not take over one copied
	float chance {0.0f};
	/// Only on the learning leash
	bool needsLearningLeash {true};
	/// The action it notices with, the actions it may copy with, the desire it may then want, and how long each stage
	/// lasts, in steps
	uint32_t noticeAction {0};
	std::vector<uint32_t> copyActions;
	uint32_t desire {0};
	uint32_t stageSteps {1};
	bool copiesDesire {false};
};

enum class MimicStage : uint8_t
{
	Notice,
	CopyAction,
	CopyDesire,
};
[[nodiscard]] const char* Name(MimicStage stage);

struct Mimicry
{
	size_t rule {0};
	MimicStage stage {MimicStage::Notice};
	uint32_t stepsLeft {0};
	/// What the player did it to, by entity number
	std::optional<uint32_t> object;
};

/// What the creature's state allows when the player does something it might copy
struct MimicConditions
{
	uint32_t phase {0};
	bool learningLeashInHand {false};
	/// How pressing what it reacts to now is
	float reactionPriority {0.0f};
	bool canSee {true};
};
/// Whether the creature may start to copy a deed. Its state must allow it, and the deed's rank must be above 0 when it
/// copies nothing; while it copies, it may start only when it has reached copying the desire, or when the new deed is
/// another one that ranks at least as high. Nothing is rolled: the rank only orders the deeds. It changes nothing
[[nodiscard]] bool MayStartMimicry(const std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                   const MimicConditions& conditions);
/// Starts to copy a deed with no test: the creature notices it first
void BeginMimicry(std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                  std::optional<uint32_t> object);
/// Starts copying a player's action when MayStartMimicry allows it
[[nodiscard]] bool StartMimicry(std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                const MimicConditions& conditions, std::optional<uint32_t> object);
/// One step of copying: when a stage's steps run out it moves to the next, the desire stage only for rules that have
/// it, then ends; random(n) is from 0 to n - 1
void StepMimicry(std::optional<Mimicry>& mimicry, std::span<const MimicRule> rules,
                 const std::function<uint32_t(uint32_t)>& random);
/// Stroked while copying, it goes straight to doing the thing itself
void StrokedWhileMimicking(std::optional<Mimicry>& mimicry, std::span<const MimicRule> rules);

/// The most pressing reaction that still leaves the creature free to copy, and the stage of growing up it needs
constexpr float k_MaxReactionPriorityToMimic = 150.0f;
constexpr uint32_t k_MinMimicPhase = 3;
/// A creature copies only a deed nearer, in metres, than the count of map cells its eyes sweep. The game compares the
/// metres with the count of cells as it is.
[[nodiscard]] bool WithinCopyReach(float distance, uint32_t sweepCells);

/// What a deed done with a miracle does before the creature starts to copy it
enum class CastDeedStep : uint8_t
{
	/// It has seen the miracle often enough: it starts to copy at once
	Copy,
	/// It counts a sighting of the miracle and then starts to copy one time in two
	CountThenRoll,
	/// It counts a sighting of the miracle and does not copy
	CountAndStop,
};
/// The sightings still needed are the times the species needs to see the miracle (a float) less the sightings so far, cut to a
/// whole number towards zero. More than one still needed: count and stop. Exactly one: count, then copy one time in two.
/// None: copy.
[[nodiscard]] CastDeedStep CastDeedGate(uint32_t sightings, uint32_t timesToSee, float speciesMultiplier);

} // namespace openblack::creature_watching
