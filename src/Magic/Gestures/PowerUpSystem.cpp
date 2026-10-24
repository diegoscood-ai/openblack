/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PowerUpSystem.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Camera/CameraShake.h"
#include "Creature/LocalPlayer.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/AudioQueries.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "GameClock.h"
#include "GestureIconModels.h"
#include "GestureIcons.h"
#include "GestureInput.h"
#include "GestureShapes.h"
#include "Help/HelpProfile.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GamePackets.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/MagicTables.h"
#include "Particles/Rules/SurfRevol.h"
#include "Particles/Utility.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::magic::gestures;

namespace
{
/// The pause after any recognition or cancel
constexpr float k_Cooldown = 0.4f;
/// the circle is forgotten 5 s after it was drawn
constexpr float k_CircleLife = 5.0f;

/// The gesture state machine, the icon provider worship sets on every land, and the hand as the gesture code sees it
/// (gestures::Reset clears only the first)
struct PowerUpState
{
	InterfaceGestures gestures;
	std::unique_ptr<IconProvider> icons;
	HandStatus hand;
};

/// This module's state (the miracles' service's hand magic state)
PowerUpState& PowerUp()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("magic::gestures: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value().HandMagic().Get<PowerUpState>();
}

bool Trace()
{
	static const bool trace = debug_env::GestureTrace();
	return debug_env::TraceLogs(trace);
}

const ecs::components::SpellSeed* HeldSeed()
{
	if (PowerUp().hand.heldSeed == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(PowerUp().hand.heldSeed))
	{
		return nullptr;
	}
	return registry.TryGet<ecs::components::SpellSeed>(PowerUp().hand.heldSeed);
}

/// The gestures' help events (Help/HelpProfile.h): 14 and 15 from ProcessPowerUpSystem, 16, 17 and 21 from the
/// selection stage, 18, 20 and 22 from the power-up gestures. (pending, creature) 19, the creature's fight gesture, is
/// not ported
void Help(int event)
{
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: help event 0x{:X}", event);
	}
	help_profile::Trigger(static_cast<help_profile::Event>(event));
}

void RemoveFromHandFx()
{
	if (PowerUp().hand.removeFromHandFx)
	{
		PowerUp().hand.removeFromHandFx();
	}
}

void ForceDropHeld()
{
	if (PowerUp().hand.forceDropHeld)
	{
		PowerUp().hand.forceDropHeld();
	}
}

/// The leash of the player's creature, read once a pass. A creature's leash is always held in its owner's hand: the
/// leash system lets no one else lead it
LeashHand HandLeash(PlayerNames player)
{
	if (!Locator::leashSystem::has_value())
	{
		return {};
	}
	const auto& leashes = Locator::leashSystem::value();
	const auto creature = leashes.PlayersCreature(player);
	if (!creature.has_value())
	{
		return {};
	}
	// before any pick the hand's leash is the rope, the one the leash key puts on
	const auto picked = leashes.Picked(*creature);
	LeashHand leash {
	    .creature = true,
	    .known = {leashes.Knows(*creature, LeashType::Evil), leashes.Knows(*creature, LeashType::Rope),
	              leashes.Knows(*creature, LeashType::Good)},
	    .picked = picked == LeashType::None ? LeashType::Rope : picked,
	};
	if (leashes.IsLeashed(*creature))
	{
		leash.holder = player;
		leash.tied = leashes.TiedTo(*creature).has_value();
	}
	return leash;
}

/// One line for each time the leash picker opens or closes, and for each leash it picks
void PickerLog(std::string_view what)
{
	if (auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "Gestures: leash picker {}", what);
	}
}

/// The first land of a single player game that is not a playground: openblack has no multiplayer game, and no
/// playground game flag
bool OnSecretScriptLand()
{
	if (!Locator::mapScriptSystem::has_value())
	{
		return false;
	}
	return SecretScriptLand(Locator::mapScriptSystem::value().Globals().landNumber, false, false);
}

/// The square wave in the leash picker starts the first land's secret script, with the script types a single player
/// game allows; without the script runtime nothing starts
void StartSecretScript()
{
	constexpr std::string_view k_SecretScript = "HiddenScript";
	PickerLog(fmt::format("square wave: starts the script {}", k_SecretScript));
	if (Locator::vm::has_value())
	{
		const auto types = static_cast<lhvm::ScriptType>(help::script_control::k_SinglePlayerScriptTypes);
		Locator::vm::value().StartScript(std::string(k_SecretScript), types);
	}
}

