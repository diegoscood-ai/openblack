/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipSpellIcon.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Influence/Influence.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "InterfaceStatus.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "SpecialPoints.h"
#include "SpellSeedGraphic.h"
#include "WorshipSite.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// SpellIcon::TChargingData::Init 0x7266E0: mesh 0x231 (561 MSH_S_PULSE_IN)
constexpr MeshId k_PulseMesh = static_cast<MeshId>(561);

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

bool IsIcon(entt::entity icon)
{
	return icon != entt::null && Registry().Valid(icon) && Registry().AllOf<WorshipSpellIcon, SpellIcon>(icon);
}

WorshipSpellIcon& IconOf(entt::entity icon)
{
	return Registry().Get<WorshipSpellIcon>(icon);
}

bool HasSite(const WorshipSpellIcon& icon)
{
	return icon.site != entt::null && Registry().Valid(icon.site) && Registry().AllOf<WorshipSite>(icon.site);
}

const GSpellSeedInfo& SeedInfoOf(entt::entity icon)
{
	return magic::GetSpellSeedInfo(Locator::infoConstants::value(), Registry().Get<const SpellIcon>(icon).seedType);
}

/// fn_007282A0 (pos, icon, status, pu, multiplier) -> fn_00727FF0: a seed charged from this icon; its creator is the icon
/// (+0x68) and its scale the icon's x the seed's (scale)
entt::entity CreateSeedFromIcon(entt::entity iconEntity, const glm::vec3& position, PlayerNames player, int powerUp,
                                float multiplier)
{
	auto& registry = Registry();
	const auto& transform = registry.Get<const Transform>(iconEntity);
	const auto seedType = registry.Get<const SpellIcon>(iconEntity).seedType;
	const auto seedEntity = magic::seed::Create(position, seedType, player, powerUp, multiplier);
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	seed.icon = iconEntity;
	seed.creator = {SpellCreator::Kind::WorshipSpellIcon, icon::PlayerOf(iconEntity), iconEntity};
	if (auto* seedTransform = registry.TryGet<Transform>(seedEntity); seedTransform != nullptr)
	{
		seedTransform->scale = transform.scale * SeedInfoOf(iconEntity).scale; // Object::SetScale
	}
	icon::AddSeed(iconEntity, seedEntity);
	return seedEntity;
}

/// The voice of a fully charged seed (PlayFullyChargedSoundFX 0x77F4E0, the jump table 0x77F5F8 by seed type): bank 9
/// (SpellDialogue), 8 for the unlisted ones
constexpr std::array<int, 30> k_FullyChargedVoice = {
    14, 6, 2, 5, 13, 9, 8, 7, 17, 16, 3, 4, 21, 24, 19, 26, 25, 8, 8, 22, 20, 18, 8, 8, 8, 8, 8, 23, 15, 1,
};

/// fn_00726490: IN_GAME 42 (G_ClickOnSpell_01) with the pitch of the icon's placement (LH_SamplePlayOptions +0x48)
void PlayTapSound(int placement)
{
	if (!Locator::audio::has_value())
	{
		return;
	}
	constexpr std::array<float, 7> k_Pitch = {100.0f, 115.0f, 130.0f, 145.0f, 155.0f, 175.0f, 190.0f};
	const int index = std::clamp(placement, 0, 5);
	auto& audio = Locator::audio::value();
	const auto id = entt::hashed_string("InGame.sad/42").value();
	if (!Locator::resources::value().GetSounds().Contains(id))
	{
		return;
	}
	const auto emitter = audio.CreateEmitter(id, audio::PlayType::Once, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(0.0f),
	                                         audio.GetSound(id).volume, audio::AudioStatus::Playing, true);
	audio.SetEmitterPitch(emitter, k_Pitch.at(static_cast<size_t>(index)));
	audio.PlayEmitter(emitter);
}

/// SpellIcon vt 0x910 GetSpellIconPlacementIndex: WorshipSpellIcon 0x77FC90 = slot - 10; TownCentreSpellIcon 0x748E90 =
/// its index in the town centre's icons (fn_00743FF0)
int PlacementIndexOf(entt::entity icon)
{
	auto& registry = Registry();
	if (const auto* worship = registry.TryGet<const WorshipSpellIcon>(icon); worship != nullptr)
	{
		return worship->slot - 10;
	}
	if (const auto* town = registry.TryGet<const TownCentreSpellIcon>(icon); town != nullptr)
	{
		return town->slot;
	}
	return 0;
}

} // namespace

