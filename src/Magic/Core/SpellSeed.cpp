/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSeed.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"

#include "Magic/CastRules.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/Hand/HandMagicFX.h"
#include "ECS/Archetypes/SpellSeedArchetype.h"
#include "ECS/Components/Transform.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "PSys/PSysManager.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Players.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/WorshipSite.h"
#include "Worship/WorshipSpellIcon.h"
#include "Spell.h"
#include "SpellCreator.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
SpellSeed& SeedOf(entt::entity seed)
{
	return Locator::entitiesRegistry::value().Get<SpellSeed>(seed);
}

bool ValidSpell(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	return spell != entt::null && registry.Valid(spell) && registry.AllOf<Spell>(spell);
}

/// SpellSeed::IsSpellCastInHand 0x729820
bool IsSpellCastInHand(const SpellSeed& seed)
{
	return seed::InfoOf(seed).castType == SpellCastType::SpellCastInHand;
}

/// SpellSeed::DoPreCastThings 0x729460
void DoPreCastThings(entt::entity entity, MagicType type, psys::ProcessInfo& info, SpellCastData& castData,
                     const psys::ProcessInfo& handInfo)
{
	auto& seed = SeedOf(entity);
	seed.flags &= static_cast<uint8_t>(~1u);
	seed::ClearSpellLink(entity);
	if (seed.icon != entt::null)
	{
		seed::ClearSpellLink(entity);
	}
	// iface->UpdateSpellInfo(NULL, &info), then +0x0C = iface +0x5C, +0x24 = iface +0x44, +0x34 = iface +0x54
	info.interfacePos = handInfo.interfacePos;
	info.cameraForward = handInfo.cameraForward;
	info.handPos = handInfo.handPos;
	info.direction = handInfo.direction;
	info.curl = handInfo.curl;
	const auto& tables = Locator::infoConstants::value();
	castData.duration = GetTimerWhenPlayerCasting(tables, type) * seed.castMultiplier;
	castData.chants = GetMagicEffectInfo(tables, type).initialChants * seed.castMultiplier;
	// 0x729502..0x72950B: `if (magicInfo.spellSeedType == SpellSeedType::Fire) castData.magnitude = 1.0f`, after
	// fn_0071FA10 put the gesture packet's size (+0x14 = GInterface +0x1B8) there, so a FIRE seed casts with a fixed
	// magnitude whatever circle was drawn. Read literally the branch is dead: info.dat leaves GMagicInfo +0x28
	// (spellSeedType) at -1 in every row and nothing writes it at run time, so the original's ball would take the last
	// circle's size, or 0 (SF_FireBall's MagnitudeFloatProvider clamps it to 0.01, a 4 cm ball) when no circle was ever
	// drawn. USER (2026-10-01): a fireball thrown from the hand was always big, whatever the gesture. (inferido) the
	// test uses the seed's own type (GSpellSeedInfo, seed +0x6C) when the info row leaves the field at None, which
	// gives the hand fireball the 1.0 the branch was written for (SetScale 1.0 x the root sprite's 4.0168).
	const auto& magicInfo = GetMagicInfo(tables, type);
	const auto infoSeed =
	    magicInfo.spellSeedType != SpellSeedType::None ? magicInfo.spellSeedType : seed.seedType; // (inferido)
	if (infoSeed == SpellSeedType::Fire)
	{
		castData.magnitude = 1.0f;
	}
}

/// SpellSeed::DoPostCastThings 0x729260
void DoPostCastThings(entt::entity entity, entt::entity spellEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& seed = SeedOf(entity);
	auto& spell = registry.Get<Spell>(spellEntity);
	// GetTribalPowerTribe != -1: the tribal power column and sound 0x1B + tribe (never in vanilla: tribal power 1)
	// linked to an icon -> WorshipSpellIcon::CancelCharge 0x77F9A0 (the seed's interface status)
	if (seed.icon != entt::null && registry.Valid(seed.icon) && registry.AllOf<WorshipSpellIcon>(seed.icon))
	{
		worship::icon::CancelCharge(seed.icon, seed.creator.player);
	}
	// Spell::SetInterfaceStatus 0x7201F0 stores the GInterfaceStatus*; "my interface" is iface == MyInterfaceStatus
	// (inferido: one local interface, both taken from the seed's inInterface flag)
	spell.castFromInterface = seed.inInterface;
	spell.isMyInterfaceCasting = seed.inInterface;
	seed.lastMagic = spell.magicType;
	if (seed.storedChants >= 0.0f)
	{
		chants::SetChants(spell, seed.storedChants);
		spell.age = seed.storedAge;
	}
	seed::SetChantStore(seed, 0.0f);
	// TODO(M2): the help triggers 9 / 10
	if (!IsSpellCastInHand(seed))
	{
		if (auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
		{
			transform->position = ToWorld(spell.castPos);
		}
	}
	if (spell.seed == entt::null)
	{
		spell.seed = entity;
	}
	seed.spell = spellEntity; // fn_007281A0
	seed.hasCast = true;
	// TODO(M2): GInterface::StartImmersion(magicInfo.immersion, 0x80000000)
}
} // namespace