LookingForArray FreshLookingFor()
{
	LookingForArray lf;
	for (auto& entry : lf)
	{
		entry = LookingFor {0, -1, -1, -1};
	}
	return lf;
}

/// The selection stays open only while the hand is ready for an object
bool SelectionOpenAndHandReady()
{
	const bool open = PowerUp().gestures.selection.open && PowerUp().hand.handReady;
	PowerUp().gestures.selection.open = open;
	return open;
}

/// The scribble (power down, or shake the seed out) and the seed's power-up gestures
bool PowerUpGestures(LookingForArray& lf)
{
	const auto* seed = HeldSeed();
	if (seed == nullptr)
	{
		return false;
	}
	auto& s = PowerUp().gestures;
	const auto outcome = PowerUpStage(
	    s.currentPowerUpGesture, s.powerUpGestures, static_cast<int>(seed->seedType), seed->icon != entt::null,
	    [](Gesture g) { return Recognise(g); }, lf);
	switch (outcome.kind)
	{
	case PowerUpStageOutcome::Kind::None:
		return false;
	case PowerUpStageOutcome::Kind::PoweredDown:
		RemoveFromHandFx();
		Success(false);
		s.gesture.gesture = k_None;
		TrySetupPowerUpGestures();
		Help(0x14);
		if (PowerUp().icons != nullptr)
		{
			PowerUp().icons->SetPowerUpCharge(seed->icon, -1);
		}
		return true;
	case PowerUpStageOutcome::Kind::ShakenOut:
		RemoveFromHandFx();
		Success(false);
		s.gesture.gesture = k_None;
		Help(0x16);
		ForceDropHeld();
		return true;
	case PowerUpStageOutcome::Kind::PoweredUp:
		Success(true);
		TrySetupPowerUpGestures();
		Help(0x12);
		s.currentPowerUpGesture = outcome.gesture;
		if (PowerUp().icons != nullptr)
		{
			PowerUp().icons->SetPowerUpCharge(seed->icon, outcome.level);
		}
		s.gesture.gesture = outcome.gesture;
		return true;
	}
	return false;
}
} // namespace

void gestures::SetIconProvider(std::unique_ptr<IconProvider> provider)
{
	PowerUp().icons = std::move(provider);
}

IconProvider* gestures::GetIconProvider()
{
	return PowerUp().icons.get();
}

PowerUpStageOutcome gestures::PowerUpStage(Gesture current, const std::array<Gesture, 3>& powerUps, int seedIndex,
                                           bool fromIcon, const std::function<bool(Gesture)>& recognise, LookingForArray& lf)
{
	if (current != k_None)
	{
		lf[k_Scribble] = LookingFor {0xD, seedIndex, lf[k_Scribble].leash, -1};
		if (recognise(k_Scribble))
		{
			return {.kind = PowerUpStageOutcome::Kind::PoweredDown};
		}
	}
	else
	{
		lf[k_Scribble].type = 0;
		if (recognise(k_Scribble))
		{
			return {.kind = PowerUpStageOutcome::Kind::ShakenOut};
		}
	}
	for (int k = 0; k < 3; ++k)
	{
		const Gesture g = powerUps[static_cast<size_t>(k)];
		if (g == k_None || g == current)
		{
			continue;
		}
		lf[g] = LookingFor {0xE + k, seedIndex, lf[g].leash, k};
		if (recognise(g) && fromIcon)
		{
			return {.kind = PowerUpStageOutcome::Kind::PoweredUp, .level = k, .gesture = g};
		}
	}
	return {};
}

bool gestures::RepeatStage(Gesture repeat, int lastSeedType, const std::function<bool(Gesture)>& recognise, LookingForArray& lf)
{
	if (repeat < k_GestureCount)
	{
		lf[repeat].type = 0xA;
		lf[repeat].seed = lastSeedType;
	}
	return recognise(repeat);
}

SelectionTables SelectionTables::FromInfo()
{
	SelectionTables tables;
	auto field = [](int seedType, auto member) -> Gesture {
		if (seedType < 0 || seedType >= static_cast<int>(k_SpellSeedCount) || !Locator::infoConstants::has_value())
		{
			return k_None;
		}
		const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), static_cast<SpellSeedType>(seedType));
		return static_cast<Gesture>(info.*member);
	};
	tables.selectionGesture = [field](int seed) { return field(seed, &GSpellSeedInfo::selectionGesture); };
	tables.gesture = [field](int seed) { return field(seed, &GSpellSeedInfo::gesture); };
	tables.gestureStage2 = [field](int seed) { return field(seed, &GSpellSeedInfo::gestureStage2); };
	return tables;
}