/// SpellIcon vt 0x914 GetWorshipSpellIcon: a worship icon is its own; a town centre's icon asks its town's site for the
/// icon of its seed (TownSpellIcon::GetWorshipSpellIcon 0x748F30 -> fn_0077C2B0)
entt::entity icon::WorshipIconOf(entt::entity icon)
{
	auto& registry = Registry();
	if (registry.AllOf<WorshipSpellIcon>(icon))
	{
		return icon;
	}
	const auto* town = registry.TryGet<const TownCentreSpellIcon>(icon);
	if (town == nullptr || town->town == entt::null || !registry.Valid(town->town))
	{
		return entt::null;
	}
	const auto* magic = registry.TryGet<const TownMagic>(town->town);
	if (magic == nullptr || magic->worshipSite == entt::null)
	{
		return entt::null;
	}
	return site::GetSpellIconFromSeedType(magic->worshipSite, registry.Get<const SpellIcon>(icon).seedType);
}

entt::entity icon::Create(const glm::vec3& worldPosition, SpellSeedType seed, entt::entity siteEntity, int16_t slot)
{
	auto& registry = Registry();
	const auto& site = registry.Get<const WorshipSite>(siteEntity);
	const auto& siteTransform = registry.Get<const Transform>(siteEntity);
	const auto& iconInfo = Locator::infoConstants::value().spellIcon.at(0); // 0xD9D3E8 "Spell Icon"

	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// ctor 0x77F140: SpellIcon(pos, info, seed, the site's scale, the site's angle)
	registry.Assign<Transform>(entity, worldPosition, glm::mat3(glm::eulerAngleY(-site.yAngle)), siteTransform.scale);
	// SpellIcon::Create3DObject 0x7261A0: Game3DObject::Create(pos, 0, info.GetMesh (203), angle, scale)
	registry.Assign<Mesh>(entity, resources::HashIdentifier(iconInfo.meshId), static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& spellIcon = registry.Assign<SpellIcon>(entity);
	spellIcon.infoIndex = 0;
	spellIcon.seedType = seed;
	spellIcon.player = site.player;
	auto& icon = registry.Assign<WorshipSpellIcon>(entity);
	icon.site = siteEntity;
	icon.slot = slot;
	// WorshipSite::AddSpellIcon 0x77C430: at the head of the site's list
	auto& icons = registry.Get<WorshipSite>(siteEntity).icons;
	icons.insert(icons.begin(), entity);

	// SpellIcon::Create3DSpellObject 0x726210: the graphic at the icon mesh's special point 0 + 1.0 (fn_00726140)
	const auto point = GetSpecialPoint(entity, 0);
	const auto graphicPosition = (point ? point->position : worldPosition) + glm::vec3(0.0f, 1.0f, 0.0f);
	const auto graphic = seed_graphic::Create(graphicPosition, seed, site.player, 1.0f, -1);
	seed_graphic::SetAutoUpdate(graphic, false);
	registry.Get<SpellIcon>(entity).graphic = graphic;
	UpdateGraphicsWithPULevels(entity); // WorshipSpellIcon::CallVirtualFunctionsForCreation 0x77F290
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: icon {} for seed {} at slot {} ({:.1f}, {:.1f}, {:.1f}) of site {}",
		                   static_cast<uint32_t>(entity), static_cast<int>(seed), slot, worldPosition.x, worldPosition.y,
		                   worldPosition.z, static_cast<uint32_t>(siteEntity));
	}
	return entity;
}

void icon::ToBeDeleted(entt::entity iconEntity)
{
	if (!IsIcon(iconEntity))
	{
		return;
	}
	auto& registry = Registry();
	auto& icon = IconOf(iconEntity);
	if (HasSite(icon))
	{
		auto& icons = registry.Get<WorshipSite>(icon.site).icons; // WorshipSite::RemoveSpellIcon 0x77C450
		icons.erase(std::remove(icons.begin(), icons.end(), iconEntity), icons.end());
		icon.site = entt::null;
	}
	for (const auto seed : std::vector<entt::entity>(icon.seeds))
	{
		magic::seed::ToBeDeleted(seed); // vt 0xC on every seed of the icon
	}
	// SpellIcon::ToBeDeleted 0x7260A0: the graphic goes
	const auto& spellIcon = registry.Get<const SpellIcon>(iconEntity);
	seed_graphic::Delete(spellIcon.graphic);
	if (spellIcon.chargeRing != entt::null && registry.Valid(spellIcon.chargeRing))
	{
		registry.Destroy(spellIcon.chargeRing);
	}
	registry.Destroy(iconEntity);
	registry.SetDirty();
}

