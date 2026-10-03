/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AudioQueries.h"

#include <cstdio>
#include <cstdlib>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/Clouds.h"
#include "3D/L3DAnim.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/GameQueries.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "ECS/Animations.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Weather/Atmos.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Resources/ResourcesInterface.h"
#include "Video/VideoPlayer.h"
#include "Worship/Citadel.h"
#include "Worship/WorshipSite.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
std::optional<audio::AnimatedThing> AnimatedThing(entt::entity entity)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	// fn_00516510 0x516548: Get3DSoundPos (GameThingWithPos: its position), else nothing
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	audio::AnimatedThing thing;
	thing.position = transform->position;
	if (const auto* action = registry.TryGet<const LivingAction>(entity); action != nullptr)
	{
		thing.turnsSinceStateChange = action->turnsSinceStateChange;
	}
	if (const auto* villager = registry.TryGet<const Villager>(entity); villager != nullptr)
	{
		audio::AnimatedThing::Villager v;
		v.alive = villager->life > 0.0f; // IsAlive 0x5165BC: Object +0x48
		v.child = villager->lifeStage == Villager::LifeStage::Child;
		v.female = villager->sex == Villager::Sex::FEMALE;
		if (villager->abode != entt::null && registry.Valid(villager->abode))
		{
			v.abode = villager->abode; // Villager::GetAbode 0x51675D
		}
		thing.villager = v;
	}
	return thing;
}

std::optional<std::string> AnimationClipName(int32_t index)
{
	if (!Locator::resources::has_value() || index < 0)
	{
		return std::nullopt;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	const auto id = ecs::ClipId(static_cast<uint32_t>(index));
	if (!animations.Contains(id))
	{
		return std::nullopt;
	}
	// the header's name is 32 chars padded with zeros
	return std::string(animations.Handle(id)->GetName().c_str());
}

std::vector<audio::StreetLantern> StreetLanterns()
{
	std::vector<audio::StreetLantern> lanterns;
	if (!Locator::entitiesRegistry::has_value())
	{
		return lanterns;
	}
	Locator::entitiesRegistry::value().Each<const StreetLantern, const Transform>(
	    [&lanterns](entt::entity entity, const StreetLantern& /*unused*/, const Transform& transform) {
		    // Object::GetHeight 0x638120
		    lanterns.push_back({entity, transform.position, ecs::object::GetHeight(entity)});
	    });
	return lanterns;
}

audio::CameraWeatherInfo WeatherSmooth(glm::vec3 point)
{
	// LH3DAtmos::GetWeatherSmooth 0x835180 with recalc (GCamera::Update's call for GCamera+0x80)
	const auto smooth = weather::atmos::GetWeatherSmooth(point, true);
	audio::CameraWeatherInfo info;
	info.temperature = smooth.temperature;
	info.rain = smooth.rain;
	info.snow = smooth.snow;
	info.overcast = smooth.overcast;
	info.windX = smooth.windX;
	info.windZ = smooth.windZ;
	return info;
}

float CameraAlignment()
{
	// fn_005E2240's argument x: fn_0064AC30 (GPlayer::ProcessPlayers 0x64A697, once a turn) passes clamp((the
	// GPlayer::GetAlignmentValue of MapCoords::CalculateMostInfluentialPlayer at the interface's camera position + 1) / 2,
	// 0, 1), ecs::effects::alignment::GetInterfaceAlignment(). The sky takes the same x (fn_005E2240 stores 2 (1 - x) at
	// 0xBF337C), so the sky's openblack overrides (OPENBLACK_TEST_SKY_ALIGNMENT, the debug slider) reach the audio too:
	// Clouds::InfluentialPlayerAlignment is 2x - 1, or the override's -1..1.
	float x = ecs::effects::alignment::GetInterfaceAlignment();
	if (const float sky = Clouds::InfluentialPlayerAlignment(); sky != x * 2.0f - 1.0f)
	{
		x = (sky + 1.0f) * 0.5f; // (openblack) an override is on
	}
	return ecs::audio_queries::GAudioAlignment(x);
}

/// GGame::GetCamera()+0x14, the camera's MapCoords (fn_00427460's argument and the point of its GetDistanceInMetres
/// 0x4274E3 / 0x427519): LH3DTech::g_camera, the render camera's position, x and z x 6553.6 __ftol
/// (GCamera::UpdateGameThingWithPosData 0x442EF3..0x442F35, from GCamera::Update 0x4426EB), GameQueries::camera's point
std::optional<ecs::map_coords::MapCoords> CameraMapCoords()
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	return ecs::map_coords::FromWorld(Locator::camera::value().GetOrigin());
}