bool Selection::Open(Gesture openCategory, const IconProvider& icons, const SelectionTables& tables)
{
	activeGesture.fill(false);
	candidate.fill(false);
	for (auto& stages : seedStage)
	{
		stages.fill(k_None);
	}
	bool found = false;
	icons.ForEachRequestableIcon(openCategory, [&](int seedType) {
		if (seedType < 0 || seedType >= static_cast<int>(k_Seeds))
		{
			return;
		}
		const Gesture selection = tables.selectionGesture(seedType);
		if (selection == k_None || selection != openCategory)
		{
			return;
		}
		const Gesture gesture = tables.gesture(seedType);
		seedStage[static_cast<size_t>(seedType)] = {selection, gesture, tables.gestureStage2(seedType)};
		candidate[static_cast<size_t>(seedType)] = true;
		if (gesture < k_GestureCount)
		{
			activeGesture[gesture] = true;
		}
		found = true;
	});
	stage = 1;
	open = found;
	timer = 0.0f;
	category = openCategory;
	return found;
}

Selection::Outcome Selection::Stage(float dt, float timeOut, const std::function<bool(Gesture)>& recognise, IconProvider& icons,
                                    LookingForArray* lookingFor)
{
	// dt is 0 while paused
	timer += dt;
	if (timer > timeOut)
	{
		open = false; // the stage still runs this time
	}
	if (lookingFor != nullptr)
	{
		(*lookingFor)[k_Scribble].type = 0;
	}
	if (recognise(k_Scribble))
	{
		open = false;
		return Outcome::Cancelled;
	}
	if (lookingFor != nullptr)
	{
		for (size_t g = 1; g < k_GestureCount; ++g)
		{
			if (activeGesture[g])
			{
				(*lookingFor)[g].type = 0xB;
			}
		}
	}
	auto fill = [this, lookingFor]() {
		if (lookingFor == nullptr)
		{
			return;
		}
		for (size_t s = 0; s < k_Seeds; ++s)
		{
			if (candidate[s] && stage < 3 && seedStage[s][stage] < k_GestureCount)
			{
				(*lookingFor)[seedStage[s][stage]].seed = static_cast<int>(s);
			}
		}
	};
	bool anyActive = false;
	Gesture recognised = k_None;
	for (Gesture g = 1; g < k_GestureCount; ++g)
	{
		if (!activeGesture[g])
		{
			continue;
		}
		anyActive = true;
		if (recognise(g))
		{
			recognised = g;
			break;
		}
	}
	if (recognised == k_None)
	{
		if (!anyActive)
		{
			open = false;
			return Outcome::None;
		}
		fill();
		return Outcome::None;
	}
	bool first = false;
	for (size_t s = 0; s < k_Seeds; ++s)
	{
		if (!candidate[s])
		{
			continue;
		}
		if (stage < 3 && seedStage[s][stage] == recognised && icons.IconValidForRequest(static_cast<int>(s)))
		{
			if (!first)
			{
				first = true; // Success(1), help 0x10, gesture cleared and the timer back to 0 (the caller)
				timer = 0.0f;
			}
			candidate[s] = true;
			if (stage + 1 == 3 || seedStage[s][stage + 1] == k_None)
			{
				open = false;
				icons.RequestSpell(static_cast<int>(s)); // help 0x11
				return Outcome::Requested;
			}
		}
		else
		{
			candidate[s] = false;
		}
	}
	++stage;
	activeGesture.fill(false);
	bool any = false;
	for (size_t s = 0; s < k_Seeds; ++s)
	{
		if (!candidate[s])
		{
			continue;
		}
		if (icons.IconValidForRequest(static_cast<int>(s)) && stage < 3)
		{
			activeGesture[seedStage[s][stage] < k_GestureCount ? seedStage[s][stage] : 0] = true;
			any = true;
		}
		else
		{
			candidate[s] = false;
		}
	}
	if (!any)
	{
		open = false;
	}
	fill();
	return first ? Outcome::StageOk : Outcome::None;
}

LeashSelectionTables LeashSelectionTables::FromInfo()
{
	LeashSelectionTables tables;
	if (!Locator::infoConstants::has_value())
	{
		return tables;
	}
	const auto& system = Locator::infoConstants::value().spellSystem;
	tables.timeOut = system.leashSelectionSystemTimeOut;
	tables.start = static_cast<Gesture>(system.leashSelectionStart);
	std::ranges::transform(system.leashSelectionGestures, tables.pickGestures.begin(),
	                       [](GestureType g) { return static_cast<Gesture>(g); });
	return tables;
}