PlayerNames icon::PlayerOf(entt::entity icon)
{
	return Registry().Get<const SpellIcon>(icon).player;
}

SpellSeedType icon::SeedTypeOf(entt::entity icon)
{
	return Registry().Get<const SpellIcon>(icon).seedType;
}

MagicType icon::MagicTypeOf(entt::entity icon, int powerUp)
{
	return magic::GetMagicInfoFromPULevel(Locator::infoConstants::value(), SeedInfoOf(icon), powerUp).magicType;
}

void icon::UpdateGraphicsWithPULevels(entt::entity icon)
{
	const auto& spellIcon = Registry().Get<const SpellIcon>(icon);
	if (spellIcon.graphic == entt::null)
	{
		return;
	}
	int level = -1;
	for (int pu = 0; pu < 3; ++pu)
	{
		if (magic::players::IsMagicTypeEnabled(spellIcon.player, magic::GetMagicTypeFromPULevel(SeedInfoOf(icon), pu)))
		{
			level = pu;
		}
	}
	seed_graphic::SetPowerUpType(spellIcon.graphic, level);
	seed_graphic::SetAlpha(spellIcon.graphic, 0.5f); // graphic +0x58 = 0.5
}

int icon::Process(entt::entity iconEntity)
{
	auto& icon = IconOf(iconEntity);
	if (icon.removeTimer != 0 && --icon.removeTimer == 0)
	{
		ToBeDeleted(iconEntity);
		return 3;
	}
	if (influence::IsInfluenceEverywhere() && PlayerOf(iconEntity) == PlayerNames::NEUTRAL)
	{
		AddToChantStore(iconEntity, Locator::infoConstants::value().spellIcon.at(0).gatheringChantAddPerGameTurn);
	}
	if (!IconOf(iconEntity).charging)
	{
		return 1;
	}
	if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
	{
		UseCreateChants(iconEntity, seed);
	}
	if (GetChantNeeded(iconEntity) <= 0.0f)
	{
		auto& charged = IconOf(iconEntity);
		if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
		{
			magic::seed::SetPowerUp(seed, charged.powerUp);
		}
		else
		{
			if (charged.hasChargingInterface && ValidForPutFullyChargedSeedInHand(iconEntity, charged.chargingFor))
			{
				PutFullyChargedSeedInHand(iconEntity, charged.chargingFor);
			}
			// PlayFullyChargedSoundFX 0x77F4E0: for the local player's icons, PlaySoundEffect(bank 9, the seed's voice)
			if (magic::players::IsHuman(PlayerOf(iconEntity)) && Locator::audio::has_value())
			{
				const auto seed = static_cast<size_t>(SeedTypeOf(iconEntity));
				const auto name = fmt::format("SpellDialogue.sad/{}", k_FullyChargedVoice.at(seed));
				const auto id = entt::hashed_string(name.c_str()).value();
				if (Locator::resources::value().GetSounds().Contains(id))
				{
					Locator::audio::value().PlaySound(id, audio::PlayType::Once);
				}
			}
		}
		auto& done = IconOf(iconEntity);
		done.charging = false;
		done.hasChargingInterface = false;
		done.powerUp = -1;
		if (trace::Enabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: icon {} fully charged", static_cast<uint32_t>(iconEntity));
		}
	}
	return 1;
}

entt::entity icon::GetHeldSpellSeed(entt::entity icon)
{
	// GPlayer::GetNextInterfaceStatus over the icon's player's hands (fn_005DC830)
	const auto seed = interface::HeldSpellSeed(PlayerOf(icon));
	if (seed != entt::null && Registry().Get<const SpellSeed>(seed).icon == icon)
	{
		return seed;
	}
	return entt::null;
}

float icon::GetChantRequired(entt::entity iconEntity)
{
	const auto& icon = IconOf(iconEntity);
	if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
	{
		return magic::seed::GetChantNeeded(Registry().Get<const SpellSeed>(seed), icon.powerUp);
	}
	return magic::GetChantsRequiredToCreate(Locator::infoConstants::value(), MagicTypeOf(iconEntity, icon.powerUp));
}