/// A town as fn_00427460 reads it: Town +0x5B8 (the Tribe given to CREATE_TOWN, TownArchetype; 0x42753D / 0x42755A) and
/// GUtils::GetDistanceInMetres 0x74CD70(camera +0x14, town +0x14) (0x4274D9..0x4274EC, 0x427515..0x427522); nullopt
/// when the entity is no longer a town (GameThing::IsAvailable, vt +0x2C, 0x4274AF)
std::optional<audio::MusicTown> MusicTownOf(entt::entity town, const ecs::map_coords::MapCoords& camera)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (town == entt::null || !registry.Valid(town) || registry.TryGet<const Town>(town) == nullptr)
	{
		return std::nullopt;
	}
	audio::MusicTown music;
	music.id = static_cast<uint32_t>(entt::to_integral(town));
	const auto* tribe = registry.TryGet<const Tribe>(town);
	music.tribe = static_cast<int>(tribe != nullptr ? *tribe : Tribe::NONE);
	music.distance = gutils::GetDistanceInMetres(camera, ecs::object::MapCoordsOf(town));
	return music;
}

std::optional<audio::MusicTown> NearestMusicTown(float maxDistance)
{
	const auto camera = CameraMapCoords();
	if (!camera || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	// fn_00602160(camera, townTriggerOffDistance) 0x427493: every player and the neutral one, GetDistanceInMetres
	// (fn_00605CD0) < best (strictly, 0x60219C), only a town with +0x9A4 (0x6021A7) or else fn_00741020 (0x6021B3: a
	// TownCentre among its abodes +0x754, or a planned one of abode number 0xC in +0x9A8; map_cells::TownHasCentre)
	return MusicTownOf(ecs::map_cells::GetNearestTownWithCentre(*camera, maxDistance), *camera);
}

std::optional<audio::MusicTown> KeptMusicTown(uint32_t id)
{
	const auto camera = CameraMapCoords();
	if (!camera || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	// GAudio+0x18C again: IsAvailable (0x4274AF) and its distance to the camera (0x427515..0x427522)
	return MusicTownOf(static_cast<entt::entity>(id), *camera);
}

std::optional<audio::ThingId> NearestTownAt(glm::vec3 point, float maxDistance)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	// MapCoords::GetNearestTown 0x6020E0(maxDistance) on the MapCoords of ResourceDropSFX 0x71B591 (MapCoords(LHPoint)
	// 0x603160 of the point): GetDistanceInMetres < best (strictly), every player and the neutral one
	const auto town = ecs::map_cells::GetNearestTown(ecs::map_coords::FromWorld(point), maxDistance);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	return static_cast<audio::ThingId>(entt::to_integral(town));
}

std::vector<audio::DesireTown> DesireTowns()
{
	std::vector<audio::DesireTown> towns;
	if (!Locator::entitiesRegistry::has_value())
	{
		return towns;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// CheckTownDesiresSFX 0x71B130: GetNextPlayer x each player's town list (map_cells::ForEachTown, the neutral last)
	ecs::map_cells::ForEachTown([&registry, &towns](entt::entity entity) {
		const auto* t = registry.TryGet<const Town>(entity);
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (t == nullptr || transform == nullptr)
		{
			return true;
		}
		audio::DesireTown town;
		town.id = static_cast<audio::ThingId>(entt::to_integral(entity));
		town.position = transform->position; // Town +0x14
		// GetStoragePit 0x73B5B0 (IsAvailable), its +0x14
		if (const auto pit = ecs::town_queries::GetStoragePit(entity); pit != entt::null)
		{
			if (const auto* pitTransform = registry.TryGet<const Transform>(pit); pitTransform != nullptr)
			{
				town.storagePit = pitTransform->position;
			}
		}
		town.population = t->stats.adults + t->stats.children; // +0x618 + +0x61C
		// Town +0x378 (TownDesire +0x344, order 2): value +0x37C, type +0x380; GetRawDesire(type) 0x73E420
		const auto& sorted = ecs::town_desire::GetSortedRawDesires(entity);
		for (size_t k = 0; k < sorted.size(); ++k)
		{
			town.desires.at(k).value = sorted.at(k).value;
			town.desires.at(k).type = sorted.at(k).index;
			town.desires.at(k).raw =
			    ecs::town_desire::GetRawDesire(entity, static_cast<TownDesireInfo>(sorted.at(k).index));
		}
		towns.push_back(town);
		return true;
	});
	return towns;
}

std::optional<std::array<float, 3>> TownResourceNeeds(audio::ThingId id)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto town = static_cast<entt::entity>(id);
	if (!Locator::entitiesRegistry::value().Valid(town) || !Locator::entitiesRegistry::value().AnyOf<Town>(town))
	{
		return std::nullopt;
	}
	// GetResourceDropSample 0x71B5F0: Town +0x19C + +0x108 + +0xC4 (TownDesire +0x168 Raw, +0xD4 Boost, +0x90 BoostA) of
	// the desires 0 (food), 1 (wood) and 10 (rain), added in that order and stored as floats
	using ecs::town_desire::Field;
	const auto need = [town](TownDesireInfo d) {
		const float raw = ecs::town_desire::GetField(town, d, Field::Raw);
		const float rawBoost = raw + ecs::town_desire::GetField(town, d, Field::Boost);
		return rawBoost + ecs::town_desire::GetField(town, d, Field::BoostA);
	};
	return std::array<float, 3> {need(TownDesireInfo::ForFood), need(TownDesireInfo::ForWood),
	                             need(TownDesireInfo::ForRain)};
}

std::optional<audio::WorshipDesire> WorshipSites()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// CheckWorshipSiteDesiresSFX 0x71B270: the local player's citadel GPlayer+0xA48 (openblack's is PLAYER_ONE).
	// OPENBLACK_TEST_WORSHIP_PLAYER=<n> (test hook, not original; milagros2's worship hooks): that player's instead
	auto player = PlayerNames::PLAYER_ONE;
	if (const char* test = std::getenv("OPENBLACK_TEST_WORSHIP_PLAYER"); test != nullptr)
	{
		const int n = std::atoi(test);
		if (n >= 0 && n < static_cast<int>(PlayerNames::_COUNT))
		{
			player = static_cast<PlayerNames>(n);
		}
	}
	const auto citadel = worship::citadel::Of(player);
	const auto* citadelTransform = citadel != entt::null ? registry.TryGet<const Transform>(citadel) : nullptr;
	if (citadelTransform == nullptr)
	{
		return std::nullopt; // 0x71B2AB
	}
	audio::WorshipDesire desire;
	desire.citadelPosition = citadelTransform->position; // GetCitadel (vt +0x114) +0x14
	desire.need = worship::citadel::StrainSoundFraction(citadel); // +0x70, capped by the caller (0x71B319)
	// Citadel +0x34..+0x48 in slot order: the site's +0x14, fn_0077B960 > 0 and CalculateDesireForFood (vt +0x420)
	const auto sites = worship::citadel::WorshipSitesOf(player);
	for (size_t i = 0; i < sites.size(); ++i)
	{
		const auto site = sites.at(i);
		const auto* transform = site != entt::null ? registry.TryGet<const Transform>(site) : nullptr;
		if (transform == nullptr)
		{
			continue;
		}
		audio::WorshipDesire::Site entry;
		entry.id = static_cast<audio::ThingId>(entt::to_integral(site));
		entry.position = transform->position;
		entry.worshippers = worship::site::DancerCount(site) > 0;
		entry.foodDesire = worship::site::CalculateDesireForFood(site);
		desire.sites.at(i) = entry;
	}
	return desire;
}