int gestures::KnownLeashCount(const LeashHand& leash)
{
	return static_cast<int>(std::ranges::count(leash.known, true));
}

bool gestures::LeashStartWanted(const LeashHand& leash, PlayerNames player)
{
	const int known = KnownLeashCount(leash);
	return (leash.holder == player && known > 1) || (!leash.holder.has_value() && known >= 1);
}

void LeashSelection::Open(Selection& selection)
{
	timer = 0.0f;
	selection.open = false;
	open = true;
}

bool LeashSelection::Active(const LeashHand& leash, PlayerNames player)
{
	if (!leash.creature || (!pending && leash.holder != player))
	{
		open = false;
	}
	return open;
}

bool LeashSelection::Start(const LeashHand& leash, Selection& selection)
{
	const bool several = KnownLeashCount(leash) > 1;
	if (!leash.holder.has_value())
	{
		// the click puts the leash on at the next turn, and the picker waits for it
		if (several)
		{
			pending = true;
			Open(selection);
		}
		return true;
	}
	if (several)
	{
		pending = false;
		Open(selection);
	}
	return false;
}

void LeashSelection::LeashClickArrived()
{
	pending = false;
}

LeashSelection::Outcome LeashSelection::Stage(float dt, bool paused, LeashSelectionTables tables,
                                              const std::function<bool(Gesture)>& recognise, const LeashHand& leash,
                                              PlayerNames player, bool secretScriptLand, LookingForArray* lf)
{
	if (!leash.creature)
	{
		open = false;
		return {.kind = Outcome::Kind::NoCreature};
	}
	if (!paused)
	{
		timer += dt;
	}
	if (timer > tables.timeOut)
	{
		open = false; // this pass still looks for the gestures
	}
	if (leash.holder != player)
	{
		return {};
	}
	if (lf != nullptr)
	{
		(*lf)[k_Scribble].type = 0;
	}
	if (recognise(k_Scribble))
	{
		open = false;
		return {.kind = Outcome::Kind::Cancelled};
	}
	// the square wave, not shown among the gestures looked for, starts the first land's secret script; the wiped
	// buffer recognises nothing more, and the rest of the pass goes on
	Outcome outcome;
	outcome.startSecretScript = secretScriptLand && recognise(k_SquareWave);
	for (const auto type : {LeashType::Evil, LeashType::Rope, LeashType::Good})
	{
		const auto index = static_cast<size_t>(type);
		const Gesture g = tables.pickGestures[index];
		if (!leash.known[index - 1] || leash.picked == type || g == k_None)
		{
			continue;
		}
		if (lf != nullptr && g < k_GestureCount)
		{
			(*lf)[g].type = static_cast<int>(index) + 3;
			(*lf)[g].leash = static_cast<int>(index);
		}
		if (!outcome.startSecretScript && recognise(g))
		{
			open = false;
			return {.kind = Outcome::Kind::Picked, .leash = type};
		}
	}
	return outcome;
}

GestureStage gestures::FirstStage(Selection& selection, LeashSelection& picker, bool charging, bool ready,
                                  const LeashHand& leash, PlayerNames player, const FightGestures& fight)
{
	if (fight.fighting && fight.controlsUp)
	{
		if (fight.moveHeld)
		{
			return GestureStage::FightMoveHeld;
		}
		selection.open = false;
		picker.open = false;
		return GestureStage::Fight;
	}
	if (charging)
	{
		selection.open = false;
		picker.open = false;
		return GestureStage::PowerUp;
	}
	selection.open = selection.open && ready;
	if (selection.open)
	{
		picker.open = false;
		return GestureStage::Selection;
	}
	return picker.Active(leash, player) ? GestureStage::LeashPicker : GestureStage::None;
}

FightGestures gestures::CurrentFight()
{
	FightGestures fight;
	if (Locator::leashSystem::has_value() && Locator::creatureFightSystem::has_value())
	{
		const auto own = Locator::leashSystem::value().PlayersCreature(creature::LocalPlayer());
		fight.fighting = own.has_value() && Locator::creatureFightSystem::value().IsDuelling(*own);
	}
	if (fight.fighting)
	{
		fight.controlsUp = ecs::audio_queries::LocalPlayerDirectsFight();
		fight.moveHeld = fight.controlsUp && Locator::gameActionSystem::has_value() &&
		                 Locator::gameActionSystem::value().Get(input::BindableActionMap::MOVE);
	}
	return fight;
}

InterfaceGestures& gestures::State()
{
	return PowerUp().gestures;
}

void gestures::Reset()
{
	PowerUp().gestures = InterfaceGestures {};
	// the gesture icons go with the map, and so do the leashes' spins inside them; their fade stays
	IconState().list.Clear();
	ResetIconModels();
}