float icon::GetChantNeeded(entt::entity iconEntity)
{
	const auto& icon = IconOf(iconEntity);
	if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
	{
		return magic::seed::GetChantNeeded(Registry().Get<const SpellSeed>(seed), icon.powerUp);
	}
	return GetChantRequired(iconEntity) - icon.chantStore;
}

float icon::ChargeFraction(entt::entity iconEntity)
{
	const auto& icon = IconOf(iconEntity);
	if (const auto seed = GetHeldSpellSeed(iconEntity); seed != entt::null)
	{
		// fn_00729890: the seed's need at that level over its cost (inf: the fraction still to charge)
		const auto& component = Registry().Get<const SpellSeed>(seed);
		const float cost = magic::GetChantsRequiredToCreate(Locator::infoConstants::value(), MagicTypeOf(iconEntity, icon.powerUp));
		return cost != 0.0f ? magic::seed::GetChantNeeded(component, icon.powerUp) / cost : 0.0f;
	}
	const float required = GetChantRequired(iconEntity);
	return required != 0.0f ? icon.chantStore / required : 0.0f;
}

bool icon::IsCharging(entt::entity icon, PlayerNames player, bool anyPlayer)
{
	const auto& component = IconOf(icon);
	if (!anyPlayer && (!component.hasChargingInterface || component.chargingFor != player))
	{
		return false;
	}
	return component.charging;
}

float icon::AddToChantStore(entt::entity iconEntity, float chants)
{
	auto& icon = IconOf(iconEntity);
	const float total = chants + icon.chantStore;
	const float required = GetChantRequired(iconEntity);
	if (required < total)
	{
		// the quirk: the excess is what the caller charges to the site
		const float excess = chants - (required - icon.chantStore);
		IconOf(iconEntity).chantStore = required;
		return excess;
	}
	icon.chantStore = total;
	return chants;
}

float icon::RemoveFromChantStore(entt::entity iconEntity, float chants)
{
	auto& icon = IconOf(iconEntity);
	const float removed = chants <= icon.chantStore ? chants : icon.chantStore;
	icon.chantStore -= removed;
	return removed;
}

void icon::ReturnAllChantsToWorshipSite(entt::entity iconEntity)
{
	auto& icon = IconOf(iconEntity);
	if (HasSite(icon))
	{
		Registry().Get<WorshipSite>(icon.site).battery += icon.chantStore;
	}
	RemoveFromChantStore(iconEntity, icon.chantStore);
}

float icon::UseCreateChants(entt::entity iconEntity, entt::entity seedEntity)
{
	auto& icon = IconOf(iconEntity);
	auto& seed = Registry().Get<SpellSeed>(seedEntity);
	const float need = magic::seed::GetChantNeeded(seed, icon.powerUp);
	if (need <= 0.0f)
	{
		return 0.0f;
	}
	const float moved = need < icon.chantStore ? need : icon.chantStore;
	magic::seed::AddToChantStore(seed, moved);
	RemoveFromChantStore(iconEntity, moved);
	return moved;
}

entt::entity icon::CreateSeed(entt::entity icon, const glm::vec3& position, PlayerNames player, int powerUp, float multiplier)
{
	return CreateSeedFromIcon(icon, position, player, powerUp, multiplier);
}

bool icon::ValidForPutFullyChargedSeedInHand(entt::entity icon, PlayerNames player)
{
	return GetChantNeeded(icon) <= 0.0f && GetHeldSpellSeed(icon) == entt::null &&
	       interface::IsHandReadyForObject(player) && magic::players::IsHuman(player);
}

int icon::PutFullyChargedSeedInHand(entt::entity iconEntity, PlayerNames player)
{
	if (!interface::IsHandReadyForObject(player))
	{
		return 0;
	}
	// fn_007282A0(icon +0x14, icon, status, -1, 1.0): at the icon
	const auto seed = CreateSeedFromIcon(iconEntity, Registry().Get<const Transform>(iconEntity).position, player, -1, 1.0f);
	UseCreateChants(iconEntity, seed); // fn_0077FCD0
	if (interface::PlaceSeedInMagicHand(player, seed) != 1)
	{
		return 1;
	}
	auto& registry = Registry();
	if (registry.Valid(seed) && registry.AllOf<SpellSeed>(seed))
	{
		magic::seed::SetInactive(registry.Get<SpellSeed>(seed), true); // fn_00729900(1)
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: seed {} of icon {} in the hand with {:.0f} chants",
		                   static_cast<uint32_t>(seed), static_cast<uint32_t>(iconEntity),
		                   registry.Get<const SpellSeed>(seed).chantStore);
	}
	return 1;
}