/// GGuidance::ProcessHeartBeatSFX 0x71C190's values and fn_0071C460's heart, for the local player (GInterfaceStatus
/// +0xCC's GetPlayer: openblack's PLAYER_ONE)
audio::HeartBeatInput HeartBeat()
{
	audio::HeartBeatInput input;
	if (!Locator::entitiesRegistry::has_value())
	{
		return input;
	}
	constexpr auto k_Player = PlayerNames::PLAYER_ONE;
	// 0x71C1B6..0x71C1FF: the town list (+0xA50, next +0x75C): Town::GetRawDesire(3) 0x73E420 + the sum (fadd, fstp)
	for (const auto town : ecs::map_cells::TownsOf(k_Player))
	{
		input.protectionDesire =
		    ecs::town_desire::GetRawDesire(town, TownDesireInfo::ForProtection) + input.protectionDesire;
	}
	// 0x71C20E GetProportionOfWorldPopulationWhoBelieveInMe 0x64B680, 0x71C224 fn_0064B700 (the influence power +0x8C of
	// this turn's GPlayer::Process)
	input.believers = magic::players::ProportionOfWorldPopulationWhoBelieveInMe(k_Player);
	input.beliefShare = influence::InfluencePowerRatio(k_Player);
	// 0x71C2A6..0x71C379: the other players' creatures (GPlayer +0xA4C) near the local player's towns. Pendiente:
	// criatura (openblack's creatures are not the players' GPlayer +0xA4C yet): none, so they add nothing
	// 0x71C566..0x71C5A6: GPlayer +0xA48 with a built, living heart; PlaySample at the citadel's +0x14
	const auto citadel = worship::citadel::Of(k_Player);
	if (worship::citadel::HasLivingHeart(citadel))
	{
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(citadel);
		    transform != nullptr)
		{
			input.citadelHeart = transform->position;
		}
	}
	return input;
}

