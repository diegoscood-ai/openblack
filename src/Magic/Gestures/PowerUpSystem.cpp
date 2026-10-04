/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PowerUpSystem.h"

#include <cstdlib>

#include <spdlog/spdlog.h>

#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "GestureInput.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/SpellSeed.h"
#include "Help/HelpProfile.h"
#include "Magic/MagicTables.h"
#include "PSys/Utility.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::magic::gestures;

namespace
{
/// [0xBF1AF8]: the pause after any recognition or cancel
constexpr float k_Cooldown = 0.4f;
/// the circle is forgotten 5 s after it was drawn ([0x8AB6E4])
constexpr float k_CircleLife = 5.0f;

InterfaceGestures g_State;
IconProvider* g_Icons = nullptr;
HandStatus g_Hand;

bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_GESTURE_TRACE") != nullptr;
	return trace;
}

const ecs::components::SpellSeed* HeldSeed()
{
	if (g_Hand.heldSeed == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(g_Hand.heldSeed))
	{
		return nullptr;
	}
	return registry.TryGet<ecs::components::SpellSeed>(g_Hand.heldSeed);
}

/// HelpProfile::Trigger 0x5C46E0 (Help/HelpProfile.h) of the gestures' call sites: 14 ProcessPowerUpSystem 0x5CF966,
/// 15 0x5CF8BA, 16 / 17 / 21 fn_005CFAE0 0x5CFC75 / 0x5CFD58 / 0x5CFB64, 18 / 20 / 22 fn_005D0000 0x5D01AA / 0x5D00A1 /
/// 0x5D010C. (pending, creature) 19 GestureCreatureSpecial (fn_005CFDE0 0x5CFF0B, the creature's fight) is not ported
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
	if (g_Hand.removeFromHandFx)
	{
		g_Hand.removeFromHandFx();
	}
}

