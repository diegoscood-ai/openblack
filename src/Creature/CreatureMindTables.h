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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/CreatureWatching.h"

namespace openblack
{
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

/// The game's tables for a creature's mind, taken from info.dat into plain values: the actions and what they do to
/// desires, which actions satisfy each desire, how feedback changes each desire, how desires depend on one another,
/// what each desire's decision trees may test, and the skills, miracles and player's deeds a creature can learn from.
namespace openblack::creature_mind_tables
{
/// The usefulness the game gives a thing a creature uses for an action, whatever the thing or the action
constexpr float k_UsingUsefulness = 0.1f;

using creature_desires::k_DesireCount;

/// An action that needs no skill names this one in its place
inline constexpr uint32_t k_NoSkillNeeded = 6;

struct ActionInfo
{
	std::string name;
	/// The desire the action serves, and what the desire is multiplied by once it is done
	uint32_t desire {0};
	float desireMultiplier {1.0f};
	/// Whether that applies however the action came about
	bool alwaysApplies {false};
	/// How many seconds after it finishes the action can still be learnt from, and whether it can be at all
	float learningWindowSeconds {0.0f};
	bool learnable {false};
	/// Stroked while holding something during it, the creature learns to eat what it holds
	bool eatWhenStroked {false};
	/// The action is done to a thing, and never to the thing the creature carries
	bool needsObjectToActOn {false};
	bool excludesCarriedObject {false};
	/// The action uses a thing to do it, which a plan of it must find (the game's own action table, not its data)
	bool needsObjectToUse {false};
	/// The desire that another creature takes the action to show
	uint32_t shownDesire {0};
	/// When the action ends well on another creature, how much nicer the creature finds the other creature
	float creatureNicenessChange {0.0f};
	/// The opinion of the action that a new creature starts with
	float startingOpinion {0.0f};
	/// The two skills the creature must know to do the action, by their rows of the skills table (6 for none), and the
	/// miracle it must know about (0 for none)
	std::array<uint32_t, 2> skillsNeeded {k_NoSkillNeeded, k_NoSkillNeeded};
	uint32_t miracleNeeded {0};
	/// Carried out to its end, the action gives the creature its full life and the temperature of the climate where it
	/// stands (sleeping, healing itself, resting to get better, going home to recover and others)
	bool fullRecovery {false};
};

struct DesireTexts
{
	/// As in "desire to impress", "am trying to impress", "am impressing"
	std::string desire;
	std::string trying;
	std::string doing;
};

struct Tables
{
	std::vector<ActionInfo> actions;
	/// The actions that satisfy each desire
	std::array<std::vector<uint32_t>, k_DesireCount> desireActions {};
	/// The actions a creature's compassion for a town may take for each of the town's desires
	creature_town_compassion::DesireActions townActions {};
	creature_learning::AllDesireRules rules {};
	creature_learning::Dependencies dependencies {};
	std::array<std::vector<creature_tree::Attribute>, k_DesireCount> attributes {};
	std::array<DesireTexts, k_DesireCount> texts {};
	/// The desires of the body, which stay at 0 until the creature grows to its first stage
	std::array<bool, k_DesireCount> bodyDesires {};
	std::vector<creature_watching::SkillRule> skills;
	std::vector<creature_watching::MiracleRule> miracles;
	std::vector<creature_watching::MimicRule> mimics;
};

[[nodiscard]] Tables Build(const InfoConstants& info);
/// A desire's attributes, as its row of the table lists them: at most ten, up to the first with no attribute (the list's
/// end), as the game reads them
[[nodiscard]] std::vector<creature_tree::Attribute> DesireAttributes(std::span<const uint32_t> row);
/// An action's row by its name
[[nodiscard]] std::optional<uint32_t> FindAction(const Tables& tables, std::string_view name);
/// Whether the creature knows what an action needs: each skill it names, and the miracle it names, known about
[[nodiscard]] bool KnowsWhatItNeeds(const ActionInfo& action, const creature_watching::Knowledge& knowledge);
/// The opinion of each action, in the table's order, that a new creature's mind starts with
[[nodiscard]] std::vector<float> StartingOpinions(const Tables& tables);
/// Whether an action of the game's table is done with something (an instrument) as well as to something
[[nodiscard]] bool NeedsInstrument(uint32_t action);
/// How a town takes an action of the game's table, done by a creature it knows: as nothing, with fear, with respect,
/// or, for the eating actions, with fear when what is eaten is one of its people and as nothing otherwise
enum class TownAttitudeFamily : uint8_t
{
	None,
	Fear,
	Respect,
	Eating,
};
[[nodiscard]] TownAttitudeFamily TownAttitudeFamilyOf(uint32_t action);

/// How many more times than usual each species must see a miracle to learn it, by its row in the creature tables
[[nodiscard]] float MiracleMultiplier(size_t speciesRow);

} // namespace openblack::creature_mind_tables