/// GAudio::ProcessChantMusic 0x427790's game side: the citadel within 150 of the camera's MapCoords (GetNearestCitadel
/// 0x602200, 0x4277A9: 0x43160000), its nearest site with dancers within 100 (fn_004639A0, 0x4277CF: 0x42C80000) and
/// that site's dance (+0xA0; (inferido) every openblack site has its dance, made with it)
std::optional<audio::ChantSite> ChantSite()
{
	const auto camera = CameraMapCoords();
	if (!camera || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto citadel = ecs::map_cells::GetNearestCitadel(*camera, 150.0f);
	if (citadel == entt::null)
	{
		return std::nullopt; // 0x4277C3
	}
	const auto site = worship::citadel::FindNearestWorshipSite(citadel, *camera, 100.0f);
	if (site == entt::null)
	{
		return std::nullopt; // 0x4277E8
	}
	const auto* component = Locator::entitiesRegistry::value().TryGet<const WorshipSite>(site);
	if (component == nullptr)
	{
		return std::nullopt;
	}
	audio::ChantSite chant;
	chant.tribe = static_cast<int>(component->tribe);                              // fn_0077C2E0
	chant.dancers = static_cast<uint32_t>(worship::site::DancerCount(site)); // Dance +0x90 (0x42786D)
	// fn_0077CD90 (0x427841 / 0x42789B): the dance centre's MapCoords (zero without the mesh's point, as fn_004639A0)
	ecs::map_coords::MapCoords centre {};
	if (const auto point = worship::site::GetSpecialPos(site, worship::site::Point::DanceCentre); point)
	{
		centre = ecs::map_coords::FromWorld(*point);
	}
	chant.position = ecs::map_coords::ToWorld(centre); // 0x4278A0..0x4278D5: GetAltitude + altitude; x, z x 10 / 65536
	// LH3DIsland::GetAltitude 0x803090 of the centre (0x42784A) and of the camera's MapCoords (0x427821)
	chant.ground = ecs::map_coords::ToWorld(ecs::map_coords::MapCoords {centre.x, centre.z, 0.0f}).y;
	chant.cameraGround = ecs::map_coords::ToWorld(ecs::map_coords::MapCoords {camera->x, camera->z, 0.0f}).y;
	return chant;
}

void RunViewHook(uint32_t turn)
{
	const char* view = std::getenv("OPENBLACK_AUDIO_TEST_VIEW");
	if (view == nullptr || !Locator::entitiesRegistry::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	unsigned at = 0;
	int wanted = 0;
	float distance = 4.0f;
	if (std::sscanf(view, "%u,%d,%f", &at, &wanted, &distance) < 2 || turn != at)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	int clip = -1;
	if (const char* anim = std::getenv("OPENBLACK_AUDIO_TEST_ANIM"); anim != nullptr)
	{
		clip = std::atoi(anim);
	}
	int index = 0;
	registry.Each<const Villager, const Transform>([&](entt::entity entity, const Villager&, const Transform& t) {
		if (clip >= 0)
		{
			auto& animation = registry.AssignOrReplace<SkeletalAnimation>(entity);
			animation.clip = ecs::ClipId(static_cast<uint32_t>(clip));
			animation.clipIndex = clip;
			animation.locked = true;
			animation.hasClip = true;
			animation.time = 0.0f;
			animation.speed = 1.0f;
		}
		if (index++ != wanted)
		{
			return;
		}
		const glm::vec3 focus = t.position + glm::vec3(0.0f, 0.8f, 0.0f);
		Locator::camera::value().GetModel().SetFlight(focus + glm::vec3(0.0f, distance * 0.35f, distance), focus);
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: turn {}, camera on villager {} at ({:.1f}, {:.1f}, {:.1f}), clip {}",
		                   turn, wanted, t.position.x, t.position.y, t.position.z, clip);
	});
}

void RunLanternHook()
{
	static uint32_t s_HookTurn = 0;
	++s_HookTurn;
	const char* hook = std::getenv("OPENBLACK_AUDIO_TEST_LANTERN");
	if (hook == nullptr || !Locator::camera::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	unsigned at = 0;
	float distance = 3.0f;
	if (std::sscanf(hook, "%u,%f", &at, &distance) < 1 || s_HookTurn != at)
	{
		return;
	}
	const auto lanterns = StreetLanterns();
	if (lanterns.empty())
	{
		return;
	}
	const auto& lantern = lanterns.front();
	const glm::vec3 top = lantern.position + glm::vec3(0.0f, lantern.height, 0.0f);
	Locator::camera::value().GetModel().SetFlight(top + glm::vec3(0.0f, distance * 0.35f, distance), top);
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: camera on lantern {} top ({:.1f}, {:.1f}, {:.1f})",
	                   static_cast<uint32_t>(lantern.thing), top.x, top.y, top.z);
}
/// (openblack test hook, audio session) OPENBLACK_AUDIO_TEST_CITADEL="<in>[,<out>]": at those hook turns the temple
/// interior is entered / left, as ENTER_EXIT_CITADEL(1) / (0) would (C4: the citadel's music and filters)
void RunCitadelHook()
{
	static uint32_t s_HookTurn = 0;
	++s_HookTurn;
	const char* hook = std::getenv("OPENBLACK_AUDIO_TEST_CITADEL");
	if (hook == nullptr || !Locator::temple::has_value())
	{
		return;
	}
	unsigned in = 0;
	unsigned out = 0;
	if (std::sscanf(hook, "%u,%u", &in, &out) < 1)
	{
		return;
	}
	auto& temple = Locator::temple::value();
	if (s_HookTurn == in && !temple.Active())
	{
		temple.Activate();
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: inside the citadel");
	}
	else if (out != 0 && s_HookTurn == out && temple.Active())
	{
		temple.Deactivate();
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: out of the citadel");
	}
}
} // namespace