void ForceDropHeld()
{
	if (g_Hand.forceDropHeld)
	{
		g_Hand.forceDropHeld();
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

/// fn_005CF1C0: the selection stays open only while the hand is ready for an object
bool SelectionOpenAndHandReady()
{
	const bool open = g_State.selection.open && g_Hand.handReady;
	g_State.selection.open = open;
	return open;
}

/// fn_005D0000: the scribble (power down, or shake the seed out) and the seed's power-up gestures
bool PowerUpGestures(LookingForArray& lf)
{
	const auto* seed = HeldSeed();
	if (seed == nullptr)
	{
		return false;
	}
	const int seedIndex = static_cast<int>(seed->seedType);
	auto& s = g_State;
	if (s.currentPowerUpGesture != k_None)
	{
		lf[k_Scribble] = LookingFor {0xD, seedIndex, lf[k_Scribble].leash, -1};
		if (Recognise(k_Scribble))
		{
			RemoveFromHandFx();
			Success(false);
			s.gesture.gesture = k_None;
			SetupPowerUpGesturesInner();
			Help(0x14);
			if (g_Icons != nullptr)
			{
				g_Icons->SetPowerUpCharge(seed->icon, -1); // packet 0x6A
			}
			return true;
		}
	}
	else
	{
		lf[k_Scribble].type = 0;
		if (Recognise(k_Scribble))
		{
			RemoveFromHandFx();
			Success(false);
			s.gesture.gesture = k_None;
			Help(0x16);
			ForceDropHeld();
			return true;
		}
	}
	for (int k = 0; k < 3; ++k)
	{
		const Gesture g = s.powerUpGestures[static_cast<size_t>(k)];
		if (g == k_None || g == s.currentPowerUpGesture)
		{
			continue;
		}
		lf[g] = LookingFor {0xE + k, seedIndex, lf[g].leash, k};
		if (Recognise(g) && seed->icon != entt::null)
		{
			Success(true);
			SetupPowerUpGesturesInner();
			Help(0x12);
			s.currentPowerUpGesture = g;
			if (g_Icons != nullptr)
			{
				g_Icons->SetPowerUpCharge(seed->icon, k); // packet 0x6A
			}
			s.gesture.gesture = g;
			return true;
		}
	}
	return false;
}
} // namespace

void gestures::SetIconProvider(IconProvider* provider)
{
	g_Icons = provider;
}

IconProvider* gestures::GetIconProvider()
{
	return g_Icons;
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

Selection::Outcome Selection::Stage(float dt, float timeOut, const std::function<bool(Gesture)>& recognise,
                                    IconProvider& icons, LookingForArray* lookingFor)
{
	// dt is 0 while paused (g_game +0x59A4)
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
				first = true; // Success(1), help 0x10, m_Gesture.gesture = 0 and the timer back to 0 (the caller)
				timer = 0.0f;
			}
			candidate[s] = true;
			if (stage + 1 == 3 || seedStage[s][stage + 1] == k_None)
			{
				open = false;
				icons.RequestSpell(static_cast<int>(s)); // help 0x11, packet 0x25 (seed, 0)
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

InterfaceGestures& gestures::State()
{
	return g_State;
}

void gestures::Reset()
{
	g_State = InterfaceGestures {};
}

void gestures::SetHandStatus(HandStatus status)
{
	g_Hand = std::move(status);
}

const HandStatus& gestures::GetHandStatus()
{
	return g_Hand;
}

void gestures::ClearBuffer()
{
	g_State.system.Clear();
}

void gestures::FeedSample(glm::ivec2 mouse)
{
	// g_game +0x14 & 4 (casting.md: a restricted input mode; inferido: taken as the pause, g_game +0x59A4), and the
	// cooldown +0x378
	if (g_Hand.paused || g_State.cooldown > 0.0f)
	{
		return;
	}
	const auto world = sampling::ScreenToLand(glm::vec2(mouse));
	// (0, 0, 0) means off the land: |c| <= [0x8C79D8] (double 1e-4), as GestureBuffer's k_Zero
	if (world && !(std::abs(world->x) <= 1e-4f && std::abs(world->y) <= 1e-4f && std::abs(world->z) <= 1e-4f))
	{
		g_State.system.AddSample(*world, mouse);
	}
	else
	{
		g_State.system.AddSampleAtLastWorld(mouse);
	}
}

void gestures::ReseedBuffer()
{
	ClearBuffer();
	FeedSample(g_Hand.mouse);
}

bool gestures::Recognise(Gesture gesture)
{
	const float ratio = sampling::ScreenRatio();
	const auto data = BuildFromSystem(g_State.system, ratio);
	return MatchGesture(Templates(), gesture, data, g_State.result, ratio);
}

void gestures::Success(bool success)
{
	if (success)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: recognised {} (reversed {}, keypoints {}..{}, template {})",
			                   static_cast<int>(g_State.result.gesture), g_State.result.reversed, g_State.result.start,
			                   g_State.result.end, g_State.result.templateIndex);
		}
		// fn_00689790: the sparkles along the stroke (PARTICLE_TYPE_GESTURE 35); StartImmersion(3, 0x80000000): force
		// feedback, not ported
		psys::utility::GestureRecognised(g_State.system, g_State.result);
	}
	ClearBuffer();
	g_State.cooldown = k_Cooldown;
}

bool gestures::SetupPowerUpGesturesInner()
{
	auto& s = g_State;
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
		    g_Icons != nullptr && g_Icons->PowerUpAvailable(seed->icon, static_cast<int>(k)))
		{
			s.powerUpGestures[k] = static_cast<Gesture>(info.powerUpGestures[k]);
			any = true;
		}
	}
	return any;
}

void gestures::SetupPowerUpGestures()
{
	SetupPowerUpGesturesInner();
	g_State.heldFlag8 = true;
	ReseedBuffer();
	if (const auto* seed = HeldSeed(); seed != nullptr)
	{
		const auto& magic = GetMagicInfoFromPULevel(Locator::infoConstants::value(), seed::InfoOf(*seed), seed->powerUp);
		g_State.currentPowerUpGesture = static_cast<Gesture>(magic.gestureType);
	}
}

bool gestures::HoldingChargingSeed()
{
	const auto* seed = HeldSeed();
	return seed != nullptr && seed->ready && seed->lastMagic == MagicType::None && seed->icon != entt::null;
}

Gesture gestures::PowerUpLevelGesture()
{
	return HoldingChargingSeed() ? g_State.currentPowerUpGesture : k_None;
}

