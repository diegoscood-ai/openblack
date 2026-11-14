/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creatures in a saved game's world file: their own values after a living thing's parts
// (docs/wiki/engine/saved-games.md#openblack, the creature).

#include "WriteCreatures.h"

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <string>
#include <utility>

#include <LHVM.h>
#include <fmt/format.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpells.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAutoscale.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSizeLimits.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/SavedSlot.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteArenas.h"
#include "ECS/SavedWorld/WriteLivingParts.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/ScriptHeld.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// The type a creature spell's slot keeps: none (16), as the slots are made, for nothing sets it in play
constexpr uint32_t k_NoSpellType = 16;
/// The player number of a creature with no player
constexpr uint8_t k_NoPlayer = 0xFF;

std::string NameOf(entt::entity creature)
{
	return "creature " + std::to_string(entt::to_integral(creature));
}

/// The creature rows: the first, the Giant Ape's, numbered 2517, the species following it downwards in the order of
/// the game's creature tables
constexpr uint32_t k_FirstCreatureRow = 2517;

/// A place in the map coordinates a record keeps: as kept while it is still the place the kept coordinates name, as
/// openblack holds places in metres and the original in map coordinates
gamesave::MapCoords KeptOrSaved(glm::vec3 point, const gamesave::MapCoords* kept)
{
	if (kept != nullptr && map_coords::ToWorld({.x = kept->x, .z = kept->z, .altitude = kept->altitude}) == point)
	{
		return *kept;
	}
	const auto coords = map_coords::FromWorld(point);
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}

/// Whether the creature's invisible spell is off, as the save tests it before it writes the fizz
bool InvisibleOff(const components::CreatureSpells* spells)
{
	return spells == nullptr || spells->spells[creature_spells::Spell::Invisible].phase == creature_spells::Phase::Off;
}
/// The steps of the game's agenda a script's play is made of: an individual animation, a static one held for its
/// seconds, and the wait until the body is free
constexpr uint32_t k_PlayIndividualStep = 10;
constexpr uint32_t k_PlayStaticStep = 4;
constexpr uint32_t k_WaitUntilFreeStep = 0x85;
/// The other steps of a forced action's agenda: picking a thing up, playing with what it holds, turning to face a
/// thing, walking to a point, going where it can throw, going near a thing, throwing, fainting, setting down what it holds,
/// looking at the hand, taking in what a thing is, sleeping, waiting, turning to face the camera, looking at the camera, facing
/// down the slope, pointing at a point and at a thing
constexpr uint32_t k_PickUpStep = 0;
constexpr uint32_t k_DiscardStep = 1;
/// The clip a set-down a script forces plays
constexpr uint32_t k_PutDownClip = 0x61;
constexpr uint32_t k_HeldObjectActionStep = 3;
constexpr uint32_t k_LookAtHandStep = 0x12;
constexpr uint32_t k_ThrowStep = 5;
constexpr uint32_t k_FaintStep = 0xE;
constexpr uint32_t k_MoveToThrowPosStep = 0x10;
constexpr uint32_t k_TurnToFaceObjectStep = 6;
/// Turning to face a point, as a script's focus on a point has it do
constexpr uint32_t k_TurnToFacePosStep = 7;
constexpr uint32_t k_GoNearObjectStep = 0x3D;
constexpr uint32_t k_ObserveObjectStep = 0x3E;
constexpr uint32_t k_SleepStep = 0x84;
constexpr uint32_t k_MoveToPosStep = 8;
constexpr uint32_t k_WaitStep = 0x18;
constexpr uint32_t k_TurnToFaceCameraStep = 0x1F;
constexpr uint32_t k_LookAtCameraStep = 0x42;
constexpr uint32_t k_FaceDownSlopeStep = 0x44;
constexpr uint32_t k_PointAtPointStep = 0x50;
constexpr uint32_t k_PointAtObjectStep = 0x51;
/// Turning its back on a thing, and the poo itself once squatted
constexpr uint32_t k_TurnAwayFromObjectStep = 0x16;
constexpr uint32_t k_PooStep = 0xF;
/// Making a portion of food from a thing and picking it up, then eating what it made
constexpr uint32_t k_MakePortionStep = 0x8A;
constexpr uint32_t k_EatMadeStep = 0x80;
/// Eating a thing, and stomping on it first
constexpr uint32_t k_EatStep = 2;
constexpr uint32_t k_StompStep = 0x11;
/// Raising a totem pole
constexpr uint32_t k_RaiseTotemStep = 0x31;
/// The Die action every faint is: its row, the clip of its faint, the carry home, the wait at home, the wait for the
/// spells to wear off and the rest to get better
constexpr uint32_t k_DieAction = 9;
constexpr uint32_t k_DieFaintClip = 0x6A;
constexpr uint32_t k_TransportHomeStep = 0x70;
constexpr float k_DieHomeWaitSeconds = 3.0f;
constexpr uint32_t k_WaitForSpellsStep = 0x8B;
constexpr uint32_t k_RestStep = 0x82;
/// Backing off from a thing, the miracle's gesture, and casting a miracle at a thing
constexpr uint32_t k_GetAwayFromObjectStep = 0x87;
constexpr uint32_t k_GestureStep = 0x58;
constexpr uint32_t k_CastAtObjectStep = 0x27;
/// How long the game's look at the camera lasts, in seconds
constexpr float k_LookAtCameraSeconds = 5.0f;

/// An action row a script may force whose agenda openblack builds as the game does: its steps and the place of its main
/// step
struct ForcedRow
{
	uint32_t action;
	/// Its steps, none for an agenda whose steps the game draws or chooses as it makes it (its main step is then the
	/// one openblack marks)
	std::optional<size_t> steps;
	uint32_t main;
	/// Its main step is its last, the cast, whatever steps come before it
	bool mainIsLast {false};
};
/// Sitting down at a thing (walk there, face it, face down the slope, sit), looking at the camera, pointing at a thing,
/// pointing at the camera (turn to it, point), looking at a thing for a while or for ever (turn to it, wait), waving
/// at the player (turn to the camera, wave) and being idle (twice a wait and a yawn, the last yawn the main step);
/// the creature guide's: being frightened on the spot (frightened, a wait, frightened again), waving at a thing (go near
/// it, face it, wave), smiling at a friend (go near, face it, and a happy clip at random) and holding a thing (pick it
/// up unless the hand is full, face the camera, play with it while the creature has done so less than twice, a wait)
/// looking a thing over (go near it, face it, watch its foot, take in what it is) and sleeping at home (a yawn at
/// random, the walk home, face down the slope, the main sleep, a dazed wake); hurling a thing (angry at random, pick it
/// up unless the hand is full, go where it can throw, the main throw), dying for good (the main faint, a long wait),
/// looking at the hand, setting down what it holds (with no main step) and sleeping by a thing (the walk to a clear
/// place by it, a yawn at random, face down the slope, the main sleep, a dazed wake); a poo and a fart by a thing (the
/// need-a-poo clip at random, go right up to it, turn its back on it, the main squat, the poo itself) and eating from a
/// food pile (the walk to it, face it, make a portion and pick it up, the main eat); eating a thing alive (hungry at
/// random and the pick-up, unless it holds a thing it can eat, then the main eat) and stomping on it and eating it (the
/// stomp and the pick-up unless it holds a thing it can eat, the main eat) and raising a totem pole (go near it, face it,
/// the summoning clip, the main raise); casting a fireball, a lightning bolt, a healing miracle's power-ups and the itch
/// on a creature (angry, kind or playful at random, the walk near it or to a clear place, the back-off, the turn to it,
/// the miracle's gesture and the main cast)
constexpr std::array<ForcedRow, 30> k_ForcedRows {{
    {.action = 20, .steps = 4, .main = 3},
    {.action = 17, .steps = std::nullopt, .main = 0},
    {.action = 78, .steps = std::nullopt, .main = 0},
    {.action = 15, .steps = std::nullopt, .main = 0},
    {.action = 8, .steps = 1, .main = 0},
    {.action = 67, .steps = 1, .main = 0xFFFFFFFF},
    {.action = 217, .steps = 2, .main = 0},
    {.action = 261, .steps = 2, .main = 0},
    {.action = 174, .steps = 3, .main = 2},
    {.action = 91, .steps = 3, .main = 2},
    {.action = 60, .steps = std::nullopt, .main = 1},
    {.action = 64, .steps = std::nullopt, .main = 0},
    {.action = 81, .steps = 4, .main = 3},
    {.action = 90, .steps = 2, .main = 1},
    {.action = 161, .steps = 4, .main = 3},
    {.action = 82, .steps = 1, .main = 0},
    {.action = 163, .steps = 1, .main = 0},
    {.action = 168, .steps = 2, .main = 1},
    {.action = 188, .steps = 2, .main = 1},
    {.action = 189, .steps = 2, .main = 1},
    {.action = 19, .steps = std::nullopt, .main = 0},
    {.action = 177, .steps = std::nullopt, .main = 0},
    {.action = 158, .steps = 4, .main = 3},
    {.action = 11, .steps = std::nullopt, .main = 0},
    {.action = 13, .steps = std::nullopt, .main = 0},
    {.action = 56, .steps = 4, .main = 3},
    {.action = 96, .steps = std::nullopt, .main = 0, .mainIsLast = true},
    {.action = 99, .steps = std::nullopt, .main = 0, .mainIsLast = true},
    {.action = 102, .steps = std::nullopt, .main = 0, .mainIsLast = true},
    {.action = 120, .steps = std::nullopt, .main = 0, .mainIsLast = true},
}};
/// The desire a plan has once a finished one is cleared: none of the forty
constexpr uint32_t k_NoDesire = 40;

