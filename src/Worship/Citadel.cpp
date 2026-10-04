/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Citadel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "Audio/Services/GameMusic.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/Town/BuildingSites.h"
#include "Help/HelpSystem.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "TownMagic.h"
#include "WorshipSite.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

bool IsCitadel(entt::entity citadel)
{
	return citadel != entt::null && Registry().Valid(citadel) && Registry().AllOf<CitadelWorship, Temple, Transform>(citadel);
}

/// CitadelHeart::Built 0x4650CD: StartScriptMusic(0x3D)
constexpr int k_TempleBuiltMusic = 0x3D;
/// SaveGameRoom::InstantSaveGame(0x14) 0x792FB0 (0x4650F1)
constexpr int k_TempleBuiltSave = 0x14;

/// GScript +0xA0 (SET_INTERFACE_CITADEL); GScript::Reset 0x6EB312: 1
uint32_t g_InterfaceCitadel = 1;

} // namespace

entt::entity citadel::Of(PlayerNames player)
{
	entt::entity found = entt::null;
	Registry().Each<const Temple, const CitadelWorship>([&](entt::entity entity, const Temple& temple, const CitadelWorship&) {
		if (found == entt::null && temple.owner == player)
		{
			found = entity;
		}
	});
	return found;
}

std::array<entt::entity, 6> citadel::WorshipSitesOf(PlayerNames player)
{
	std::array<entt::entity, 6> sites {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	// 0x71B2A5..0x71B2AD: GPlayer +0xA48, none -> nothing
	const auto citadelEntity = Of(player);
	if (!IsCitadel(citadelEntity))
	{
		return sites;
	}
	// 0x71B2B3..0x71B301: +0x34 + 4 i, i = 0..5 (CitadelWorship::sites is indexed by the slot, WorshipSite +0x110)
	const auto& worship = Registry().Get<const CitadelWorship>(citadelEntity);
	for (size_t i = 0; i < sites.size(); ++i)
	{
		const auto site = worship.sites.at(i);
		sites.at(i) = site != entt::null && Registry().Valid(site) && Registry().AllOf<WorshipSite>(site) ? site : entt::null;
	}
	return sites;
}

float citadel::StrainSoundFraction(entt::entity citadelEntity)
{
	return IsCitadel(citadelEntity) ? Registry().Get<const CitadelWorship>(citadelEntity).strainSoundFraction : 0.0f;
}

float citadel::StrainSoundFractionAtMostOne(entt::entity citadelEntity)
{
	const float fraction = StrainSoundFraction(citadelEntity);
	// 0x71B319..0x71B332: fld +0x70; fcomp 1.0 (0x8AA390); C0 (below or unordered) -> +0x70, else 1.0 (0x3F800000)
	return fraction < 1.0f || std::isnan(fraction) ? fraction : 1.0f;
}

entt::entity citadel::FindNearestWorshipSite(entt::entity citadelEntity, const ecs::map_coords::MapCoords& coords,
                                             float maxDistance)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	const auto& worship = Registry().Get<const CitadelWorship>(citadelEntity);
	entt::entity best = entt::null;
	float bestDistance = maxDistance; // [esp + 0x24], the argument
	// 0x4639A9..0x463A07: +0x34 + 4 i, i = 0..5
	for (const auto site : worship.sites)
	{
		// 0x4639B3..0x4639C0: a site, and fn_0077B960 (the dance's +0x90) != 0
		if (site == entt::null || !Registry().Valid(site) || !Registry().AllOf<WorshipSite>(site) ||
		    site::DancerCount(site) == 0)
		{
			continue;
		}
		// 0x4639C2..0x4639D7: a zeroed MapCoords filled by fn_0077CD90 (GetSpecialPos 8); (inferido) a mesh without
		// the point leaves it at 0, as the original's zeroed MapCoords
		ecs::map_coords::MapCoords centre {};
		if (const auto point = site::GetSpecialPos(site, site::Point::DanceCentre); point)
		{
			centre = ecs::map_coords::FromWorld(*point);
		}
		// 0x4639DC..0x4639F7: GetDistanceInMetres(centre, coords) < best (fcom; test ah, 1)
		const float distance = gutils::GetDistanceInMetres(centre, coords);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = site;
		}
	}
	return best;
}