void gestures::SetHandStatus(HandStatus status)
{
	PowerUp().hand = std::move(status);
}

const HandStatus& gestures::GetHandStatus()
{
	return PowerUp().hand;
}

void gestures::ClearBuffer()
{
	PowerUp().gestures.system.Clear();
}

void gestures::FeedSample(glm::ivec2 mouse)
{
	// A restricted input mode (inferred: taken as the pause), and the cooldown
	if (PowerUp().hand.paused || PowerUp().gestures.cooldown > 0.0f)
	{
		return;
	}
	// the interface's land under the message's pixel
	const auto world = sampling::InterfaceLandUnderPixel(mouse);
	// (0, 0, 0) means off the land: kept when any |c| > 1e-4, as GestureBuffer's k_Zero (a component that is not a
	// number counts as near 0)
	if (world && (std::abs(world->x) > 1e-4f || std::abs(world->y) > 1e-4f || std::abs(world->z) > 1e-4f))
	{
		PowerUp().gestures.system.AddSample(*world, mouse);
	}
	else
	{
		PowerUp().gestures.system.AddSampleAtLastWorld(mouse);
	}
}

void gestures::ReseedBuffer()
{
	ClearBuffer();
	FeedSample(PowerUp().hand.mouse);
}

bool gestures::Recognise(Gesture gesture)
{
	const float ratio = sampling::ScreenRatio();
	const auto data = BuildFromSystem(PowerUp().gestures.system, ratio);
	return MatchGesture(Templates(), gesture, data, PowerUp().gestures.result, ratio);
}

void gestures::Success(bool success)
{
	if (success)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: recognised {} (reversed {}, keypoints {}..{}, template {})",
			                   static_cast<int>(PowerUp().gestures.result.gesture), PowerUp().gestures.result.reversed,
			                   PowerUp().gestures.result.start, PowerUp().gestures.result.end,
			                   PowerUp().gestures.result.templateIndex);
		}
		// The sparkles along the stroke and the light sheet (gesture particles), in the colour of this interface's player;
		// force feedback is not ported
		particles::utility::GestureRecognised(PowerUp().gestures.system, PowerUp().gestures.result, creature::LocalPlayer());
	}
	ClearBuffer();
	PowerUp().gestures.cooldown = k_Cooldown;
}

bool gestures::TrySetupPowerUpGestures()
{
	auto& s = PowerUp().gestures;
	s.circlePending = false;
	s.circleTimer = 0.0f;
	const auto* seed = HeldSeed();
	if (seed == nullptr)
	{
		return false;
	}
	const auto& info = seed::InfoOf(*seed);
	s.currentPowerUpGesture = k_None;
	bool any = false;
	for (size_t k = 0; k < 3; ++k)
	{
		s.powerUpGestures[k] = k_None;
		const MagicType magic = info.magicTypes[k + 1];
		if (magic != MagicType::None && players::IsMagicTypeEnabled(seed->creator.player, magic) && seed->icon != entt::null &&
		    PowerUp().icons != nullptr && PowerUp().icons->PowerUpAvailable(seed->icon, static_cast<int>(k)))
		{
			s.powerUpGestures[k] = static_cast<Gesture>(info.powerUpGestures[k]);
			any = true;
		}
	}
	return any;
}

void gestures::SetupPowerUpGestures()
{
	TrySetupPowerUpGestures();
	PowerUp().gestures.showHeldGestureTrail = true;
	ReseedBuffer();
	if (const auto* seed = HeldSeed(); seed != nullptr)
	{
		// the gesture the info of the seed's magic at its level names while the game runs (none for the base level)
		const auto& tables = Locator::infoConstants::value();
		const auto& magic = MagicInfoForPowerUpLevel(tables, seed::InfoOf(*seed), seed->powerUp);
		PowerUp().gestures.currentPowerUpGesture = static_cast<Gesture>(GetGestureOfMagicInfo(tables, magic.magicType));
	}
}

bool gestures::HoldingChargingSeed()
{
	const auto* seed = HeldSeed();
	return seed != nullptr && seed->ready && seed->lastMagic == MagicType::None && seed->icon != entt::null;
}

Gesture gestures::PowerUpLevelGesture()
{
	return HoldingChargingSeed() ? PowerUp().gestures.currentPowerUpGesture : k_None;
}

float gestures::CircleSecondsLeft(const InterfaceGestures& state)
{
	return state.circlePending ? std::max(k_CircleLife - state.circleTimer, 0.0f) : 0.0f;
}