/// A thing by its number, or none
gamesave::Ref RefOf(const std::optional<uint32_t>& thing, const std::function<gamesave::Ref(uint32_t)>& refOf)
{
	return thing.has_value() ? refOf(*thing) : gamesave::Ref {};
}

/// Seconds as the game's whole turns
uint32_t TurnsOf(float seconds)
{
	return static_cast<uint32_t>(game_clock::TicksForSeconds(seconds));
}

/// A step of a forced action's agenda as the game keeps it, by the sub-action it is and its argument; none for a step
/// openblack does not build as the game does, with the reason
std::optional<gamesave::AgendaStep> ForcedStepOf(const creature_mind::Step& step, const components::CreatureMindState& mind,
                                                 const std::function<gamesave::Ref(uint32_t)>& refOf,
                                                 const gamesave::Ref& actOn, bool eatsMade, std::string& why)
{
	using Kind = creature_mind::Step::Kind;
	using Movement = creature_mind::Movement::Kind;
	if (step.kind == Kind::Static && step.faint)
	{
		// fainting, its clip the faint's
		return gamesave::AgendaStep {.action = k_FaintStep, .integer = static_cast<uint32_t>(step.sequence[0])};
	}
	if (step.kind == Kind::Static && step.untilRested)
	{
		// sleeping until rested
		return gamesave::AgendaStep {.action = k_SleepStep};
	}
	if (step.kind == Kind::Static)
	{
		return gamesave::AgendaStep {
		    .action = k_PlayStaticStep, .integer = static_cast<uint32_t>(step.animation), .real = step.seconds};
	}
	if (step.kind == Kind::Action)
	{
		return gamesave::AgendaStep {.action = k_PlayIndividualStep, .integer = static_cast<uint32_t>(step.animation)};
	}
	if (step.kind == Kind::Observe && step.object.has_value())
	{
		// taking in what a thing is: the kind of the creature's belief about it
		const auto* belief = mind.beliefs.Find(static_cast<entt::entity>(*step.object));
		if (belief == nullptr)
		{
			why = "a forced look over a thing it holds no belief about";
			return std::nullopt;
		}
		return gamesave::AgendaStep {.action = k_ObserveObjectStep, .integer = belief->kind};
	}
	if (step.kind == Kind::Wait)
	{
		return gamesave::AgendaStep {.action = k_WaitStep, .integer = TurnsOf(step.seconds)};
	}
	if (step.kind == Kind::LookAtCamera)
	{
		return gamesave::AgendaStep {.action = k_LookAtCameraStep,
		                             .integer = step.turns.value_or(TurnsOf(k_LookAtCameraSeconds))};
	}
	if (step.kind == Kind::Move && step.movement.kind == Movement::TurnToFaceObject)
	{
		return gamesave::AgendaStep {
		    .action = k_TurnToFaceObjectStep, .object = RefOf(step.movement.object, refOf), .real = step.seconds};
	}
	if (step.kind == Kind::Move && step.movement.kind == Movement::TurnToFace && step.gaze.has_value() && step.gaze->camera)
	{
		return gamesave::AgendaStep {.action = k_TurnToFaceCameraStep};
	}
	if (step.kind == Kind::Move && step.movement.kind == Movement::TurnAwayFromObject)
	{
		return gamesave::AgendaStep {.action = k_TurnAwayFromObjectStep, .object = RefOf(step.movement.object, refOf)};
	}
	if (step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::PickUpPortion)
	{
		return gamesave::AgendaStep {.action = k_MakePortionStep,
		                             .object = RefOf(step.order.object, refOf),
		                             .integer = static_cast<uint32_t>(step.order.portion)};
	}
	if (step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::Eat)
	{
		// eating the portion it made, else the thing the plan is done to
		if (eatsMade)
		{
			return gamesave::AgendaStep {.action = k_EatMadeStep};
		}
		return gamesave::AgendaStep {.action = k_EatStep, .object = actOn};
	}
	if (step.kind == Kind::Move && step.movement.kind == Movement::GetAwayFromObject)
	{
		return gamesave::AgendaStep {
		    .action = k_GetAwayFromObjectStep, .object = RefOf(step.movement.object, refOf), .real = step.movement.maxDistance};
	}
	if (step.kind == Kind::Gesture)
	{
		return gamesave::AgendaStep {.action = k_GestureStep, .integer = static_cast<uint32_t>(step.animation)};
	}
	if (step.kind == Kind::Cast && step.cast.object.has_value())
	{
		// cast from its pose at the thing and held for its seconds
		return gamesave::AgendaStep {.action = k_CastAtObjectStep,
		                             .object = RefOf(step.cast.object, refOf),
		                             .integer = static_cast<uint32_t>(step.animation),
		                             .real = step.seconds,
		                             .spell = step.cast.magicType};
	}
	if (step.kind == Kind::RaiseTotem)
	{
		return gamesave::AgendaStep {.action = k_RaiseTotemStep, .object = RefOf(step.object, refOf)};
	}
	if (step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::Destroy)
	{
		return gamesave::AgendaStep {.action = k_StompStep, .object = RefOf(step.order.object, refOf)};
	}
	if (step.kind == Kind::Apply && step.effect == creature_mind::Effect::Poo)
	{
		return gamesave::AgendaStep {.action = k_PooStep};
	}
	if (step.kind == Kind::Move && step.movement.kind == Movement::GoNearObject)
	{
		return gamesave::AgendaStep {
		    .action = k_GoNearObjectStep, .object = RefOf(step.movement.object, refOf), .real = step.movement.maxDistance};
	}
	if (step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::PickUp)
	{
		return gamesave::AgendaStep {.action = k_PickUpStep, .object = RefOf(step.order.object, refOf)};
	}
	if (step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::PutDown)
	{
		// setting down what it holds, with the put-down's clip
		return gamesave::AgendaStep {.action = k_DiscardStep, .integer = k_PutDownClip};
	}
	if (step.kind == Kind::WatchHand)
	{
		return gamesave::AgendaStep {.action = k_LookAtHandStep};
	}
	if (step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::Throw)
	{
		return gamesave::AgendaStep {.action = k_ThrowStep, .object = RefOf(step.order.object, refOf)};
	}
	if (step.kind == Kind::Move && step.movement.kind == Movement::ToThrowPosition)
	{
		return gamesave::AgendaStep {
		    .action = k_MoveToThrowPosStep, .object = RefOf(step.movement.object, refOf), .real = step.movement.maxDistance};
	}
	if (step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::Keep)
	{
		return gamesave::AgendaStep {.action = k_HeldObjectActionStep, .integer = static_cast<uint32_t>(step.order.animation)};
	}
	if (step.kind == Kind::Move && step.movement.kind == Movement::FaceDownSlope)
	{
		return gamesave::AgendaStep {.action = k_FaceDownSlopeStep};
	}
	if (step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::PointAt)
	{
		const float seconds = step.order.seconds.value_or(0.0f);
		if (step.order.object.has_value())
		{
			return gamesave::AgendaStep {
			    .action = k_PointAtObjectStep, .object = RefOf(step.order.object, refOf), .real = seconds};
		}
		return gamesave::AgendaStep {.action = k_PointAtPointStep,
		                             .point = {step.order.point.x, step.order.pointHeight.value_or(0.0f), step.order.point.y},
		                             .real = seconds};
	}
	if (step.kind == Kind::Move && step.movement.kind == Movement::ToPoint && step.movement.pointHeight.has_value())
	{
		// a walk to a point, within the step's distance of it
		return gamesave::AgendaStep {.action = k_MoveToPosStep,
		                             .point = {step.movement.point.x, *step.movement.pointHeight, step.movement.point.y},
		                             .real = step.movement.maxDistance};
	}
	// a walk to a point whose height is not held
	why = step.kind == Kind::Move ? "a forced action still walking to its place" : "a step of a forced action not written yet";
	return std::nullopt;
}