bool citadel::HasLivingHeart(entt::entity citadelEntity)
{
	// 0x71C56E..0x71C5A6: the heart (+0x30), IsBuilt, GetLife > 0 (fcomp 0; test ah, 0x41: not below nor equal)
	const auto heart = HeartOf(citadelEntity);
	return heart != entt::null && ecs::abodes::IsBuilt(heart) && ecs::life::LifeOf(heart) > 0.0f;
}

void citadel::Initialise(entt::entity temple, float heartYAngle)
{
	auto& worship = Registry().AssignOrReplace<CitadelWorship>(temple);
	worship.heartYAngle = heartYAngle;
}

entt::entity citadel::AddTown(entt::entity citadelEntity, entt::entity town)
{
	const auto site = FindOrCreateWorshipSite(citadelEntity, town);
	if (site == entt::null)
	{
		return entt::null;
	}
	const auto& towns = Registry().Get<const WorshipSite>(site).towns;
	if (std::ranges::find(towns, town) == towns.end())
	{
		site::AddTown(site, town);
	}
	// the heart's vt 0x5B0(1.0) (UNVERIFIED: a refresh of the heart; nothing to do here)
	return site;
}

entt::entity citadel::FindOrCreateWorshipSite(entt::entity citadelEntity, entt::entity town)
{
	if (!IsCitadel(citadelEntity) || town == entt::null)
	{
		return entt::null;
	}
	if (Registry().Get<const CitadelWorship>(citadelEntity).cannotCreateSites || !town::IsAllowedToCreateWorshipSite(town))
	{
		return entt::null;
	}
	const auto* tribe = Registry().TryGet<const Tribe>(town);
	return tribe != nullptr ? FindOrCreateWorshipSite(citadelEntity, *tribe) : entt::null;
}

entt::entity citadel::FindOrCreateWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	if (const auto site = FindTribeWorshipSite(citadelEntity, tribe); site != entt::null)
	{
		return site;
	}
	// Citadel::RequestANewWorshipSite 0x4633F0: Town::GetNearestTownToPos 0x73B170 (the citadel's MapCoords +0x14, the
	// tribe, 0x7FFF = any abode type, FLT_MAX [0x8C7E1C]) at 0x46345C; that town's MapCoords, else the citadel's
	auto near = Registry().Get<const Transform>(citadelEntity).position;
	if (const auto town = ecs::map_cells::GetNearestTownToPos(ecs::map_coords::FromWorld(near), tribe,
	                                                          ecs::map_cells::k_AnyAbodeType,
	                                                          std::numeric_limits<float>::max());
	    town != entt::null)
	{
		near = Registry().Get<const Transform>(town).position;
	}
	// WorshipSite::Create 0x77AC50(..., angle 0, scale 1.0, percent 0, underConstruction 0) (0x46347F..0x463492): the
	// MultiMapFixed ctor gives +0x58 bit 3 and +0x5C = 0 (0x52E22D..0x52E234), so IsBuilt 0x77BDD0 (+0x5C >= 1) is
	// false until it is built; WorshipSite::Draw 0x5193D0 then takes DrawBuilding 0x517F90, nothing at 0 %
	const auto site = site::Create(citadelEntity, tribe, near);
	if (site != entt::null)
	{
		auto& part = Registry().AssignOrReplace<CitadelPartBuild>(site);
		part.buildFlags = CitadelPartBuild::k_Built;
		part.percentBuilt = 0.0f;
		ecs::abodes::RedrawConstruction(site);
	}
	return site;
}

entt::entity citadel::FindTribeWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	for (const auto site : Registry().Get<const CitadelWorship>(citadelEntity).sites)
	{
		if (site != entt::null && Registry().Valid(site) && Registry().Get<const WorshipSite>(site).tribe == tribe)
		{
			return site;
		}
	}
	return entt::null;
}

