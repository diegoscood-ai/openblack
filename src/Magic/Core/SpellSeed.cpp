/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSeed.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Audio/Audio.h"

#include "Magic/CastRules.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/Hand/HandMagicFX.h"
#include "ECS/Archetypes/SpellSeedArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
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
#include "Magic/Spells/SpellForest.h"
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

/// Object::Get2DRadius 0x638180 of the seed (vt +0x64, 0x6022D9): GetScale x max(mesh +0x24, +0x2C), from
/// SpellSeed::GetMesh 0x729850 (the info's mesh, which the Game3DObject +0x40 keeps even while the seed is not drawn:
/// so the info's mesh, not the Mesh component, which goes while the seed is hidden)
float SeedRadius(entt::entity entity, const SpellSeed& seed)
{
	if (!Locator::entitiesRegistry::value().AllOf<Transform>(entity))
	{
		return 0.0f;
	}
	return ecs::object::MeshRadius2D(resources::HashIdentifier(seed::InfoOf(seed).mesh), ecs::object::GetScale(entity));
}

/// fn_006022C0 (MapCoords this = the seed's, obj = the seed, 1): ecs::map_cells::TallestOverlapping, the highest
/// GetTopPos of the objects of the seed's cell (fixed list, then mobile) that are not the seed, not living and not
/// moving, overlapping the seed's circle (its own Get2DRadius, the seed's override). 0 if none or out of bounds
float TopOfObjectsUnder(entt::entity entity, const SpellSeed& seed, const glm::vec3& position)
{
	if (!cast_rules::InBounds(position))
	{
		return 0.0f; // MapCoords::InBounds 0x6042C0
	}
	const auto coords = ecs::map_coords::FromMetres(glm::vec2(position.x, position.z));
	return ecs::map_cells::TallestOverlapping(coords, entity, coords, SeedRadius(entity, seed), true);
}

/// The seed's Game3DObject drawn or not this frame (openblack: its Mesh, which the renderer and the hand's pick see)
void ShowMesh(entt::entity entity, const SpellSeed& seed, bool show)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool has = registry.AllOf<Mesh>(entity);
	if (show && !has)
	{
		registry.Assign<Mesh>(entity, resources::HashIdentifier(seed::InfoOf(seed).mesh), static_cast<int8_t>(0),
		                      static_cast<int8_t>(0));
		registry.SetDirty();
	}
	else if (!show && has)
	{
		registry.Remove<Mesh>(entity);
		registry.SetDirty();
	}
}

/// SpellSeed's draw from its spell 0x729020 (DrawSpellSeed 0x721360 with +0xAC set)
void DrawFromSpell(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& seed = SeedOf(entity);
	// fn_00728FC0, then the seed's own spell (+0x60) must be set (0x729033)
	if (!seed::FollowsSpell(seed) || !ValidSpell(seed.spell))
	{
		// not drawn: out of the hand nothing else draws it (the hand draws the one it holds, HandSpellSeed.cpp)
		const auto held = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHeldObject() : std::nullopt;
		if (!held.has_value() || *held != entity)
		{
			ShowMesh(entity, seed, false);
		}
		return;
	}
	auto* transform = registry.TryGet<Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	const auto& spell = registry.Get<const Spell>(seed.spell);
	// a copy of the seed's MapCoords (+0x14); its altitude = fn_006022C0(seed, 1); vt 0x540 of the seed's spell on it
	// (call at 0x72906E). Of the spell vtables only SpellForest's has its own (0x725750, the only pointer to it, slot
	// 0x540 of 0x8F4FE4); all the others keep Spell::AdjustSpellSeedPos 0x55CE60, a bare `ret 4`.
	float altitude = TopOfObjectsUnder(entity, seed, transform->position);
	if (spell.spellClass == SpellClass::Forest)
	{
		altitude = spell_forest::AdjustSpellSeedPos(seed.spell, altitude);
	}
	// LHPoint (x, GetAltitudeAndSetColorSpecular 0x803340 + altitude, z) -> LHMatrix::Translation 0x403530 on the
	// Game3DObject's matrix (+0x14). Translation rewrites the whole matrix: identity rows (0x403532..0x403558), then
	// the point, so the seed is drawn upright, unturned and unscaled whatever the hand left in it (the spin of the
	// worship icon / hand is not kept). +0x44 = 1.0 and +0x48 = 0 are LH3DObject's scale and y_angle (bw1-decomp
	// LH3DObject.h; Game3DObject::SetPosition 0x63B740 writes them the same way). openblack's Transform is both the
	// matrix and the Object scale (+0x50): the four seeds that reach here (STORM, NATURE, SHIELD, PHYSICAL_SHIELD:
	// seedFollowsSpell, neither cast nor kept in the hand) all have the info scale 1, so scale 1 is the same.
	// AddForDrawing(seed) 0x63B5D0, which sends the object draw collision
	const glm::vec3 at = ToWorld(glm::vec3(transform->position.x, altitude, transform->position.z));
	if (TraceEnabled() && (!registry.AllOf<Mesh>(entity) || std::abs(transform->position.y - at.y) > 0.25f))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: seed {} drawn over spell {} at ({:.1f}, {:.2f}, {:.1f}), altitude {:.2f}",
		                   static_cast<uint32_t>(entity), static_cast<uint32_t>(seed.spell), at.x, at.y, at.z, altitude);
	}
	if (transform->position != at || transform->rotation != glm::mat3(1.0f) || transform->scale != glm::vec3(1.0f))
	{
		transform->position = at;
		transform->rotation = glm::mat3(1.0f); // LHMatrix::Translation 0x403530 (+0x48 y_angle 0)
		transform->scale = glm::vec3(1.0f);    // +0x44 = 1.0
		registry.SetDirty();
	}
	ShowMesh(entity, seed, true);
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