/// An action a script forces, as the game's agenda keeps it: its plan of obeying its player with the action's row, what
/// it is done for, to and with, and its agenda from the step it is at, for the rows openblack builds as the game does
std::optional<gamesave::KeptCreaturePlan> ForcedPlanOf(const components::CreatureMindState& mind,
                                                       const creature_planner::Plan& current, gamesave::Ref self,
                                                       const std::function<gamesave::Ref(uint32_t)>& refOf, std::string& why)
{
	const auto row = std::ranges::find(k_ForcedRows, current.action, &ForcedRow::action);
	const auto& idle = mind.idle;
	if (row == k_ForcedRows.end())
	{
		why = fmt::format("a forced action of row {}, whose agenda is not written yet", current.action);
		return std::nullopt;
	}
	if ((row->steps.has_value() && idle.agenda.size() != *row->steps) || idle.step >= idle.agenda.size())
	{
		// sitting where it stands, say, openblack keeps fewer steps than the game's
		why = fmt::format("a forced action of row {} of {} steps, not the game's {}", current.action, idle.agenda.size(),
		                  row->steps.value_or(0));
		return std::nullopt;
	}
	// the main step: the one openblack marks, else the row's
	const auto marked = std::ranges::find_if(idle.agenda, [](const creature_mind::Step& step) { return step.main; });
	if (!row->steps.has_value() && !row->mainIsLast && marked == idle.agenda.end())
	{
		why = fmt::format("a forced action of row {} with no main step", current.action);
		return std::nullopt;
	}
	const auto main = row->mainIsLast               ? static_cast<uint32_t>(idle.agenda.size() - 1)
	                  : marked != idle.agenda.end() ? static_cast<uint32_t>(marked - idle.agenda.begin())
	                                                : row->main;
	// the game saves a creature dead for ever from its agenda's first step
	constexpr uint32_t k_DeadForeverAction = 261;
	const size_t first = current.action == k_DeadForeverAction ? 0 : idle.step;
	gamesave::KeptCreaturePlan plan {
	    .desire = static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer),
	    .action = current.action,
	    .objects = {RefOf(current.activityObject, refOf), RefOf(current.object, refOf), RefOf(current.instrument, refOf)},
	    .firstWord = idle.stepStarted ? 0U : 1U,
	    .current = static_cast<uint32_t>(first),
	    .state = idle.stepStarted ? 1U : 0U,
	    .count = static_cast<uint32_t>(idle.agenda.size()),
	    .words = {0, main, 0},
	    .creature = self};
	const auto actOn = RefOf(current.object, refOf);
	const bool eatsMade = std::ranges::any_of(idle.agenda, [](const creature_mind::Step& step) {
		return step.kind == creature_mind::Step::Kind::Object &&
		       step.order.kind == creature_mind::ObjectOrder::Kind::PickUpPortion;
	});
	for (size_t i = first; i < idle.agenda.size(); ++i)
	{
		const auto step = ForcedStepOf(idle.agenda[i], mind, refOf, actOn, eatsMade, why);
		if (!step.has_value())
		{
			return std::nullopt;
		}
		plan.steps.push_back(*step);
	}
	return plan;
}