bool icon::CancelCharge(entt::entity iconEntity, PlayerNames player)
{
	auto& icon = IconOf(iconEntity);
	if (!icon.charging && icon.chantStore == 0.0f)
	{
		return false;
	}
	if (!icon.hasChargingInterface || icon.chargingFor != player)
	{
		return false;
	}
	icon.charging = false;
	icon.powerUp = -1;
	icon.hasChargingInterface = false;
	ReturnAllChantsToWorshipSite(iconEntity);
	return true;
}

bool icon::StartCharge(entt::entity iconEntity, PlayerNames player, int powerUp, bool requireChants)
{
	auto& icon = IconOf(iconEntity);
	if (icon.charging)
	{
		return false;
	}
	bool chants = true;
	if (HasSite(icon))
	{
		if (requireChants && !(site::TotalChantsAvailable(Registry().Get<const WorshipSite>(icon.site)) > 0.0f))
		{
			chants = icon.chantStore != 0.0f;
		}
	}
	else
	{
		chants = icon.chantStore != 0.0f;
	}
	if (!chants)
	{
		return false;
	}
	icon.powerUp = powerUp;
	icon.charging = true;
	icon.chargingFor = player;
	icon.hasChargingInterface = true;
	icon.chargeStartTurn = magic::CurrentTurn(); // GGame::GetGameTurn (the symbol is the setter 0x77F680)
	if (ValidForPutFullyChargedSeedInHand(iconEntity, player))
	{
		PutFullyChargedSeedInHand(iconEntity, player);
	}
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: icon {} starts charging (pu {}), store {:.0f}",
		                   static_cast<uint32_t>(iconEntity), powerUp, IconOf(iconEntity).chantStore);
	}
	return true;
}

bool icon::ValidForStartCharge(entt::entity iconEntity, PlayerNames player, int powerUp, bool requireChants)
{
	const auto& icon = IconOf(iconEntity);
	if (icon.charging || GetHeldSpellSeed(iconEntity) != entt::null)
	{
		return false;
	}
	if (!magic::players::IsMagicTypeEnabled(player, MagicTypeOf(iconEntity, powerUp)))
	{
		return false;
	}
	if (HasSite(icon))
	{
		if (!requireChants || site::TotalChantsAvailable(Registry().Get<const WorshipSite>(icon.site)) > 0.0f)
		{
			return true;
		}
	}
	return icon.chantStore != 0.0f;
}

bool icon::ValidForRequestSpell(entt::entity icon, PlayerNames player, int powerUp, bool requireChants)
{
	// vt 0xD4 IsBuilt (WorshipSpellIcon::IsBuilt 0x77FEE0: the site's; openblack's sites are always built)
	if (ValidForPutFullyChargedSeedInHand(icon, player))
	{
		return true;
	}
	return ValidForStartCharge(icon, player, powerUp, requireChants);
}

bool icon::RequestSpell(entt::entity icon, PlayerNames player, int powerUp, bool requireChants)
{
	if (ValidForPutFullyChargedSeedInHand(icon, player))
	{
		return PutFullyChargedSeedInHand(icon, player) == 1;
	}
	if (ValidForStartCharge(icon, player, powerUp, requireChants))
	{
		return StartCharge(icon, player, powerUp, requireChants);
	}
	return false;
}

bool icon::PowerUpValid(entt::entity icon, PlayerNames player, int powerUp)
{
	return GetHeldSpellSeed(icon) != entt::null &&
	       magic::players::IsMagicTypeEnabled(player, MagicTypeOf(icon, powerUp));
}

bool icon::SetChargingPowerUp(entt::entity iconEntity, PlayerNames player, int powerUp)
{
	if (!PowerUpValid(iconEntity, player, powerUp))
	{
		return false;
	}
	auto& icon = IconOf(iconEntity);
	icon.charging = true;
	icon.powerUp = powerUp;
	icon.chargingFor = player;
	icon.hasChargingInterface = true;
	icon.chargeStartTurn = magic::CurrentTurn();
	return true;
}

int icon::ActualInterfaceTap(entt::entity icon, PlayerNames player)
{
	if (ValidForPutFullyChargedSeedInHand(icon, player))
	{
		return PutFullyChargedSeedInHand(icon, player);
	}
	if (CancelCharge(icon, player))
	{
		return 1;
	}
	if (interface::IsHandReadyForObject(player) && ValidForStartCharge(icon, player, -1, false))
	{
		return StartCharge(icon, player, -1, false) ? 1 : 0;
	}
	return 0;
}