namespace
{
/// A circle a debug tool or the testbed put in as if it had been drawn, the first waiting; the other events wait on
std::optional<ecs::systems::GestureEvent> TakeInjectedCircle()
{
	if (!Locator::gestureEvents::has_value())
	{
		return std::nullopt;
	}
	auto& events = Locator::gestureEvents::value();
	std::optional<ecs::systems::GestureEvent> circle;
	for (const auto& event : events.TakeEvents())
	{
		if (!circle.has_value() && event.kind == ecs::systems::GestureEvent::Kind::Circle)
		{
			circle = event;
		}
		else
		{
			events.Inject(event);
		}
	}
	return circle;
}
} // namespace

bool gestures::IsGesturing()
{
	const auto& hand = GetHandStatus();
	const auto& state = State();
	// a button bit with no known setter and an unknown flag are not ported
	if (HoldingChargingSeed())
	{
		return true;
	}
	// a seed in the hand with the held gesture trail shown
	if (hand.heldSeed != entt::null && state.showHeldGestureTrail)
	{
		return true;
	}
	// the selection open with the hand ready
	if (state.selection.open && hand.handReady)
	{
		return true;
	}
	// a circle-sized seed in the hand
	if (hand.heldSeed != entt::null && Locator::entitiesRegistry::value().Valid(hand.heldSeed))
	{
		const auto* seed = Locator::entitiesRegistry::value().TryGet<const ecs::components::SpellSeed>(hand.heldSeed);
		if (seed != nullptr && seed::InfoOf(*seed).sizingGesture == GestureType::Circle)
		{
			return true;
		}
	}
	return false;
}

ScribbleCancel gestures::ScribbleCancelFor(const HandStatus& hand, bool iconCharging, PlayerNames player,
                                           const LeashHand& leash)
{
	if (hand.holdingSomething)
	{
		return hand.inInfluence && hand.validToShake ? ScribbleCancel::ShakeHeld : ScribbleCancel::None;
	}
	if (leash.holder == player && !leash.tied)
	{
		return ScribbleCancel::ShakeLeash;
	}
	return iconCharging ? ScribbleCancel::CancelIcon : ScribbleCancel::None;
}

PowerUpPassState gestures::CurrentPassState()
{
	return {
	    .insideCitadel = game_clock::IsInsideCitadel(),
	    .lockedSelect = Locator::handSystem::has_value() && Locator::handSystem::value().HoldsLockedSelect(),
	    .interfaceActive = interface_active::IsActive(),
	    .boxHasMouse = interface_active::BoxHasMouse(),
	};
}