/// A creature's own plan of sleeping on the spot that found no clear place, the game's plain sleep its agenda (a yawn
/// or not, the walk home, the face down the slope, the sleep, the main step, and the wake): its plan as the game keeps
/// it, the walk's point on the land at its place, as the game's walk home takes it; none for any other agenda
std::optional<gamesave::KeptCreaturePlan> OwnSleepPlanOf(const components::CreatureMindState& mind,
                                                         const creature_planner::Plan& current, gamesave::Ref self,
                                                         const std::function<gamesave::Ref(uint32_t)>& refOf,
                                                         const std::function<float(glm::vec2)>& groundAt, std::string& why)
{
	constexpr uint32_t k_SleepOnTheSpot = 313;
	using Kind = creature_mind::Step::Kind;
	using Movement = creature_mind::Movement::Kind;
	const auto& idle = mind.idle;
	const auto& agenda = idle.agenda;
	// a yawn or not, then the walk home, the face down the slope, the sleep and the wake
	const size_t from = agenda.size() == 5 ? 1 : 0;
	const bool plainSleep = current.action == k_SleepOnTheSpot && (agenda.size() == 4 || agenda.size() == 5) &&
	                        (from == 0 || agenda[0].kind == Kind::Action) && agenda[from].kind == Kind::Move &&
	                        agenda[from].movement.kind == Movement::ToPoint && agenda[from + 1].kind == Kind::Move &&
	                        agenda[from + 1].movement.kind == Movement::FaceDownSlope &&
	                        agenda[from + 2].kind == Kind::Static && agenda[from + 2].untilRested &&
	                        agenda[from + 3].kind == Kind::Action;
	if (!plainSleep || idle.step >= agenda.size())
	{
		return std::nullopt;
	}
	gamesave::KeptCreaturePlan plan {
	    .desire = static_cast<uint32_t>(current.desire),
	    .action = current.action,
	    .objects = {RefOf(current.activityObject, refOf), RefOf(current.object, refOf), RefOf(current.instrument, refOf)},
	    .firstWord = idle.stepStarted ? 0U : 1U,
	    .current = static_cast<uint32_t>(idle.step),
	    .state = idle.stepStarted ? 1U : 0U,
	    .count = static_cast<uint32_t>(agenda.size()),
	    .words = {0, static_cast<uint32_t>(from + 2), 0},
	    .creature = self};
	const auto actOn = RefOf(current.object, refOf);
	for (size_t i = idle.step; i < agenda.size(); ++i)
	{
		auto step = agenda[i];
		if (step.kind == Kind::Move && step.movement.kind == Movement::ToPoint && !step.movement.pointHeight.has_value())
		{
			step.movement.pointHeight = groundAt(step.movement.point);
		}
		const auto saved = ForcedStepOf(step, mind, refOf, actOn, false, why);
		if (!saved.has_value())
		{
			return std::nullopt;
		}
		plan.steps.push_back(*saved);
	}
	return plan;
}
/// A creature's own plan of fishing and eating the catch, as the game keeps it: the walk to the shoal, the food made from
/// the sea, its pick-up and the eat of it, the main step. openblack makes the food and picks it up in one step, which
/// is the game's two: until it starts it is the making still to come, once started the pick-up under way. None for any
/// other agenda
std::optional<gamesave::KeptCreaturePlan> FishAndEatPlanOf(const components::CreatureMindState& mind,
                                                           const creature_planner::Plan& current, gamesave::Ref self,
                                                           const std::function<gamesave::Ref(uint32_t)>& refOf)
{
	constexpr uint32_t k_FishAndEat = 155;
	constexpr uint32_t k_FishFromSeaStep = 0x5C;
	constexpr uint32_t k_PickUpMadeStep = 0x37;
	constexpr uint32_t k_GameSteps = 4;
	constexpr uint32_t k_MainStep = 3;
	using Kind = creature_mind::Step::Kind;
	using Order = creature_mind::ObjectOrder::Kind;
	const auto& idle = mind.idle;
	const auto& agenda = idle.agenda;
	const bool fishing = current.action == k_FishAndEat && agenda.size() == 3 && agenda[0].kind == Kind::Move &&
	                     agenda[0].movement.kind == creature_mind::Movement::Kind::ToPoint &&
	                     agenda[0].movement.pointHeight.has_value() && agenda[1].kind == Kind::Object &&
	                     agenda[1].order.kind == Order::FishFromSea && agenda[2].kind == Kind::Object &&
	                     agenda[2].order.kind == Order::Eat;
	if (!fishing || idle.step >= agenda.size())
	{
		return std::nullopt;
	}
	// the game's step for each of openblack's, the fishing step the pick-up once it has started
	const uint32_t gameStep = idle.step == 0 ? 0 : (idle.step == 1 ? (idle.stepStarted ? 2 : 1) : 3);
	gamesave::KeptCreaturePlan plan {
	    .desire = static_cast<uint32_t>(current.desire),
	    .action = current.action,
	    .objects = {RefOf(current.activityObject, refOf), RefOf(current.object, refOf), RefOf(current.instrument, refOf)},
	    .firstWord = idle.stepStarted ? 0U : 1U,
	    .current = gameStep,
	    .state = idle.stepStarted ? 1U : 0U,
	    .count = k_GameSteps,
	    .words = {0, k_MainStep, 0},
	    .creature = self};
	const std::array<gamesave::AgendaStep, k_GameSteps> steps {
	    gamesave::AgendaStep {
	        .action = k_MoveToPosStep,
	        .point = {agenda[0].movement.point.x, *agenda[0].movement.pointHeight, agenda[0].movement.point.y},
	        .real = agenda[0].movement.maxDistance},
	    gamesave::AgendaStep {.action = k_FishFromSeaStep}, gamesave::AgendaStep {.action = k_PickUpMadeStep},
	    gamesave::AgendaStep {.action = k_EatMadeStep}};
	plan.steps.assign(steps.begin() + gameStep, steps.end());
	return plan;
}
/// A creature's own plan of looking over the place of a miracle, as the game keeps it: pointing first or not, the walk
/// to within five heights, the turn to the place, then the puzzled wait or the point, the last the main step; each
/// step's point the place on the land. None for any other agenda
std::optional<gamesave::KeptCreaturePlan> ExaminePlacePlanOf(const components::CreatureMindState& mind,
                                                             const creature_planner::Plan& current, gamesave::Ref self,
                                                             const std::function<gamesave::Ref(uint32_t)>& refOf,
                                                             const std::function<float(glm::vec2)>& groundAt)
{
	constexpr uint32_t k_ExaminePos = 157;
	// the point held a second when pointing first, two when pointing last
	constexpr float k_FirstPointSeconds = 1.0f;
	constexpr float k_LastPointSeconds = 2.0f;
	using Kind = creature_mind::Step::Kind;
	using Movement = creature_mind::Movement::Kind;
	const auto& idle = mind.idle;
	const auto& agenda = idle.agenda;
	const auto isTurn = [](const creature_mind::Step& step) {
		return step.kind == Kind::Move && step.movement.kind == Movement::TurnToFace;
	};
	const auto isPoint = [](const creature_mind::Step& step) {
		return step.kind == Kind::Object && step.order.kind == creature_mind::ObjectOrder::Kind::PointAt;
	};
	const bool pointedFirst = agenda.size() == 5;
	const size_t walk = pointedFirst ? 2 : 0;
	const bool examining = current.action == k_ExaminePos && (agenda.size() == 3 || pointedFirst) &&
	                       (!pointedFirst || (isTurn(agenda[0]) && isPoint(agenda[1]))) && agenda[walk].kind == Kind::Move &&
	                       agenda[walk].movement.kind == Movement::ToPoint && isTurn(agenda[walk + 1]) &&
	                       (pointedFirst ? agenda[walk + 2].kind == Kind::Wait : isPoint(agenda[walk + 2]));
	if (!examining || idle.step >= agenda.size())
	{
		return std::nullopt;
	}
	const auto place = agenda[walk].movement.point;
	const gamesave::Point at {place.x, groundAt(place), place.y};
	std::vector<gamesave::AgendaStep> steps;
	if (pointedFirst)
	{
		steps.push_back({.action = k_TurnToFacePosStep, .point = at});
		steps.push_back({.action = k_PointAtPointStep, .point = at, .real = k_FirstPointSeconds});
	}
	steps.push_back({.action = k_MoveToPosStep, .point = at, .real = agenda[walk].movement.maxDistance});
	steps.push_back({.action = k_TurnToFacePosStep, .point = at});
	if (pointedFirst)
	{
		steps.push_back({.action = k_WaitStep, .integer = TurnsOf(agenda[walk + 2].seconds)});
	}
	else
	{
		steps.push_back({.action = k_PointAtPointStep, .point = at, .real = k_LastPointSeconds});
	}
	gamesave::KeptCreaturePlan plan {
	    .desire = static_cast<uint32_t>(current.desire),
	    .action = current.action,
	    .objects = {RefOf(current.activityObject, refOf), RefOf(current.object, refOf), RefOf(current.instrument, refOf)},
	    .firstWord = idle.stepStarted ? 0U : 1U,
	    .current = static_cast<uint32_t>(idle.step),
	    .state = idle.stepStarted ? 1U : 0U,
	    .count = static_cast<uint32_t>(agenda.size()),
	    .words = {0, static_cast<uint32_t>(agenda.size() - 1), 0},
	    .creature = self};
	plan.steps.assign(steps.begin() + static_cast<std::ptrdiff_t>(idle.step), steps.end());
	return plan;
}
} // namespace

bool openblack::ecs::saved_world_write::ScriptIsRunning(const std::map<uint32_t, lhvm::VMTask>& tasks, std::string_view script)
{
	return std::ranges::any_of(tasks, [script](const auto& task) { return task.second.name == script; });
}

float openblack::ecs::saved_world_write::GlobalAsFloat(std::span<const lhvm::VMVar> globals, std::string_view name)
{
	const auto found = std::ranges::find_if(globals, [name](const lhvm::VMVar& global) { return global.name == name; });
	return found != globals.end() ? found->value.floatVal : 0.0f;
}

float openblack::ecs::saved_world_write::SavedAlignment(float alignment, bool localPlayers,
                                                        const std::map<uint32_t, lhvm::VMTask>* tasks,
                                                        std::span<const lhvm::VMVar> globals)
{
	if (!localPlayers || tasks == nullptr || !ScriptIsRunning(*tasks, k_CurseScript))
	{
		return alignment;
	}
	return GlobalAsFloat(globals, k_CurseAlignmentGlobal);
}