bool icon::InterfaceValidToTap(entt::entity icon, PlayerNames player)
{
	auto& registry = Registry();
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<SpellIcon>(icon))
	{
		return false;
	}
	const auto owner = registry.Get<const SpellIcon>(icon).player;
	if (influence::IsInfluenceEverywhere() && owner == PlayerNames::NEUTRAL)
	{
		return true;
	}
	return owner == player;
}

int icon::InterfaceTap(entt::entity icon, PlayerNames player)
{
	const auto worshipIcon = WorshipIconOf(icon);
	if (worshipIcon == entt::null)
	{
		return 0;
	}
	if (ActualInterfaceTap(worshipIcon, player) == 1 && magic::players::IsHuman(player))
	{
		PlayTapSound(PlacementIndexOf(icon));
	}
	return 1;
}

float icon::MaintainSpell(entt::entity iconEntity, float amount)
{
	auto& icon = IconOf(iconEntity);
	if (HasSite(icon))
	{
		return site::MaintainSpell(icon.site, amount);
	}
	if (amount <= icon.chantStore)
	{
		icon.chantStore -= amount;
		return amount;
	}
	const float store = icon.chantStore;
	icon.chantStore = 0.0f;
	return store;
}

void icon::AddSeed(entt::entity iconEntity, entt::entity seed)
{
	auto& registry = Registry();
	registry.Get<SpellSeed>(seed).flags |= 1u;
	auto& seeds = IconOf(iconEntity).seeds;
	if (std::ranges::find(seeds, seed) == seeds.end())
	{
		seeds.insert(seeds.begin(), seed);
	}
}

void icon::RemoveSeed(entt::entity iconEntity, entt::entity seed)
{
	auto& registry = Registry();
	if (IsIcon(iconEntity))
	{
		auto& seeds = IconOf(iconEntity).seeds;
		seeds.erase(std::remove(seeds.begin(), seeds.end(), seed), seeds.end());
	}
	if (registry.Valid(seed) && registry.AllOf<SpellSeed>(seed))
	{
		registry.Get<SpellSeed>(seed).flags &= static_cast<uint8_t>(~1u);
	}
}

void icon::UpdateChargingVisual(entt::entity iconEntity, float phase)
{
	auto& registry = Registry();
	auto& spellIcon = registry.Get<SpellIcon>(iconEntity);
	const bool charging = IsCharging(iconEntity, PlayerNames::NEUTRAL, true);
	// TChargingData::Draw 0x7267A0: while charging, t = (phase - 0.1, wrapped) x 3.33 (fn_0068F720(0.1)); for 0 <= t < 1
	// the MSH_S_PULSE_IN ring on the icon's matrix, scaled 3 (1 - t), alpha 255 (1 - t), raised 0.1 x twice the
	// mesh's height (the 0.1 of 0x981A38), in the player's colour
	float t = -1.0f;
	if (charging)
	{
		t = (phase <= 0.1f ? phase + 1.0f : phase) - 0.1f;
		t *= 3.33f;
	}
	const bool visible = charging && t >= 0.0f && t < 1.0f;
	if (!visible)
	{
		if (spellIcon.chargeRing != entt::null && registry.Valid(spellIcon.chargeRing))
		{
			registry.Destroy(spellIcon.chargeRing);
			registry.SetDirty();
		}
		spellIcon.chargeRing = entt::null;
		return;
	}
	const auto& transform = registry.Get<const Transform>(iconEntity);
	if (spellIcon.chargeRing == entt::null || !registry.Valid(spellIcon.chargeRing))
	{
		spellIcon.chargeRing = registry.Create();
		registry.Assign<Transform>(spellIcon.chargeRing, transform);
		registry.Assign<Mesh>(spellIcon.chargeRing, resources::HashIdentifier(k_PulseMesh), static_cast<int8_t>(0),
		                      static_cast<int8_t>(0));
		registry.Assign<Alpha>(spellIcon.chargeRing, 1.0f);
	}
	auto& ring = registry.Get<Transform>(spellIcon.chargeRing);
	const float scale = 3.0f * (1.0f - t);
	ring = transform;
	ring.scale = transform.scale * scale;
	registry.Get<Alpha>(spellIcon.chargeRing).value = 1.0f - t;
	registry.SetDirty();
}