const GSpellSeedInfo& seed::InfoOf(const SpellSeed& seed)
{
	return GetSpellSeedInfo(Locator::infoConstants::value(), seed.seedType);
}

entt::entity seed::Create(const glm::vec3& worldPosition, SpellSeedType seedType, PlayerNames player, int powerUp,
                          float multiplier)
{
	const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), seedType);
	const auto entity = ecs::archetypes::SpellSeedArchetype::Create(worldPosition, seedType, info.scale);
	auto& registry = Locator::entitiesRegistry::value();
	auto& seed = registry.Assign<SpellSeed>(entity);
	seed.seedType = seedType;
	seed.powerUp = powerUp;
	seed.castMultiplier = multiplier;
	seed.inInterface = true;
	seed.creator = creator::OfPlayer(player); // iface->GetPlayer()
	// fn_00728140 (the common init) zeroes +0x7C / +0x80 / +0x84 / +0x74 / +0x78; ctor 0x7280A0 then stores 0 in +0x7C,
	// +0x84, +0x74, +0x78 and -1 in +0x80 (+0x7C = -1 "none" is only written by StoreChantsAndAgeFromSpell 0x728780)
	seed.storedAge = 0.0f;
	seed.storedChants = 0.0f;
	seed.chantStoreCopy = 0.0f;
	seed.chantStore = 0.0f;
	seed.storedMaxObjects = -1;
	return entity;
}

MagicType seed::MagicTypeOf(const SpellSeed& seed)
{
	return GetMagicInfoFromPULevel(Locator::infoConstants::value(), InfoOf(seed), seed.powerUp).magicType;
}

float seed::GetChantNeeded(const SpellSeed& seed, int powerUp)
{
	const auto& tables = Locator::infoConstants::value();
	const auto type = GetMagicInfoFromPULevel(tables, InfoOf(seed), powerUp).magicType;
	return GetChantsRequiredToCreate(tables, type) - seed.chantStore;
}

float seed::GetPower(const SpellSeed& seed)
{
	const float cost = GetChantsRequiredToCreate(Locator::infoConstants::value(), MagicTypeOf(seed));
	// 0x7298B0 divides without a test: store / 0 is inf or NaN on the x87, and the min against 1.0 [0x8AA390] then
	// gives 1
	const float power = cost > 0.0f ? seed.chantStore / cost : 1.0f;
	return power < 1.0f ? power : 1.0f;
}

void seed::SetChantStore(SpellSeed& seed, float chants)
{
	seed.chantStore = chants;
	seed.chantStoreCopy = chants;
}

void seed::AddToChantStore(SpellSeed& seed, float chants)
{
	SetChantStore(seed, seed.chantStore + chants);
}

void seed::SetPowerUp(entt::entity entity, int powerUp)
{
	auto& seed = SeedOf(entity);
	const bool lower = powerUp < seed.powerUp;
	seed.powerUp = powerUp;
	const float excess = -GetChantNeeded(seed, powerUp);
	// excess > 0 with a worship site (vt 0x30C: the icon's) -> site +0xF0 += excess, the store keeps the cost
	auto& registry = Locator::entitiesRegistry::value();
	if (excess > 0.0f && seed.icon != entt::null && registry.Valid(seed.icon) && registry.AllOf<WorshipSpellIcon>(seed.icon))
	{
		const auto site = registry.Get<const WorshipSpellIcon>(seed.icon).site;
		if (site != entt::null && registry.Valid(site) && registry.AllOf<WorshipSite>(site))
		{
			registry.Get<WorshipSite>(site).battery += excess;
			SetChantStore(seed, seed.chantStore - excess);
		}
	}
	if (!seed.inInterface)
	{
		return;
	}
	// The local interface: ToolTips::ForceToolTips(the effect's toolTipsEnum) (no tooltips yet), the render hand's
	// in-hand effect (fn_0046E7B0), PHandFX SetPULevel(pu + 1, delayed) and, unless the level went down,
	// AddSpellToHandVisuals(0); LoadFileData(particleType); the level's voice (fn_00729C40)
	hand_fx::CreateInHandEffect(entity);
	hand_fx::SetPULevel(powerUp + 1, true);
	if (!lower)
	{
		hand_fx::AddSpellToHandVisuals(false);
	}
	// fn_00729C40: PU 0 / 1 / 2 -> SpellDialogue samples 10 / 11 / 12 (the jump table 0x729C80); none for -1 (0x729C48);
	// GAudio::PlaySoundEffect 0x429D60(NULL, sample, mode 2, loops 0, +0x10 0, is3D 0, AUDIO_SFX_BANK_TYPE 9) 0x729C77
	if (powerUp >= 0 && powerUp <= 2)
	{
		audio::PlaySoundEffect(audio::Owner::None(), 10 + powerUp, 2, 0, false, false, audio::SfxBank::SpellDialogue);
	}
}