std::optional<gamesave::CreatureFields>
openblack::ecs::saved_world_write::CreatureFieldsOf(WriteContext& context, entt::entity creature,
                                                    const gamesave::CreatureFields* kept)
{
	const auto& registry = context.GetRegistry();
	const auto& body = registry.Get<const components::Creature>(creature);
	gamesave::CreatureFields fields = kept != nullptr ? *kept : gamesave::CreatureFields {};
	fields.version = gamesave::k_CreatureVersion;
	fields.savedInTemple = game_clock::IsInsideCitadel() ? 1 : 0;

	// its place in the game's list of creatures, which names its mind file, and its player
	const auto list = saved_slot::CreaturesInListOrder(registry);
	const auto place = std::ranges::find(list, creature);
	fields.index = place != list.end() ? static_cast<uint8_t>(place - list.begin()) : k_NoPlayer;
	fields.owner = body.owner != PlayerNames::_COUNT ? static_cast<uint8_t>(body.owner) : k_NoPlayer;

	// the creature spells: each slot's phase, its turns left, the value it found, its hold and its miracle
	const auto* spells = registry.TryGet<const components::CreatureSpells>(creature);
	const creature_spells::Spells none {};
	const auto& held = spells != nullptr ? spells->spells : none;
	for (size_t i = 0; i < gamesave::k_CreatureSpellCount; ++i)
	{
		const auto& slot = held.slots.at(i);
		auto& saved = fields.spells.at(i);
		if (kept == nullptr)
		{
			saved.spell = k_NoSpellType;
		}
		saved.phase = static_cast<uint32_t>(slot.phase);
		saved.turnsLeft = slot.turnsLeft;
		saved.found = slot.before;
		saved.holdTurns = slot.holdTurns;
		saved.miracle = {slot.miracle != entt::null && registry.Valid(slot.miracle) ? context.Reference(slot.miracle) : 0};
	}
	// the spells waiting: the seconds each was cast for are not held yet (openblack keeps its turns), so a creature
	// with spells waiting stops the write
	fields.queued.clear();
	if (!held.waiting.empty())
	{
		context.Fail(NameOf(creature) + " has spells waiting: the seconds they were cast for are not held yet");
		return std::nullopt;
	}

	// the mind holds still while a freeze is on it
	const auto* mind = registry.TryGet<const components::CreatureMindState>(creature);
	fields.mindProcessing = mind != nullptr && mind->paused ? 0 : 1;

	// the sizes the size spells take it to
	const auto* limits = registry.TryGet<const components::CreatureSizeLimits>(creature);
	const creature_spells::SizeLimits defaults {};
	fields.largestSize = limits != nullptr ? limits->limits.largest : defaults.largest;
	fields.smallestSize = limits != nullptr ? limits->limits.smallest : defaults.smallest;
	fields.mayGoThroughVortex = body.mayGoThroughVortex ? 1 : 0;

	// its fizz, none while its invisible spell is off, as the save sets it
	const auto* fizz = registry.TryGet<const components::CreatureFizz>(creature);
	const creature_fizz::Fizz noFizz {};
	const auto& fizzed = fizz != nullptr ? fizz->fizz : noFizz;
	fields.fizzedForGood = fizzed.goesForGood ? 1 : 0;
	const bool zeroed = InvisibleOff(spells);
	fields.fizz = zeroed ? 0.0f : fizzed.now;
	fields.fizzTarget = zeroed ? 0.0f : fizzed.target;
	fields.fizzPerSecond = zeroed ? 0.0f : fizzed.perSecond;
	fields.freeze = spells != nullptr ? spells->freeze : 0.0f;
	// IgnoredByVillagers: only the constructor (0) and the load write it, so a creature made in play writes 0
	if (kept == nullptr)
	{
		fields.word = 0;
	}

	// the miracle it holds while it casts, then two visuals nothing sets in play (+0x1084 and +0x1088: only the
	// constructor, the load and the clears write them, so as made, 0, or as kept), and the beams of a cast from above
	// at its hands, the mirror hand's first, while their spot visuals are there
	const auto* casting = registry.TryGet<const components::CreatureCasting>(creature);
	const auto heldMiracle = casting != nullptr ? casting->held : entt::entity {entt::null};
	fields.spellVisuals[0] = {heldMiracle != entt::null && registry.Valid(heldMiracle) ? context.Reference(heldMiracle) : 0};
	if (kept == nullptr)
	{
		fields.spellVisuals[1] = {0};
		fields.spellVisuals[2] = {0};
	}
	for (size_t i = 0; i < fields.handParticles.size(); ++i)
	{
		fields.handParticles.at(i) = {casting != nullptr ? SpotVisualObject(context, casting->beams.at(i)) : 0};
	}

	// where the leash last sent it: the point above its target, the mark shown there while it is, the thing, and
	// whether it has a target
	const auto* leash = registry.TryGet<const components::CreatureLeash>(creature);
	fields.targetPoint = leash != nullptr ? gamesave::Point {leash->targetPoint.x, leash->targetPoint.y, leash->targetPoint.z}
	                                      : gamesave::Point {};
	fields.targetParticles = {leash != nullptr ? SpotVisualObject(context, leash->targetMarker) : 0};
	fields.targetObject = {leash != nullptr && leash->targetThing != entt::null && registry.Valid(leash->targetThing)
	                           ? context.Reference(leash->targetThing)
	                           : 0};
	fields.hasTarget = leash != nullptr && leash->hasTarget ? 1 : 0;

	// its fight: in a duel, the arena its maker names, its fight health and who fights for it; out of a fight, no
	// arena, and its fight health as it left its last fight
	const auto* fighting = registry.TryGet<const components::CreatureFighting>(creature);
	if (fighting != nullptr && fighting->stage != components::CreatureFighting::Stage::Duel)
	{
		context.Fail(NameOf(creature) + " is fighting, before or after its duel: its fight's steps there are not " +
		             "written from openblack yet");
		return std::nullopt;
	}
	fields.arena = {0};
	if (fighting != nullptr)
	{
		if (const auto arena = ArenaKeyOf(registry, creature); arena.has_value())
		{
			fields.arena = {context.Reference(*arena)};
		}
		fields.fightHealth = fighting->fighter.health;
		fields.autoFight = AutoFightOf(fighting->fighter);
	}
	else
	{
		const auto* fightHealth = registry.TryGet<const components::CreatureFightHealth>(creature);
		fields.fightHealth = fightHealth != nullptr ? fightHealth->health : components::CreatureFightHealth {}.health;
		// whether the computer fights for it: openblack keeps it only within a fight, so out of one it is the
		// constructor's 0 (a script's setting out of a fight is not kept yet)
		if (kept == nullptr)
		{
			fields.autoFight = 0;
		}
	}

	// its home: its player's temple's pen while there is one, else the home the leash keeps it at; none, as kept
	const auto* keptHome = kept != nullptr ? &kept->home : nullptr;
	if (const auto pen = player_creature::TemplePenOf(body.owner); pen.has_value())
	{
		fields.home = KeptOrSaved(*pen, keptHome);
	}
	else if (leash != nullptr && leash->home.has_value())
	{
		fields.home = KeptOrSaved(*leash->home, keptHome);
	}

	// the villagers it called to dance with it, the last called first, and the dance it made last while there is one
	fields.dancePartners.numbers.clear();
	fields.dance = {0};
	if (const auto* dance = registry.TryGet<const components::CreatureDance>(creature))
	{
		for (const auto dancer : dance->dancers)
		{
			if (dancer != entt::null && registry.Valid(dancer))
			{
				fields.dancePartners.numbers.push_back(context.Reference(dancer));
			}
		}
		if (dance->dance != entt::null && registry.Valid(dance->dance))
		{
			fields.dance = {context.Reference(dance->dance)};
		}
	}

	fields.canDie = body.canDie ? 1 : 0;
	fields.strength = body.strength;
	fields.size = body.size;
	const auto* vm = Locator::vm::has_value() ? &Locator::vm::value() : nullptr;
	fields.alignment =
	    SavedAlignment(body.alignment, body.owner == creature::LocalPlayer(), vm != nullptr ? &vm->GetTasks() : nullptr,
	                   vm != nullptr ? std::span<const lhvm::VMVar>(vm->GetVariables()) : std::span<const lhvm::VMVar> {});
	// the animation's pace: 1 less the freeze, as the freeze eases it
	fields.animationPace = 1.0f - fields.freeze;

	// its scale as a script sets it going with its size
	const auto* autoscale = registry.TryGet<const components::CreatureAutoscale>(creature);
	fields.autoScale = autoscale != nullptr && autoscale->enabled ? 1 : 0;
	fields.autoScaleShare = autoscale != nullptr ? autoscale->factor : 0.0f;

	// its plan follows while a script controls it, it fights or it keeps villagers it called to dance with it
	if (script_held::IsControlledByScript(creature) || fighting != nullptr || !fields.dancePartners.numbers.empty())
	{
		const auto* mind = registry.TryGet<const components::CreatureMindState>(creature);
		std::string why = "it has no mind";
		const auto refOf = [&context](uint32_t thing) {
			return gamesave::Ref {context.Reference(static_cast<entt::entity>(thing))};
		};
		const auto opponent = fighting != nullptr && fighting->opponent != entt::null && registry.Valid(fighting->opponent)
		                          ? std::optional(gamesave::Ref {context.Reference(fighting->opponent)})
		                          : std::nullopt;
		// knocked out, its plan is the Die action every faint is
		const auto* knockedOut = registry.TryGet<const components::CreatureKnockedOut>(creature);
		const gamesave::Ref self {context.Reference(creature)};
		const auto plan =
		    fighting != nullptr ? (opponent.has_value() ? DuelPlanOf(mind, fighting->madeArena, self, *opponent, why)
		                                                : std::optional<gamesave::KeptCreaturePlan> {})
		    : knockedOut != nullptr && !knockedOut->permanent
		        ? KnockedOutPlanOf(*knockedOut, self,
		                           knockedOut->actedOn.has_value() ? gamesave::Ref {context.Reference(*knockedOut->actedOn)}
		                                                           : self,
		                           body.owner != PlayerNames::NEUTRAL, why)
		    : mind != nullptr ? ScriptHeldPlanOf(*mind, self, refOf, why)
		                      : std::nullopt;
		if (!plan.has_value())
		{
			context.Fail(NameOf(creature) + " keeps its plan (a script controls it, it fights or it keeps dancers): its " +
			             "plan is not written from openblack yet (" +
			             (opponent.has_value() || fighting == nullptr ? why : "no opponent") + ")");
			return std::nullopt;
		}
		fields.keepsPlan = 1;
		fields.plan = *plan;
	}
	else
	{
		fields.keepsPlan = 0;
		fields.plan = {};
	}

	// the glows of its creed powers: each slot's scale and power, none without a glow
	const auto* glows = registry.TryGet<const components::CreatureHandGlows>(creature);
	for (size_t i = 0; i < gamesave::k_CreedPowerCount; ++i)
	{
		const auto& glow = glows != nullptr && i < glows->glows.size() ? glows->glows.at(i) : std::nullopt;
		fields.creedPowers.at(i) =
		    glow.has_value() ? gamesave::CreedPower {.scale = glow->scale, .power = glow->power} : gamesave::CreedPower {};
	}
	return fields;
}