void gestures::ProcessPowerUpSystem(float dt)
{
	auto& s = g_State;
	// The early outs that wipe the buffer: the temple (g_game +0x205A28 == 1), status +0x24 & 0x20, !IsActive(),
	// status +0x3C and byte [0xC4CCEE]. openblack has none of those states.
	// A fire seed in the hand with a MagicFireBall under it feeds the fireball (SendApplyToObject): TODO(M5)
	// GCamera::IsMoving wipes the gesture in progress, unless LH3DCameraChecker holds it or the camera mode lets the
	// player gesture while it moves (CameraModeNew3 in an arena fight, Dance, Follow: none in openblack)
	if (sampling::CameraMoving())
	{
		ClearBuffer();
	}
	if (!g_Hand.paused && s.cooldown > 0.0f)
	{
		s.cooldown -= dt;
	}
	auto lf = FreshLookingFor();
	if (s.circlePending)
	{
		if (!g_Hand.paused)
		{
			s.circleTimer += dt;
		}
		if (s.circleTimer > k_CircleLife)
		{
			s.circlePending = false;
		}
	}
	// (a) the circle that sizes a storm or a shield, while the action press lasts (m_Buttons & 0x200)
	if (g_Hand.actionLatched)
	{
		if (const auto* seed = HeldSeed(); seed != nullptr && !s.circlePending)
		{
			if (const auto g = static_cast<Gesture>(seed::InfoOf(*seed).sizingGesture); g != k_None) // fn_00729AC0
			{
				lf[g].type = 0xC;
				if (Recognise(g))
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
	// The local creature fighting (action state 16, fn_005CFDE0) and the leash selection (fn_005CEFB0, 0x5D01F0) need a
	// creature: none yet
	const bool fighting = false;
	auto recognise = [](Gesture g) { return Recognise(g); };
	if (HoldingChargingSeed())
	{
		s.selection.open = false;
		if (PowerUpGestures(lf))
		{
			return;
		}
	}
	else if (SelectionOpenAndHandReady())
	{
		if (g_Icons != nullptr)
		{
			const auto outcome = s.selection.Stage(dt, Locator::infoConstants::value().spellSystem.selectionSystemTimeOut,
			                                       recognise, *g_Icons, &lf);
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
	// (b) SCRIBBLE cancels: the object in the hand is shaken out, or an icon's charge is cancelled
	if (!HoldingChargingSeed())
	{
		if (g_Hand.holdingSomething)
		{
			if (g_Hand.inInfluence && g_Hand.validToShake)
			{
				lf[k_Scribble].type = 0;
				if (Recognise(k_Scribble))
				{
					RemoveFromHandFx();
					Success(false);
					ForceDropHeld();
					return;
				}
			}
		}
		// TODO(M8): the leash scribble (status->leash +0x14 && !+0x24 -> packet 0x5F, casting.md §2.3 (b))
		else if (g_Icons != nullptr && g_Icons->AnyIconChargingForHand())
		{
			lf[k_Scribble].type = 0;
			if (Recognise(k_Scribble))
			{
				RemoveFromHandFx();
				Success(false);
				g_Icons->CancelMostChargedIcon(); // packet 0x1E
				return;
			}
		}
	}
	const auto& system = Locator::infoConstants::value().spellSystem;
	// (c) the spiral opens the player's miracle selection, the inverse spiral the creature's
	if (!fighting && g_Hand.handReady && g_Icons != nullptr)
	{
		for (const auto category : {static_cast<Gesture>(system.selectionSystemGestureNonCreature),
		                            static_cast<Gesture>(system.selectionSystemGestureCreature)})
		{
			if (!g_Icons->AnyRequestableIconOfCategory(category) || (s.selection.open && s.selection.category == category))
			{
				continue;
			}
			if (category < k_GestureCount)
			{
				lf[category].type = category == static_cast<Gesture>(system.selectionSystemGestureNonCreature) ? 8 : 9;
			}
			if (Recognise(category) && category != k_None)
			{
				const bool opened = s.selection.Open(category, *g_Icons, SelectionTables::FromInfo());
				s.currentPowerUpGesture = k_None; // OpenSelection 0x5CF010: currentPUGesture (+0x178) = 0
				if (opened)
				{
					s.heldFlag8 = true;
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
	if (!fighting && repeat != k_None && !SelectionOpenAndHandReady() && g_Hand.handReady && g_Icons != nullptr &&
	    g_Icons->CanRepeat())
	{
		if (repeat < k_GestureCount)
		{
			lf[repeat].type = 0xA;
			lf[repeat].seed = s.lastSeedType;
		}
		if (Recognise(repeat))
		{
			Success(true);
			Help(0xE);
			s.gesture.gesture = k_None;
			g_Icons->RepeatLast(s.lastSeedType); // packet 0x26
			return;
		}
	}
	// (e) the leash selection (SQUARE_SPIRAL): creature only
	// DisplayGesture::Update(lf) fn_0068ABA0: the HUD icons (S_Gesture0/1.raw), not drawn yet
	s.lookingFor = lf;
}