void seed::SetInactive(SpellSeed& seed, bool inactive)
{
	if (inactive)
	{
		seed.turnsInHand = 0;
		seed.ready = false;
	}
	else
	{
		seed.ready = true;
	}
}

int seed::InterfaceSetInMagicHand(entt::entity entity)
{
	SetPowerUp(entity, SeedOf(entity).powerUp);
	// TODO(M2): HelpProfile::Trigger(13 with a spell, else 12)
	if (!StoreChantsAndAgeFromSpell(entity) || (SeedOf(entity).flags & 2u) != 0)
	{
		ToBeDeleted(entity);
		return 3;
	}
	auto& seed = SeedOf(entity);
	// fn_005DCA20: the interface's last seed type (R repeats it)
	if (seed.inInterface)
	{
		gestures::State().lastSeedType = static_cast<int>(seed.seedType);
	}
	seed.flags &= static_cast<uint8_t>(~1u);
	seed.lastMagic = MagicType::None;
	seed.hasCast = false;
	seed.turnsInHand = 0;
	seed.ready = true;
	ClearSpellLink(entity);
	if (seed.icon != entt::null)
	{
		seed.flags &= static_cast<uint8_t>(~1u);
		ClearSpellLink(entity);
	}
	// the local hand: LoadFileData(particleType) (the effect files load on use here) and, for a tribe with tribal power
	// above 1, PHandFX::StartTribalPowerRing (never in the vanilla game)
	if (seed.inInterface)
	{
		const auto tribe = GetTribalPowerTribe(GetMagicEffectInfo(Locator::infoConstants::value(), MagicTypeOf(seed)),
		                                       &players::MagicOf(seed.creator.player).tribalPower);
		if (tribe != -1)
		{
			hand_fx::StartTribalPowerRing(tribe);
		}
	}
	return 1;
}

void seed::ProcessInHand(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	// the first turn, for the local interface (IsInterfacePowerUpWhenInHand = 1): SetupPowerUpGestures fn_005CEE30
	if (seed.turnsInHand == 0 && seed.inInterface)
	{
		gestures::SetupPowerUpGestures();
	}
	++seed.turnsInHand;
	if (!seed.ready)
	{
		const float held = static_cast<float>(seed.turnsInHand) * static_cast<float>(k_TurnMs) * 0.001f;
		if (held > Locator::infoConstants::value().spellSystem.delayBeforeSeedActive)
		{
			seed.ready = true;
			// fn_005D1260 (GInterface::EndAction) for the local interface
			if (seed.inInterface && Locator::handSystem::has_value())
			{
				Locator::handSystem::value().EndAction();
			}
			if (TraceEnabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: seed {} ready after {} turns",
				                   static_cast<uint32_t>(entity), seed.turnsInHand);
			}
		}
	}
	if (ValidSpell(seed.spell) && Locator::entitiesRegistry::value().Get<Spell>(seed.spell).closedDown)
	{
		ToBeDeleted(entity);
	}
	// Object::ProcessInHand 0x639AD0: TODO(M2)
}

bool seed::StoreChantsAndAgeFromSpell(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	if (!ValidSpell(seed.spell))
	{
		seed.storedChants = -1.0f;
		seed.storedAge = 0.0f;
		seed.storedMaxObjects = -1;
		return true;
	}
	const auto spellEntity = seed.spell;
	auto& spell = Locator::entitiesRegistry::value().Get<Spell>(spellEntity);
	seed.storedChants = spell.chants;
	SetChantStore(seed, spell.chants);
	seed.storedAge = spell.age;
	const auto maxObjects = OpsOf(spell.spellClass).maxObjectsToCreate; // vt 0x550
	seed.storedMaxObjects = maxObjects != nullptr ? maxObjects(spellEntity) : spell.maxObjectsToCreate;
	ClearSpellLink(entity);
	SeedOf(entity).hasCast = false;
	return OpsOf(spell.spellClass).hasEnoughChantsForRecast(spellEntity); // vt 0x518
}