uint32_t openblack::ecs::saved_world_write::AutoFightOf(const creature_fight::Fighter& fighter)
{
	// nobody, the player with the computer waiting to take over, or the computer
	constexpr uint32_t k_Nobody = 0;
	constexpr uint32_t k_PlayerThenComputer = 1;
	constexpr uint32_t k_Computer = 2;
	if (fighter.autoFight || fighter.control == creature_fight::Control::Computer)
	{
		return k_Computer;
	}
	return fighter.control == creature_fight::Control::Player ? k_PlayerThenComputer : k_Nobody;
}

std::optional<gamesave::KeptCreaturePlan>
openblack::ecs::saved_world_write::DuelPlanOf(const components::CreatureMindState* mind, bool madeTheArena, gamesave::Ref self,
                                              gamesave::Ref opponent, std::string& why)
{
	// the fight's sub-actions: the fight itself, ordering the partner to do one, the response after it, the partner
	// let go again and waiting for ever for the next order
	constexpr uint32_t k_FightStep = 0x1C;
	constexpr uint32_t k_OrderStep = 0x4A;
	constexpr uint32_t k_RespondStep = 0x48;
	constexpr uint32_t k_RemobiliseStep = 0x4D;
	constexpr uint32_t k_WaitForeverStep = 0x4E;
	// the fight's agenda: its twelve own steps and the six orders to the partner between them, the fight the main one
	constexpr uint32_t k_FightSteps = 18;
	constexpr uint32_t k_DuelStep = 14;
	// the fight under way, past its start
	constexpr uint32_t k_DuelStage = 1;
	constexpr uint32_t k_FightAction = 21;
	// telling another creature to go away, whose agenda is the fight's one time in two
	constexpr uint32_t k_TellToGoAwayAction = 256;
	constexpr uint32_t k_ObeyCreature = 26;
	constexpr uint32_t k_ObeyCreatureAction = 124;
	constexpr uint32_t k_NoMainStep = 0xFFFFFFFF;
	if (madeTheArena)
	{
		// the creature that started the fight, at its main step: the fight with the opponent, the order of the response
		// after it to the opponent, its own response and the opponent let go, under the plan that made the fight's
		// agenda, a fight or telling the opponent to go away
		const auto& current = mind != nullptr ? mind->planner.current : std::nullopt;
		if (!current.has_value() || (current->action != k_FightAction && current->action != k_TellToGoAwayAction))
		{
			why = current.has_value()
			          ? fmt::format("a fight its mind's plan is not of (desire {}, action {}{})",
			                        static_cast<uint32_t>(current->desire), current->action, current->forced ? ", forced" : "")
			          : "a fight its mind's plan is not of (no plan)";
			return std::nullopt;
		}
		return gamesave::KeptCreaturePlan {
		    .desire = static_cast<uint32_t>(current->desire),
		    .action = current->action,
		    .objects = {gamesave::Ref {}, opponent, gamesave::Ref {}},
		    .firstWord = 0,
		    .current = k_DuelStep,
		    .state = k_DuelStage,
		    .count = k_FightSteps,
		    .words = {0, k_DuelStep, 0},
		    .steps = {{.action = k_FightStep, .object = opponent},
		              {.action = k_OrderStep, .object = self, .other = opponent, .orderKind = k_RespondStep},
		              {.action = k_RespondStep, .object = opponent},
		              {.action = k_RemobiliseStep, .object = opponent}},
		    .creature = self,
		    .other = opponent};
	}
	// the opponent, as its order to fight left it: obeying the creature that started it, the fight, then waiting for
	// ever for its next order
	return gamesave::KeptCreaturePlan {
	    .desire = k_ObeyCreature,
	    .action = k_ObeyCreatureAction,
	    .objects = {opponent, opponent, gamesave::Ref {}},
	    .firstWord = 0,
	    .current = 0,
	    .state = k_DuelStage,
	    .count = 2,
	    .words = {0, k_NoMainStep, 0},
	    .steps = {{.action = k_FightStep, .object = opponent}, {.action = k_WaitForeverStep, .object = opponent}},
	    .creature = self,
	    .other = opponent};
}