void citadel::ProcessSpellIcons(entt::entity citadelEntity)
{
	if (!IsCitadel(citadelEntity))
	{
		return;
	}
	float strain = 0.0f;
	for (const auto site : std::array(Registry().Get<const CitadelWorship>(citadelEntity).sites))
	{
		if (site == entt::null || !Registry().Valid(site))
		{
			continue;
		}
		site::ProcessSpellIcons(site);
		const float siteStrain = Registry().Get<const WorshipSite>(site).strain;
		if (siteStrain > strain)
		{
			strain = siteStrain;
		}
	}
	// Citadel::SetWorshipStrainSoundFrac 0x463850 (the local player's citadel only)
	auto& worship = Registry().Get<CitadelWorship>(citadelEntity);
	if (!magic::players::IsHuman(Registry().Get<const Temple>(citadelEntity).owner) || worship.strainSoundFraction == strain)
	{
		return;
	}
	// 0x4638A4..0x4638B9: mov eax, [0xD01A38]; fild qword (zero high half); fmul [0x8C7E30] = 0.001f, read every turn
	const float step = static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	if (worship.strainSoundFraction > strain)
	{
		worship.strainSoundFraction -= step;
		if (worship.strainSoundFraction <= strain)
		{
			worship.strainSoundFraction = strain;
		}
	}
	else
	{
		worship.strainSoundFraction += step;
		if (!(worship.strainSoundFraction < strain))
		{
			worship.strainSoundFraction = strain;
		}
	}
}

entt::entity citadel::CreateBuiltWorshipSite(entt::entity citadelEntity, Tribe tribe)
{
	// 0x465115: GetPlayer (vt +0x1C); 0x465123 Citadel::FindOrCreateWorshipSite(GTribeInfo*) 0x463220; none -> 0
	const auto site = FindOrCreateWorshipSite(citadelEntity, tribe);
	if (site == entt::null)
	{
		return entt::null;
	}
	const auto player = Registry().Get<const Temple>(citadelEntity).owner;
	for (const auto town : ecs::map_cells::TownsOf(player))
	{
		const auto* townTribe = Registry().TryGet<const Tribe>(town);
		if (townTribe == nullptr || *townTribe != tribe) // Town +0x5B8: the tribe index (0x465151)
		{
			continue;
		}
		// 0x46517A..0x465192: not in the site's +0xA4 list -> WorshipSite::AddTown 0x77C800
		const auto& towns = Registry().Get<const WorshipSite>(site).towns;
		if (std::ranges::find(towns, town) == towns.end())
		{
			site::AddTown(site, town);
		}
		// 0x465197..0x4651B5: the town's building site of it -> BuildBy(1.0) (vt +0x900, WorshipSite 0x77DC50) and
		// Town::RemoveBuildingSite 0x73BA20; none: the site stays as it is
		if (ecs::building_sites::GetBuildingSiteInList(town, site) != entt::null)
		{
			ecs::abodes::BuildBy(site, 1.0f);
			ecs::building_sites::RemoveBuildingSite(town, site);
		}
		return site;
	}
	// 0x465163..0x465174: no town of that tribe -> BuildBy(1.0) and 0
	ecs::abodes::BuildBy(site, 1.0f);
	return entt::null;
}

void citadel::OpenWorshipSites(entt::entity citadelEntity, float boost)
{
	if (!IsCitadel(citadelEntity))
	{
		return;
	}
	// fn_464F50: each town of GetPlayer() +0xA50 (next +0x75C)
	for (const auto town : ecs::map_cells::TownsOf(Registry().Get<const Temple>(citadelEntity).owner))
	{
		// Citadel::FindOrCreateWorshipSite(Town*) 0x4631D0; none -> the next town
		const auto site = FindOrCreateWorshipSite(citadelEntity, town);
		if (site == entt::null)
		{
			continue;
		}
		// the town not in the site's +0xA4 list -> WorshipSite::AddTown 0x77C800
		const auto& towns = Registry().Get<const WorshipSite>(site).towns;
		if (std::ranges::find(towns, town) == towns.end())
		{
			site::AddTown(site, town);
		}
		// IsBuilt == 1 && IsRepaired (vt +0x890 / +0x88C) -> the next town
		if (ecs::abodes::IsBuilt(site) && ecs::abodes::IsRepaired(site))
		{
			continue;
		}
		// Town::GetBuildingSiteInList 0x73CE40, else Town::AddBuildingSite(MultiMapFixed*) 0x73B8E0; none -> the next
		auto buildingSite = ecs::building_sites::GetBuildingSiteInList(town, site);
		if (buildingSite == entt::null)
		{
			buildingSite = ecs::building_sites::AddBuildingSite(town, site);
		}
		if (buildingSite == entt::null)
		{
			continue;
		}
		ecs::building_sites::SetDesireBoost(buildingSite, boost); // +0x63C (0x464FCE)
	}
}