float ecs::audio_queries::GAudioAlignment(float x)
{
	// fn_005E2240 0x5E2240..0x5E2291, in float steps (the game's x87 at 24 bits, fn_007DEE00): fcom [0x8AA398] (0),
	// test ah, 1 (C0: below or unordered) -> 0; else fcom [0x8AA390] (1), test ah, 0x41 (C0 | C3: below or equal) keeps
	// it, else 1; s = (1 - x) + (1 - x) (fsubr, fadd st0, st0; to [0xBF337C], the sky); GAudio+0x190 = 2 (0x8AB478) - s - 1
	if (!(x >= 0.0f))
	{
		x = 0.0f;
	}
	else if (x > 1.0f)
	{
		x = 1.0f;
	}
	const float s = (1.0f - x) + (1.0f - x);
	const float twoMinusS = 2.0f - s;
	return twoMinusS - 1.0f;
}

void ecs::audio_queries::Fill(audio::GameQueries& queries)
{
	// GSoundMap::GetSurfaceType 0x71D8E0: agua's ecs::sea_cells, the single source
	queries.surfaceType = [](glm::vec3 point) { return ecs::sea_cells::GetSurfaceType(point); };
	queries.weatherSmooth = &WeatherSmooth;
	// g_game+0x250188 != 0, the LHVideoPlayer of a full screen film (written at 0x54AC23, cleared by DeleteVideo
	// 0x54A969 and ClearVariables 0x54BF28; tmp_dis\audio\video_audio.md): asistente's player, atomic
	queries.videoPlaying = &video::IsPlaying;
	// GAudio+0x190, written by fn_005E2240 (ProcessAtmosBanks' group 0x428FFA, the alignment music fn_00427460)
	queries.cameraAlignment = &CameraAlignment;
	// GPlayer::GetAlignmentValue 0x64D6A0 of the local player (ProcessCitadelMusic 0x427BB8): openblack's is PLAYER_ONE
	queries.localPlayerAlignment = []() { return ecs::effects::alignment::Get(PlayerNames::PLAYER_ONE); };
	queries.animatedThing = &AnimatedThing;
	queries.animationClipName = &AnimationClipName;
	queries.streetLanterns = &StreetLanterns;
	// fn_00427460's towns (the tribe's music, A9): fn_00602160 0x602160 through ecs::map_cells (milagros2), the tribe
	// Town +0x5B8 and the distance GUtils::GetDistanceInMetres 0x74CD70 to the camera's MapCoords
	queries.nearestTown = &NearestMusicTown;
	queries.town = &KeptMusicTown;
	// GGuidance::ResourceDropSFX 0x71B570: MapCoords::GetNearestTown 0x6020E0 (map_cells::GetNearestTown) and its three
	// values (TownDesire +0x90 / +0xD4 / +0x168 of asistente's ecs::town_desire)
	queries.nearestTownAt = &NearestTownAt;
	queries.townResourceNeeds = &TownResourceNeeds;
	// CheckTownDesiresSFX 0x71B130: every town's sorted raw desires (ecs::town_desire); CheckWorshipSiteDesiresSFX
	// 0x71B270: the citadel's sites (milagros2's worship::citadel / worship::site)
	queries.desireTowns = &DesireTowns;
	queries.worshipSites = &WorshipSites;
	// ProcessHeartBeatSFX 0x71C190: the protection desires, 0x64B680, fn_0064B700 (influence power) and the heart
	queries.heartBeat = &HeartBeat;
	// ProcessChantMusic 0x427790: the worship site near the camera (GameMusic plays its chant)
	queries.chantSite = &ChantSite;
}

void ecs::audio_queries::RunTestHooks(uint32_t turn)
{
	RunViewHook(turn);
	RunLanternHook();
	RunCitadelHook();
}