std::optional<std::vector<gamesave::AgendaStep>>
openblack::ecs::saved_world_write::DanceStepsOf(std::span<const creature_mind::Step> agenda, size_t from,
                                                const std::function<float(glm::vec2)>& groundAt,
                                                const std::function<gamesave::Ref(uint32_t)>& refOf)
{
	using Kind = creature_mind::Step::Kind;
	using Order = creature_mind::DanceOrder::Kind;
	constexpr size_t k_DanceSteps = 14;
	// the dance's own steps from its fifth, none at the walk and the point
	constexpr std::array<std::optional<Order>, 10> k_Orders {
	    Order::Set,          Order::Summon,         Order::StopLooking, std::nullopt,       std::nullopt,
	    Order::SendToPlaces, Order::WaitUntilStill, Order::Start,       Order::TellToDance, Order::Dance};
	// the dance's sub-actions in its agenda's order
	constexpr std::array<uint32_t, k_DanceSteps> k_Codes {0x1D, 0x3D, 0x06, 0x0A, 0x7E, 0x0C, 0x30,
	                                                      0x08, 0x50, 0x83, 0x19, 0x2C, 0x2E, 0x17};
	if (agenda.size() != k_DanceSteps || from >= agenda.size() || agenda[0].kind != Kind::StopMoving ||
	    agenda[3].kind != Kind::Action || agenda[8].kind != Kind::Object)
	{
		return std::nullopt;
	}
	for (size_t i = 4; i < k_DanceSteps; ++i)
	{
		const auto order = k_Orders.at(i - 4);
		if (order.has_value() && (agenda[i].kind != Kind::Dance || agenda[i].dance.kind != *order))
		{
			return std::nullopt;
		}
	}
	const auto villagerRef = [&refOf](const std::optional<uint32_t>& villager) {
		return villager.has_value() ? refOf(*villager) : gamesave::Ref {};
	};
	const auto pointAt = [&groundAt](glm::vec2 at) { return gamesave::Point {at.x, groundAt(at), at.y}; };
	std::vector<gamesave::AgendaStep> steps;
	for (size_t i = from; i < k_DanceSteps; ++i)
	{
		const auto& step = agenda[i];
		gamesave::AgendaStep saved {.action = k_Codes.at(i)};
		switch (i)
		{
		case 1:
		case 2:
			saved.object = villagerRef(step.movement.object);
			saved.real = i == 1 ? step.movement.maxDistance : step.seconds;
			break;
		case 3:
			saved.integer = static_cast<uint32_t>(step.animation);
			break;
		case 4:
			saved.point = pointAt(step.dance.point);
			saved.integer = step.dance.type;
			break;
		case 5:
			saved.object = villagerRef(step.dance.villager);
			saved.integer = step.dance.count;
			break;
		case 7:
			saved.point = pointAt(step.movement.point);
			break;
		case 8:
			saved.point = pointAt(step.order.point);
			saved.real = step.order.seconds.value_or(0.0f);
			break;
		case 13:
			saved.integer = step.dance.count;
			break;
		default:
			break;
		}
		steps.push_back(saved);
	}
	return steps;
}

std::optional<gamesave::KeptCreaturePlan>
openblack::ecs::saved_world_write::KnockedOutPlanOf(const components::CreatureKnockedOut& knockedOut, gamesave::Ref self,
                                                    gamesave::Ref actedOn, bool hasPlayer, std::string& why)
{
	using Stage = components::CreatureKnockedOut::Stage;
	if (knockedOut.stage != Stage::Lying || !knockedOut.lieTurns.has_value() || !knockedOut.rests.has_value())
	{
		why = "a knock-out past its lie, whose steps are not written yet";
		return std::nullopt;
	}
	// the Die action's agenda: the main faint, the lie's wait, the carry home for a player's creature, three seconds'
	// wait, the wait for the spells and the rest it decided on
	std::vector<gamesave::AgendaStep> agenda;
	agenda.push_back({.action = k_FaintStep, .integer = k_DieFaintClip});
	agenda.push_back({.action = k_WaitStep, .integer = static_cast<uint32_t>(*knockedOut.lieTurns)});
	if (hasPlayer)
	{
		agenda.push_back({.action = k_TransportHomeStep});
	}
	agenda.push_back({.action = k_WaitStep, .integer = TurnsOf(k_DieHomeWaitSeconds)});
	agenda.push_back({.action = k_WaitForSpellsStep});
	if (*knockedOut.rests)
	{
		agenda.push_back({.action = k_RestStep});
	}
	// one stage a turn: the faint's start the turn it faints, its second once it is seen lying; the wait's turns count
	// down from the next, and the turn they are out the carry home is next, not yet started
	uint32_t current = 0;
	uint32_t stage = knockedOut.seenLying ? 2 : 1;
	bool started = true;
	if (knockedOut.lieCounting)
	{
		current = 1;
		stage = 1;
		if (*knockedOut.lieTurns <= 0)
		{
			current = 2;
			stage = 0;
			started = false;
		}
	}
	gamesave::KeptCreaturePlan plan {.desire = static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer),
	                                 .action = k_DieAction,
	                                 .objects = {gamesave::Ref {}, actedOn, gamesave::Ref {}},
	                                 .firstWord = started ? 0U : 1U,
	                                 .current = current,
	                                 .state = stage,
	                                 .count = static_cast<uint32_t>(agenda.size()),
	                                 .words = {0, 0, 0},
	                                 .creature = self};
	plan.steps.assign(agenda.begin() + current, agenda.end());
	return plan;
}

std::optional<gamesave::KeptCreaturePlan>
openblack::ecs::saved_world_write::ScriptHeldPlanOf(const components::CreatureMindState& mind, gamesave::Ref self,
                                                    const std::function<gamesave::Ref(uint32_t)>& refOf, std::string& why)
{
	// the agenda as the game clears it: a step still to start, no step, no main step
	constexpr uint32_t k_NoMainStep = 0xFFFFFFFF;
	gamesave::KeptCreaturePlan plan {.firstWord = 1, .words = {0, k_NoMainStep, 0}, .creature = self};
	const auto& idle = mind.idle;
	const auto left = idle.step < idle.agenda.size() ? idle.agenda.size() - idle.step : 0;
	// no plan and nothing left to do: the plan a finished one leaves, its desire the one for none
	if (!mind.planner.current.has_value() && left == 0)
	{
		plan.desire = k_NoDesire;
		return plan;
	}
	const auto& current = mind.planner.current;
	if (current.has_value() && current->desire == creature_desires::Desire::ObeyPlayer && current->forced &&
	    current->action != creature_planner::k_FollowPlayerAction)
	{
		return ForcedPlanOf(mind, *current, self, refOf, why);
	}
	const auto groundAt = [](glm::vec2 at) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
	};
	// its own plan of sleeping on the spot, done as the game's plain sleep at home
	if (current.has_value() && !current->forced && left != 0)
	{
		if (auto own = OwnSleepPlanOf(mind, *current, self, refOf, groundAt, why); own.has_value())
		{
			return own;
		}
	}
	// its own plan of fishing and eating the catch
	if (current.has_value() && !current->forced && left != 0)
	{
		if (auto own = FishAndEatPlanOf(mind, *current, self, refOf); own.has_value())
		{
			return own;
		}
	}
	// its plan of looking over a miracle's place, its own or the one a nice miracle forces on it
	if (current.has_value() && left != 0)
	{
		if (auto own = ExaminePlacePlanOf(mind, *current, self, refOf, groundAt); own.has_value())
		{
			return own;
		}
	}
	// a dance with villagers it carries out, its own plan's: the dance's agenda, the last of its main steps the point at
	// the place
	if (current.has_value() && left != 0)
	{
		if (auto steps = DanceStepsOf(idle.agenda, idle.step, groundAt, refOf); steps.has_value())
		{
			constexpr uint32_t k_LastMainDanceStep = 8;
			plan.desire = static_cast<uint32_t>(current->desire);
			plan.action = current->action;
			plan.objects = {RefOf(current->activityObject, refOf), RefOf(current->object, refOf),
			                RefOf(current->instrument, refOf)};
			plan.count = static_cast<uint32_t>(idle.agenda.size());
			plan.current = static_cast<uint32_t>(idle.step);
			plan.state = idle.stepStarted ? 1 : 0;
			plan.firstWord = idle.stepStarted ? 0 : 1;
			plan.words = {0, k_LastMainDanceStep, 0};
			plan.steps = std::move(*steps);
			return plan;
		}
	}
	// a script's play ends waiting until the body is free; a script's move is its walks to the point
	const auto isMoveLeg = [](const creature_mind::Step& step) {
		return step.kind == creature_mind::Step::Kind::Move && step.movement.kind == creature_mind::Movement::Kind::ToPoint &&
		       step.movement.pointHeight.has_value();
	};
	const bool play = left != 0 && idle.agenda.back().kind == creature_mind::Step::Kind::WaitUntilFree;
	const bool move = left != 0 && std::ranges::all_of(idle.agenda, isMoveLeg);
	// a script's focus turns it to face its point, or the thing it focuses on, in one step
	const bool focus = left == 1 && (idle.agenda[idle.step].kind == creature_mind::Step::Kind::TurnToFacePoint ||
	                                 (idle.agenda[idle.step].kind == creature_mind::Step::Kind::Move &&
	                                  idle.agenda[idle.step].movement.kind == creature_mind::Movement::Kind::TurnToFaceObject));
	if (!current.has_value() || current->desire != creature_desires::Desire::ObeyPlayer ||
	    current->action != creature_planner::k_FollowPlayerAction || idle.activity != creature_mind::Activity::Told ||
	    (!play && !move && !focus))
	{
		// the kinds of the steps left, and of their movements, so that the refusal says what the script had it do
		std::string kinds;
		for (size_t i = idle.step; i < idle.agenda.size(); ++i)
		{
			kinds += fmt::format("{}{}/{}", kinds.empty() ? "" : " ", static_cast<int>(idle.agenda[i].kind),
			                     static_cast<int>(idle.agenda[i].movement.kind));
		}
		why = current.has_value() ? fmt::format("not a script's play (desire {}, action {}, activity {}, {} steps left: {})",
		                                        static_cast<uint32_t>(current->desire), current->action,
		                                        static_cast<int>(idle.activity), left, kinds)
		                          : fmt::format("not a script's play (no plan, {} steps left: {})", left, kinds);
		return std::nullopt;
	}
	// its plan of obeying its player, about itself
	plan.desire = static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer);
	plan.action = creature_planner::k_FollowPlayerAction;
	plan.objects = {gamesave::Ref {}, self, gamesave::Ref {}};
	plan.count = static_cast<uint32_t>(idle.agenda.size());
	plan.current = static_cast<uint32_t>(idle.step);
	// a step under way is past its start
	plan.state = idle.stepStarted ? 1 : 0;
	plan.firstWord = idle.stepStarted ? 0 : 1;
	for (size_t i = idle.step; i < idle.agenda.size(); ++i)
	{
		const auto& step = idle.agenda[i];
		switch (step.kind)
		{
		case creature_mind::Step::Kind::Action:
			plan.steps.push_back({.action = k_PlayIndividualStep, .integer = static_cast<uint32_t>(step.animation)});
			break;
		case creature_mind::Step::Kind::Static:
			plan.steps.push_back({.action = k_PlayStaticStep,
			                      .integer = static_cast<uint32_t>(step.animation),
			                      .real = creature_script_play::k_StaticActionSeconds});
			break;
		case creature_mind::Step::Kind::WaitUntilFree:
			plan.steps.push_back({.action = k_WaitUntilFreeStep});
			break;
		case creature_mind::Step::Kind::TurnToFacePoint:
			// the turn a script's focus on a point makes, to the point it focuses on (its height as the script gave it)
			plan.steps.push_back({.action = k_TurnToFacePosStep,
			                      .point = mind.scriptFocus.has_value()
			                                   ? gamesave::Point {mind.scriptFocus->x, mind.scriptFocus->y, mind.scriptFocus->z}
			                                   : gamesave::Point {step.movement.point.x, 0.0f, step.movement.point.y}});
			break;
		case creature_mind::Step::Kind::Move:
			if (step.movement.kind == creature_mind::Movement::Kind::TurnToFaceObject)
			{
				// the turn a script's focus on a thing makes, held its seconds
				plan.steps.push_back(
				    {.action = k_TurnToFaceObjectStep, .object = RefOf(step.movement.object, refOf), .real = step.seconds});
				break;
			}
			// a leg of the script's move: a walk to within its distance of the point
			plan.steps.push_back(
			    {.action = k_MoveToPosStep,
			     .point = {step.movement.point.x, step.movement.pointHeight.value_or(0.0f), step.movement.point.y},
			     .real = step.movement.maxDistance});
			break;
		default:
			why = "a step of its play is not one the script's play makes";
			return std::nullopt;
		}
	}
	return plan;
}