void gestures::ProcessPowerUpSystem(float dt)
{
	auto& s = PowerUp().gestures;
	// The early outs that wipe the buffer: inside the citadel, a locked select in the hand, an inactive interface and a
	// box holding the mouse
	if (!PowerUpPassGoesOn(CurrentPassState()))
	{
		ClearBuffer();
		return;
	}
	// A fire seed in the hand with a fireball under the cursor is applied to it (the seed takes the ball in)
	{
		const auto& hand = PowerUp().hand;
		const auto* seed = HeldSeed();
		const bool cursorIsFireBall =
		    hand.cursorObject != entt::null && Locator::entitiesRegistry::has_value() &&
		    std::as_const(Locator::entitiesRegistry::value()).Valid(hand.cursorObject) &&
		    std::as_const(Locator::entitiesRegistry::value()).AllOf<ecs::components::MagicFireBall>(hand.cursorObject);
		if (FeedsFireBall(seed != nullptr, seed != nullptr ? seed->seedType : SpellSeedType::None, cursorIsFireBall) &&
		    hand.applyHeldToCursorObject)
		{
			hand.applyHeldToCursorObject();
		}
	}
	// A moving camera wipes the gesture in progress, unless a camera shake is going on or the camera mode lets the
	// player gesture while it moves (the follow, the dance and the player's camera in an arena fight: none in openblack)
	const bool shaking = Locator::scriptState::has_value() && !camera_shake::Checkers().empty();
	if (CameraMoveWipesPath(sampling::CameraMoving(), shaking, false))
	{
		ClearBuffer();
	}
	if (!PowerUp().hand.paused && s.cooldown > 0.0f)
	{
		s.cooldown -= dt;
	}
	auto lf = FreshLookingFor();
	if (s.circlePending)
	{
		if (!PowerUp().hand.paused)
		{
			s.circleTimer += dt;
		}
		if (s.circleTimer > k_CircleLife)
		{
			s.circlePending = false;
		}
	}
	// (a) the circle that sizes a storm or a shield, while the action press lasts
	if (PowerUp().hand.actionLatched)
	{
		if (const auto* seed = HeldSeed(); seed != nullptr && !s.circlePending)
		{
			if (const auto g = static_cast<Gesture>(seed::InfoOf(*seed).sizingGesture); g != k_None)
			{
				lf[g].type = 0xC;
				// a circle put in by a tool stands for the one drawn: its centre and its radius across the land
				if (const auto injected = TakeInjectedCircle(); injected.has_value())
				{
					s.circlePosition = injected->centre;
					s.circleSize = injected->radius;
					s.circleGesture = g;
					s.gesture.gesture = g;
					s.circlePending = true;
					s.circleTimer = 0.0f;
				}
				else if (Recognise(g))
				{
					s.gesture = PacketFromResult(Templates(), s.system, s.result, sampling::CurrentProjection());
					s.circlePosition = s.gesture.position;
					s.circleSize = s.gesture.size;
					s.circleGesture = g;
					s.gesture.gesture = g;
					s.circlePending = true;
					s.circleTimer = 0.0f;
					if (Trace())
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: circle at ({:.1f}, {:.1f}, {:.1f}) size {:.1f}",
						                   s.circlePosition.x, s.circlePosition.y, s.circlePosition.z, s.circleSize);
					}
					Success(true);
				}
			}
		}
	}
	// while the local player's creature fights, the selections and the leash are not looked for
	const auto fight = CurrentFight();
	const bool fighting = fight.fighting;
	auto recognise = [](Gesture g) { return Recognise(g); };
	const auto player = creature::LocalPlayer();
	const auto leash = HandLeash(player);
	const auto leashTables = LeashSelectionTables::FromInfo();
	auto& picker = s.leashSelection;
	const bool pickerWasOpen = picker.open;
	const auto stage = FirstStage(s.selection, picker, HoldingChargingSeed(), PowerUp().hand.handReady, leash, player, fight);
	if (pickerWasOpen && !picker.open)
	{
		PickerLog(stage == GestureStage::Fight       ? "closed: the creature fights"
		          : stage == GestureStage::PowerUp   ? "closed: a charging seed in the hand"
		          : stage == GestureStage::Selection ? "closed: the miracle selection is open"
		                                             : "closed: the leash is not in the hand");
	}
	if (stage == GestureStage::FightMoveHeld)
	{
		// the move button in the fight is for the fight's own moves: the path is wiped
		ClearBuffer();
		return;
	}
	// (not ported) the fight's own gestures, the special move and the miracles the creature can cast in a fight, need
	// the player's fight controls; with none of them recognised the pass goes on to the scribble
	if (stage == GestureStage::PowerUp)
	{
		if (PowerUpGestures(lf))
		{
			return;
		}
	}
	else if (stage == GestureStage::Selection)
	{
		if (PowerUp().icons != nullptr)
		{
			const auto outcome = s.selection.Stage(dt, Locator::infoConstants::value().spellSystem.selectionSystemTimeOut,
			                                       recognise, *PowerUp().icons, &lf);
			switch (outcome)
			{
			case Selection::Outcome::Cancelled:
				RemoveFromHandFx();
				Success(false);
				Help(0x15);
				s.gesture.gesture = k_None;
				return;
			case Selection::Outcome::Requested:
				Success(true);
				Help(0x10);
				Help(0x11);
				s.gesture.gesture = k_None;
				return;
			case Selection::Outcome::StageOk:
				Success(true);
				Help(0x10);
				s.gesture.gesture = k_None;
				break;
			case Selection::Outcome::None:
				break;
			}
		}
	}
	else if (stage == GestureStage::LeashPicker)
	{
		const auto outcome =
		    picker.Stage(dt, PowerUp().hand.paused, leashTables, recognise, leash, player, OnSecretScriptLand(), &lf);
		switch (outcome.kind)
		{
		case LeashSelection::Outcome::Kind::GoesOn:
			if (outcome.startSecretScript)
			{
				ClearBuffer();
				StartSecretScript();
			}
			if (!picker.open)
			{
				PickerLog("closed: time out");
			}
			break;
		case LeashSelection::Outcome::Kind::NoCreature:
			PickerLog("closed: no creature");
			return;
		case LeashSelection::Outcome::Kind::Cancelled:
			PickerLog("closed: scribble");
			RemoveFromHandFx();
			Success(false);
			s.gesture.gesture = k_None;
			return;
		case LeashSelection::Outcome::Kind::Picked:
			PickerLog(fmt::format("picked leash {}", static_cast<int>(outcome.leash)));
			Success(true);
			s.gesture.gesture = k_None;
			// the leash goes on, or changes, at the next game turn, through the leash type's packet
			const auto picked = static_cast<int32_t>(outcome.leash);
			game_packets::Push({.type = game_packets::Type::LeashType, .value = picked, .player = player});
			return;
		}
	}
	// (b) SCRIBBLE cancels: the object in the hand is shaken out, the leash held in the hand is shaken off, or an
	// icon's charge is cancelled
	if (!HoldingChargingSeed())
	{
		const bool iconCharging = PowerUp().icons != nullptr && PowerUp().icons->AnyIconChargingForHand();
		const auto branch = ScribbleCancelFor(PowerUp().hand, iconCharging, player, leash);
		if (branch != ScribbleCancel::None)
		{
			lf[k_Scribble].type = 0;
			if (Recognise(k_Scribble))
			{
				RemoveFromHandFx();
				Success(false);
				switch (branch)
				{
				case ScribbleCancel::ShakeHeld:
					ForceDropHeld();
					break;
				case ScribbleCancel::ShakeLeash:
					// The leash comes off at the next game turn, through the click on the creature's packet
					game_packets::Push({.type = game_packets::Type::CreatureLeashClick, .player = player});
					break;
				case ScribbleCancel::CancelIcon:
					PowerUp().icons->CancelMostChargedIcon();
					break;
				case ScribbleCancel::None:
					break;
				}
				return;
			}
		}
	}
	const auto& system = Locator::infoConstants::value().spellSystem;
	// (c) the spiral opens the player's miracle selection, the inverse spiral the creature's
	if (!fighting && PowerUp().hand.handReady && PowerUp().icons != nullptr)
	{
		for (const auto category : {static_cast<Gesture>(system.selectionSystemGestureNonCreature),
		                            static_cast<Gesture>(system.selectionSystemGestureCreature)})
		{
			if (!PowerUp().icons->AnyRequestableIconOfCategory(category) ||
			    (s.selection.open && s.selection.category == category))
			{
				continue;
			}
			if (category < k_GestureCount)
			{
				lf[category].type = category == static_cast<Gesture>(system.selectionSystemGestureNonCreature) ? 8 : 9;
			}
			if (Recognise(category) && category != k_None)
			{
				const bool opened = s.selection.Open(category, *PowerUp().icons, SelectionTables::FromInfo());
				s.currentPowerUpGesture = k_None; // opening the selection clears it
				if (opened)
				{
					s.showHeldGestureTrail = true;
					Success(true);
					Help(0xF);
					s.gesture.gesture = k_None;
				}
				return;
			}
		}
	}
	// (d) R repeats the last miracle
	const auto repeat = static_cast<Gesture>(system.selectionSystemGestureR);
	// the selection's test closes it when the hand is not ready, so it runs only where it did before
	const bool selectionOpenAndReady = !fighting && repeat != k_None && SelectionOpenAndHandReady();
	if (PowerUp().icons != nullptr &&
	    RepeatLookedFor(fighting, repeat, selectionOpenAndReady, PowerUp().hand.handReady, PowerUp().icons->CanRepeat()))
	{
		if (RepeatStage(repeat, s.lastSeedType, recognise, lf))
		{
			Success(true);
			Help(0xE);
			s.gesture.gesture = k_None;
			PowerUp().icons->RepeatLast(s.lastSeedType);
			return;
		}
	}
	// (e) the square spiral puts the leash on, and opens the leash picker when the creature knows more than one leash
	if (leash.creature && !fighting && !picker.Active(leash, player) && !SelectionOpenAndHandReady() &&
	    LeashStartWanted(leash, player) && leashTables.start != k_None)
	{
		if (leashTables.start < k_GestureCount)
		{
			lf[leashTables.start].type = 2;
			lf[leashTables.start].leash = static_cast<int>(leash.picked);
		}
		if (Recognise(leashTables.start))
		{
			Success(true);
			s.gesture.gesture = k_None;
			if (picker.Start(leash, s.selection))
			{
				// the leash goes on at the next game turn, through the click on the creature's packet
				game_packets::Push({.type = game_packets::Type::CreatureLeashClick, .player = player});
			}
			if (picker.open)
			{
				PickerLog(picker.pending ? "open, the leash goes on at the next turn" : "open");
			}
		}
	}
	// the gesture icons the table asks for, made again only when it changed
	s.lookingFor = lf;
	const auto screen = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
	IconState().list.Update(lf, screen, particles::surf_revol::PlayerColour(static_cast<int>(player)),
	                        [](Gesture g) { return ShapeOf(g).length; });
}