entt::entity citadel::GetSpellIcon(entt::entity citadelEntity, MagicType type)
{
	if (!IsCitadel(citadelEntity))
	{
		return entt::null;
	}
	for (const auto site : Registry().Get<const CitadelWorship>(citadelEntity).sites)
	{
		if (site != entt::null && Registry().Valid(site))
		{
			if (const auto icon = site::GetSpellIconFromMagicType(site, type); icon != entt::null)
			{
				return icon;
			}
		}
	}
	return entt::null;
}

void citadel::PostLoadCleanup()
{
	std::vector<std::pair<entt::entity, PlayerNames>> citadels;
	Registry().Each<const Temple, const CitadelWorship>([&](entt::entity entity, const Temple& temple, const CitadelWorship&) {
		citadels.emplace_back(entity, temple.owner);
	});
	for (const auto& [citadelEntity, player] : citadels)
	{
		for (const auto town : ecs::map_cells::TownsOf(player))
		{
			const auto* magic = Registry().TryGet<const TownMagic>(town);
			if (magic != nullptr && magic->worshipSite == entt::null) // vt 0x30C GetWorshipSite
			{
				AddTown(citadelEntity, town);
			}
		}
	}
}

entt::entity citadel::HeartOf(entt::entity citadelEntity)
{
	return IsCitadel(citadelEntity) && Registry().AllOf<CitadelHeart>(citadelEntity) ? citadelEntity : entt::null;
}

void citadel::Process(entt::entity citadelEntity)
{
	// 0x462D79..0x462D89: +0x30 the heart and its 3D object +0x40; none -> return
	const auto heart = HeartOf(citadelEntity);
	if (heart == entt::null)
	{
		return;
	}
	// 0x462D8F..0x462DE4: the player's alignment (GetAlignmentValue 0x64D6A0, fn_64ACC0) into the 3D object (vt +0x218
	// / +0x20C). (pending)
	// 0x462DE4..0x462E4F: old = +0x40 +0x9C; p = GetPercentBuilt (vt +0x880); old < 1 && p >= 1 -> RemoveMapObject (vt
	// +0x548), SetPercent(p), InsertMapObject (vt +0x544); else SetPercent(p)
	const float old = Registry().Get<const CitadelHeart>(heart).drawPercent;
	const float percent = ecs::abodes::GetPercentBuilt(heart);
	if (old < 1.0f && !(percent < 1.0f))
	{
		ecs::map_cells::RemoveMapObject(heart);
		SetHeartDrawPercent(heart, percent);
		ecs::map_cells::InsertMapObject(heart);
	}
	else
	{
		SetHeartDrawPercent(heart, percent);
	}
	// 0x462E55..0x463068: the alignment again, the effects fn_4630E0 / fn_454AA0 and CitadelHeart::SetAlignmentFlock
	// 0x465270. (pending)
}

void citadel::SetHeartDrawPercent(entt::entity heart, float percent)
{
	auto* h = heart != entt::null && Registry().Valid(heart) ? Registry().TryGet<CitadelHeart>(heart) : nullptr;
	if (h == nullptr)
	{
		return;
	}
	// 0x883120: p < 0 -> 0; p > 1 -> 1
	if (percent < 0.0f)
	{
		percent = 0.0f;
	}
	if (percent > 1.0f)
	{
		percent = 1.0f;
	}
	// p == 1 && +0x9C < 1 -> vt +0x80(edx = 1) ((pending) the flag it sets); +0x9C = p; p == 1 -> [0xEB9A1C + 4 x
	// +0xA8] = 1 ((pending) a per-player "temple complete" flag, its readers not searched)
	h->drawPercent = percent;
	ecs::abodes::RedrawConstruction(heart); // the draw 0x882A40
}