bool seed::FollowsSpell(const SpellSeed& seed)
{
	const auto& info = InfoOf(seed);
	// fn_00728FC0: not in the map (IsObjectInMap vt 0x178: SpellSeed::InsertMapObject 0x728F30 is empty, so never), not
	// cast in hand (0x729820), not kept in hand (0x729840), seedFollowsSpell (info +0x120), the spell (if any: a NULL
	// +0x60 goes on, 0x728FF6) still open (+0x40), and linked to an icon (+0x5C). The icon link survives the cast: only
	// ClearSpellIconLink 0x7281D0, from ToBeDeleted 0x728280, clears it.
	const bool spellOpen = !ValidSpell(seed.spell) || !Locator::entitiesRegistry::value().Get<Spell>(seed.spell).closedDown;
	return !IsSpellCastInHand(seed) && info.isKeptInHand == 0 && info.seedFollowsSpell != 0 && spellOpen &&
	       seed.icon != entt::null;
}

bool seed::ValidForPlaceInHand(entt::entity entity, PlayerNames handPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<SpellSeed>(entity))
	{
		return false;
	}
	const auto& seed = SeedOf(entity);
	// GetPlayer 0x729800: the interface status's (+0x64) player, NULL without one (openblack: inInterface, the creator)
	const bool hasPlayer = seed.inInterface;
	// g_game +0x14 & 0x2000 (test ch, 0x20) and GPlayer::IsNeutral 0x64AC00
	if (influence::IsInfluenceEverywhere() && hasPlayer && seed.creator.player == PlayerNames::NEUTRAL)
	{
		return true;
	}
	// the status's GetPlayer (vt 0x1C) == the seed's, and IsAvailable (GameThing 0x401810: a seed being deleted is gone
	// from openblack's registry)
	return hasPlayer && seed.creator.player == handPlayer;
}

void seed::DrawSpells()
{
	// Spell::DrawSpells 0x7203F0 first calls fn_0064AF20 (each player and the neutral one: player +0xA48's six slots
	// +0x34, fn_0077B3B0 each; not identified, not ported), fn_00725FE0 (a bare ret), fn_0072BF50 (the physical
	// shields' DrawShield: map_shield::DrawShields, MagicLoop.cpp) and fn_00682950 (the PSysFireball list g_game
	// +0x205C9C: fn_00682F30 sends each catchable ball's invisible draw collision; not ported here). Then Spell::Draw
	// 0x720430 (vt 0x50C) of each spell of g_game +0x205BC4: vt 0x108(1) of the spell's +0xB0 object (not identified)
	// and DrawSpellSeed vt 0x508 (0x721360 in every spell class: with a seed +0xAC, jmp 0x729020).
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> seeds;
	registry.Each<const Spell>([&](entt::entity, const Spell& spell) {
		if (spell.seed != entt::null && registry.Valid(spell.seed) && registry.AllOf<SpellSeed>(spell.seed))
		{
			seeds.push_back(spell.seed);
		}
	});
	for (const auto entity : seeds)
	{
		if (registry.Valid(entity) && registry.AllOf<SpellSeed>(entity))
		{
			DrawFromSpell(entity);
		}
	}
}

int seed::ProcessFromSpell(entt::entity entity)
{
	auto& seed = SeedOf(entity);
	if (!FollowsSpell(seed))
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