namespace
{
/// The creature record its load kept on it, if any
std::optional<gamesave::CreatureRecord> KeptCreatureOf(const WriteContext& context, entt::entity creature)
{
	if (auto kept = context.KeptRecordOf(creature, gamesave::CreatureRecord::k_Type); kept.has_value())
	{
		if (auto* record = std::get_if<gamesave::CreatureRecord>(&*kept); record != nullptr)
		{
			return std::move(*record);
		}
	}
	return std::nullopt;
}

/// The species' row of the game's creature table, none without the game's tables
const GCreatureInfo* SpeciesRowOf(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& rows = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < rows.size() ? &rows.at(row) : nullptr;
}

/// A creature: the parts every object writes, a living thing's, then its own values
std::optional<gamesave::SavedRecord> WriteCreature(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& body = registry.Get<const components::Creature>(entity);
	const auto* row = SpeciesRowOf(body.species);
	if (row == nullptr)
	{
		context.Fail(NameOf(entity) + " has no row of its species in the game's creature table");
		return std::nullopt;
	}
	const auto kept = KeptCreatureOf(context, entity);
	gamesave::CreatureRecord record = kept.value_or(gamesave::CreatureRecord {});

	// its thing's flags: as kept, none on a creature made in play (openblack does not hold a script's taking it yet)

	record.position = PositionPartOf(context, entity, kept.has_value() ? &kept->position : nullptr);
	if ((record.position.flags & position_flags::k_InPhysics) != 0)
	{
		context.Fail(NameOf(entity) + " is in the physics: its body there is not written from openblack yet");
		return std::nullopt;
	}
	// its object's place is where it stood as its turn began, as every living thing copies it then; one that has had
	// no turn yet stands where it was made
	gamesave::MapCoords target = kept.has_value() ? kept->object.coords : record.position.position;
	if (const auto* drawn = registry.TryGet<const components::DrawPosition>(entity); drawn != nullptr && drawn->started)
	{
		const auto keptAt =
		    kept.has_value() ? map_coords::ToWorld({.x = kept->object.coords.x, .z = kept->object.coords.z, .altitude = 0.0f})
		                     : glm::vec3(0.0f);
		if (!kept.has_value() || keptAt.x != drawn->turnStart.x || keptAt.z != drawn->turnStart.z)
		{
			target.x = map_coords::ToFixed(drawn->turnStart.x);
			target.z = map_coords::ToFixed(drawn->turnStart.z);
		}
	}
	const auto info = k_FirstCreatureRow - static_cast<uint32_t>(creature::InfoRow(body.species));
	record.object = ObjectPartOf(context, entity, kept.has_value() ? &kept->object : nullptr, target, info);
	// its object's scale is the size its body had as it was made (the species' start size, kept within the body's
	// limits), and its object's angle stays as made: the creature's size and turns are its body's, not its object's
	record.object.scale = kept.has_value() ? kept->object.scale : creature_morph::ClampScale(row->startScale);
	record.object.heading = kept.has_value() ? kept->object.heading : 0.0f;

	// the moving thing's, the walk's and the living thing's parts, else as its constructor leaves them
	LivingParts keptParts;
	if (kept.has_value())
	{
		keptParts = {.mobile = kept->mobile, .move = kept->move, .living = kept->living};
	}
	auto parts = LivingPartsOf(context, entity, kept.has_value() ? &keptParts : nullptr, LivingConstructorParts(*row));
	if (!parts.has_value())
	{
		return std::nullopt;
	}
	record.mobile = parts->mobile;
	record.move = std::move(parts->move);
	record.living = std::move(parts->living);
	// the flock it is kept in, none once that flock has gone: a deleted flock takes itself off each of its members
	record.living.flock.number = context.UnlessGone(record.living.flock.number);

	auto fields = CreatureFieldsOf(context, entity, kept.has_value() ? &kept->creature : nullptr);
	if (!fields.has_value())
	{
		return std::nullopt;
	}
	record.creature = std::move(*fields);
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterCreatureWriters(ClassWriters& writers)
{
	writers.Register(
	    gamesave::CreatureRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::Creature>(entity); },
	    WriteCreature);
}

void openblack::ecs::saved_world_write::ZeroFizzForSave(Registry& registry, entt::entity creature)
{
	auto* fizz = registry.TryGet<components::CreatureFizz>(creature);
	if (fizz != nullptr && InvisibleOff(registry.TryGet<const components::CreatureSpells>(creature)))
	{
		fizz->fizz.now = 0.0f;
		fizz->fizz.target = 0.0f;
		fizz->fizz.perSecond = 0.0f;
	}
}