void seed::ClearSpellLink(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	const auto spellEntity = seed.spell;
	if (!ValidSpell(spellEntity))
	{
		seed.spell = entt::null;
		return;
	}
	// TODO(M2): magicInfo.stopImmersion && the local interface -> StopImmersion(immersion)
	seed.spell = entt::null;
	auto& spell = Locator::entitiesRegistry::value().Get<Spell>(spellEntity);
	if (spell.seed == entity)
	{
		spell.seed = entt::null; // fn_007213B0 (its ClearSpellLink call finds no spell now)
		CloseDown(spellEntity);  // vt 0x530
	}
	else if (spell.psys != 0)
	{
		psys::manager::CloseDown(spell.psys); // fn_00720190: the spell's PSys only
	}
}

void seed::ApplyUnlockProcess(entt::entity entity)
{
	if (InfoOf(SeedOf(entity)).deleteSeedOnceCast != 0)
	{
		ToBeDeleted(entity);
		return;
	}
	if (!StoreChantsAndAgeFromSpell(entity))
	{
		ToBeDeleted(entity);
	}
}

int seed::ProcessFromSpell(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	const auto& info = InfoOf(seed);
	// fn_00728FC0: not in the map (IsObjectInMap: openblack's seeds never are), not cast in hand, not kept in hand,
	// seedFollowsSpell, the spell (if any: a NULL +0x60 goes on, 0x728FF6) still open, and linked to an icon
	const bool spellOpen = !ValidSpell(seed.spell) || !Locator::entitiesRegistry::value().Get<Spell>(seed.spell).closedDown;
	const bool follows = !IsSpellCastInHand(seed) && info.isKeptInHand == 0 && info.seedFollowsSpell != 0 && spellOpen &&
	                     seed.icon != entt::null;
	if (!follows)
	{
		return 1;
	}
	const auto& transform = Locator::entitiesRegistry::value().Get<Transform>(entity);
	if (influence::CalculatePlayerInfluence(seed.creator.player, transform.position) <= 0.0f && ValidSpell(seed.spell) &&
	    !Locator::entitiesRegistry::value().Get<Spell>(seed.spell).closedDown)
	{
		CloseDown(seed.spell);
	}
	return 1;
}

bool seed::CanCast(entt::entity entity, const glm::vec3& position)
{
	const auto& seed = SeedOf(entity);
	const auto type = MagicTypeOf(seed);
	// fn_00729120: the cast rule for the seed's player; then vt 0x30 with the seed's creator
	if (!cast_rules::CanCastRule(GetMagicInfo(Locator::infoConstants::value(), type), position, seed.creator.player))
	{
		return false;
	}
	return cast_rules::CanCastAt(type, position);
}

int seed::Cast(entt::entity entity, const glm::vec3& position, entt::entity* out, float magnitude,
               const psys::ProcessInfo& handInfo)
{
	auto& seed = SeedOf(entity);
	const MagicType type = seed.lastMagic != MagicType::None ? seed.lastMagic : MagicTypeOf(seed);
	if (!ValidSpell(seed.spell))
	{
		if (seed.hasCast)
		{
			*out = entt::null;
			return 0;
		}
	}
	else if (IsSpellCastInHand(seed))
	{
		*out = seed.spell;
		return 1;
	}
	SpellCastData castData {magnitude, 0.0f, 0.0f, seed.storedMaxObjects};
	psys::ProcessInfo info;
	info.power = 1.0f;
	info.enabled = true;
	DoPreCastThings(entity, type, info, castData, handInfo);
	entt::entity spell = entt::null;
	const int result = CastAtPos(type, SeedOf(entity).creator, position, &spell, &castData, info);
	if (result != 0 && spell != entt::null)
	{
		DoPostCastThings(entity, spell);
	}
	*out = spell;
	return result;
}

void seed::ToBeDeleted(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<SpellSeed>(entity))
	{
		return;
	}
	// ClearSpellIconLink 0x7281D0: RemoveFromSpellIcon 0x7281C0 (fn_0077F7D0), then no icon
	if (const auto icon = SeedOf(entity).icon; icon != entt::null)
	{
		worship::icon::RemoveSeed(icon, entity);
	}
	SeedOf(entity).icon = entt::null;
	ClearSpellLink(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}