void citadel::HeartBuilt(entt::entity heart)
{
	auto& registry = Registry();
	const auto* h = registry.TryGet<const CitadelHeart>(heart);
	if (h == nullptr)
	{
		return;
	}
	// 2. player = GetPlayer() (0x468020: +0x80 ? the citadel's player : the town's); SetLife(1.0) (vt +0x5B0, 0x46501B)
	const auto player = registry.Get<const Temple>(heart).owner;
	ecs::life::SetLife(heart, 1.0f);
	// 3. each town of the player (+0xA50, next +0x75C): GetBuildingSiteInList(this) -> RemoveBuildingSite(this)
	for (const auto town : ecs::map_cells::TownsOf(player))
	{
		if (ecs::building_sites::GetBuildingSiteInList(town, heart) != entt::null)
		{
			ecs::building_sites::RemoveBuildingSite(town, heart);
		}
	}
	// 4. GetCitadel (vt +0x114 = +0x80); none -> 1
	const auto citadelEntity = registry.Get<const CitadelHeart>(heart).citadel;
	if (citadelEntity == entt::null)
	{
		return;
	}
	OpenWorshipSites(citadelEntity, 0.0f); // fn_464F50(citadel, 0) (0x46506A)
	// the local player (GetPlayer() == g_game +0x18 + 0xA60 x byte g_game +0x205A59, 0x46506F..0x465096).
	// (approximate) IsHuman stands for it, as ProcessSpellIcons above (openblack's local player is the human one)
	if (!magic::players::IsHuman(player))
	{
		return;
	}
	// !((g_game +0x25005C) +0x45E8 && +0x45EC): not in a script's wide screen (0x465098..0x4650B6)
	if (const auto* help = help::Get(); help != nullptr && help->IsScriptWideScreen())
	{
		return;
	}
	// !GAudio +0x28 (no script music) && g_game +0x205A08 != 1 -> GAudio::StartScriptMusic(0x3D) 0x428230
	// (0x4650B8..0x4650D1)
	{
		const auto lock = audio::game_music::Lock();
		if (auto* music = audio::game_music::Get();
		    music != nullptr && music->GetScriptType() == 0 && influence::LandNumber() != 1)
		{
			music->StartScriptMusic(k_TempleBuiltMusic);
		}
	}
	// !g_game +0x205A0C (a playground game: openblack has none) && !IsMultiplayerGame -> SaveGameRoom::InstantSaveGame(
	// 0x14) 0x792FB0. (not ported) openblack has no save games
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Citadel: the temple is built; InstantSaveGame({:#x}) not ported",
	                   k_TempleBuiltSave);
}

void citadel::WorshipSiteBuilt(entt::entity siteEntity)
{
	// 0x77AC19..0x77AC46: each town of GetPlayer() (CitadelPart 0x469750, the citadel's): GetBuildingSiteInList ->
	// RemoveBuildingSite
	const auto* site = Registry().TryGet<const WorshipSite>(siteEntity);
	if (site == nullptr)
	{
		return;
	}
	for (const auto town : ecs::map_cells::TownsOf(site->player))
	{
		if (ecs::building_sites::GetBuildingSiteInList(town, siteEntity) != entt::null)
		{
			ecs::building_sites::RemoveBuildingSite(town, siteEntity);
		}
	}
}

void citadel::SetInterfaceCitadel(uint32_t value)
{
	g_InterfaceCitadel = value; // (g_game +0x250090) +0xA0 = POP() (0x70B9BD)
}

uint32_t citadel::InterfaceCitadel()
{
	return g_InterfaceCitadel;
}

void citadel::ResetInterfaceCitadel()
{
	g_InterfaceCitadel = 1; // GScript::Reset 0x6EB312
}

bool citadel::EntranceValidToTap([[maybe_unused]] entt::entity entrance)
{
	// 0x468F50: GGame::IsMultiplayerGame 0x552F80 (openblack: never) ? 1 : GScript +0xA0 != 0
	return g_InterfaceCitadel != 0;
}

uint32_t citadel::EntranceTap(entt::entity entrance, bool myInterface)
{
	// 0x468EF0: +0x54 the heart; its GetPlayer (vt +0x1C) the local player (g_game +0x18 + 0xA60 x byte g_game
	// +0x205A59) and the status MyInterfaceStatus 0x555880 -> GGame::GoInsideCitadel(0, 0) 0x553E10; returns 1
	const auto* door = Registry().TryGet<const CitadelEntrance>(entrance);
	const auto heart = door != nullptr ? door->heart : entt::null;
	if (heart == entt::null || !Registry().Valid(heart) || !Registry().AllOf<Temple>(heart))
	{
		return 1;
	}
	// (approximate) IsHuman stands for "the local player", as in HeartBuilt
	if (magic::players::IsHuman(Registry().Get<const Temple>(heart).owner) && myInterface)
	{
		// (approximate) GoInsideCitadel's own steps (0x553E10..0x554004: g_game +0x205A28 = 1, the camera) are the
		// temple interior's Activate, as ENTER_EXIT_CITADEL(1) does
		if (Locator::temple::has_value() && !Locator::temple::value().Active())
		{
			Locator::temple::value().Activate();
		}
	}
	return 1;
}
