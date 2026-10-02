/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

// What GAudio reads from the rest of the game (dev\tmp_dis\audio\PLAN.md §2.4). The audio does not include the ECS:
// the game registers these functions (Game.cpp) and every query left unset gives the neutral value written next to it,
// which is the value of a game without that system (marked (inferred) where the original has no such state).
// The music part (milestones A5, A7, A9) and the filters of the sample channels (B1).

namespace openblack::audio
{

/// A script object as the audio sees it: the CHL object id (the entity's value in openblack)
using ThingId = uint32_t;

/// The camera as GAudio reads it
struct CameraState
{
	/// LH3DTech::g_camera 0xEA1DB8 (the render camera's position), used for the 3D distances of ProcessThingMusic
	/// (fn_00429420 0x429479..0x4294C1) (inferred: GGame::GetCamera()+0x14, the MapCoords of ProcessAlignmentMusic, is
	/// the same point)
	glm::vec3 position {0.0f};
	/// GGame::GetCamera()+0x14 MapCoords +8: the height above the land (MapCoords::Set 0x603340 stores y - GetAltitude,
	/// 0x603371..0x60337C), read by fn_00427460 at 0x427498
	float heightAboveGround {0.0f};
};

/// A town as the alignment music sees it (fn_00427460)
struct MusicTown
{
	uint32_t id {0}; ///< what GAudio+0x18C keeps (the Town pointer in the original)
	int tribe {0};   ///< Town +0x5B8 (0x42753D / 0x42755A), the index of the tribe table 0x9C9A0C
	/// GUtils::GetDistanceInMetres 0x74CD70 between the camera's MapCoords and the town's (+0x14): the x/z distance
	/// (GetDistance 0x74CCB0 is hypotenuse(dx, dz)) times 10 / 65536 (ConvertWholeDistanceToMeters 0x74DCC0)
	float distance {0.0f};
};

/// A town as GGuidance's CheckTownDesiresSFX 0x71B130 reads it
struct DesireTown
{
	ThingId id {0};            ///< the Town (the key of GGuidance's LastThings, fn_0071AE10)
	glm::vec3 position {0.0f}; ///< Town +0x14: where its desire is said
	/// GetStoragePit 0x73B5B0 +0x14: the distance to the camera is measured from it; nullopt: no storage pit
	std::optional<glm::vec3> storagePit;
	uint32_t population {0}; ///< +0x618 + +0x61C (the sum the original tests)
	struct Desire
	{
		float value {0.0f}; ///< +0x37C + 12 k
		uint32_t type {0};  ///< +0x380 + 12 k, TOWN_DESIRE_INFO
		float raw {0.0f};   ///< Town::GetRawDesire(type) 0x73E420
	};
	std::array<Desire, 17> desires {};
};

/// The local player's citadel as CheckWorshipSiteDesiresSFX 0x71B270 reads it
struct WorshipDesire
{
	struct Site
	{
		ThingId id {0};
		glm::vec3 position {0.0f}; ///< +0x14
		bool worshippers {false};  ///< fn_0077B960 (-> 0x77CFB0) > 0
		float foodDesire {0.0f};   ///< CalculateDesireForFood (vt +0x420)
	};
	std::array<std::optional<Site>, 6> sites {}; ///< Citadel +0x34..+0x48
	glm::vec3 citadelPosition {0.0f};            ///< GetCitadel (vt +0x114) +0x14, where it is said
	float need {0.0f};                           ///< Citadel +0x70 (capped at 1 by the caller)
};

/// What GGuidance::ProcessHeartBeatSFX 0x71C190 reads of the local player
struct HeartBeatInput
{
	float protectionDesire {0.0f}; ///< the sum of Town::GetRawDesire(3) over the player's towns (+0xA50, +0x75C)
	float believers {0.0f};        ///< GPlayer::GetProportionOfWorldPopulationWhoBelieveInMe 0x64B680
	float beliefShare {0.0f};      ///< fn_0064B700: GPlayer+0x8C over the sum of the active players' (0 for 0)
	/// For each other player's creature (GPlayer+0xA4C, whose interface is not the local one) whose nearest town
	/// (MapCoords::GetNearestTown 0x601F90) is the local player's: that distance
	std::vector<uint32_t> enemyCreatureDistances;
	/// The local citadel (GPlayer+0xA48) +0x14 when its +0x30 (the heart) answers vt +0x890 and has life (GetLife > 0)
	std::optional<glm::vec3> citadelHeart;
};

/// What fn_00516510 (the sound events of an animation clip, audio::AnimationSounds::Fire) reads of the animated thing
struct AnimatedThing
{
	glm::vec3 position {0.0f}; ///< this->Get3DSoundPos (0x516548: GameThingWithPos, its position)
	/// Living::TurnsSinceStateChange (P_THROWN 0x5166B1 / P_THROWN_VORTEX 0x5166F8); 0 for a thing without one
	uint16_t turnsSinceStateChange {0};
	/// The villager's part (IsVillager): nullopt for an animal or any other thing
	struct Villager
	{
		bool alive {true};  ///< IsAlive (vtable +0x5B4, 0x5165BC): Object +0x48 life > 0
		bool child {false}; ///< the voice 3 of the clip's group 1 (a child)
		bool female {false}; ///< the voice 2 (else 1, a man)
		/// Villager::GetAbode (0x51675D, the banter 0x92 at the house): nullopt for none or a gone one
		std::optional<entt::entity> abode;
	};
	std::optional<Villager> villager;
};

/// A street lantern as the lanterns' SoundTags read it (GStreetLantern, the list g_game+0x205C34)
struct StreetLantern
{
	entt::entity thing {};
	glm::vec3 position {0.0f};
	float height {0.0f}; ///< Object::GetHeight 0x638120 (the tag's offset (0, height, 0), 0x734810)
};

/// GCamera+0x80 (WeatherInfo, filled by GCamera::Update with LH3DAtmos::GetWeatherSmooth): the weather at the camera
struct CameraWeatherInfo
{
	int8_t temperature {0};
	int8_t rain {0};
	int8_t snow {0};
	int8_t overcast {0};
	int8_t windX {0};
	int8_t windZ {0};
};

struct GameQueries
{
	/// g_game+0x250188 != 0 (a full screen video: ProcessMusic 0x427DF8, ProcessAudioGameTurn 0x4270B1): the
	/// LHVideoPlayer, written by fn_0054AB20 (0x54AC23), cleared by DeleteVideo 0x54A969 and ClearVariables 0x54BF28;
	/// only the intro and fall films set it, not the tips or the pre-intro (tmp_dis\audio\video_audio.md). Filled with
	/// video::IsPlaying (ECS/AudioQueries.cpp). Unset: false.
	std::function<bool()> videoPlaying;
	/// g_game+0x205A08, SET_LAND_NUMBER (0x7177A4); ProcessMusic plays nothing on land 6 (0x427E1D). Unset: 0.
	std::function<int()> landNumber;
	/// g_game+0x205A40, the game turn (ProcessAlignmentMusic needs > 20, 0x427A2E; GGame::EndTurn calls
	/// ProcessAudioGameTurn only after turn 5, 0x54E997). Unset: 0.
	std::function<uint32_t()> turn;
	/// GGame::GetCamera() (0x4279D7), nullopt when there is none. Unset: nullopt.
	std::function<std::optional<CameraState>()> camera;
	/// HelpSystem +0x45E8 && +0x45EC: the script's wide screen is on (0x4279E9..0x427A01). Unset: false (openblack has
	/// no HelpSystem yet).
	std::function<bool()> scriptWideScreen;
	/// fn_005C6C50: the wide screen bars are moving (0x427A07). Unset: false.
	std::function<bool()> wideScreenChanging;
	/// GAudio+0x190: the alignment (-1..1) of the player with the most influence where the camera is, as fn_005E2240
	/// writes it once a turn (GPlayer::ProcessPlayers 0x64A697 -> fn_0064AC30: x = clamp((alignment + 1) / 2, 0, 1),
	/// +0x190 = 2 - 2 (1 - x) - 1). Read by ProcessAtmosBanks (group 1 above -0.6, else 2, 0x428FFA) and the alignment
	/// music (fn_00427460 0x427466). Game: ecs::audio_queries (ecs::effects::alignment::GetInterfaceAlignment). Unset: 0,
	/// neutral (GAudio::Reset 0x426CC2 sets 0).
	std::function<float()> cameraAlignment;
	/// fn_00602160(camera, maxDistance): the nearest town of every player and the neutral one (GetNextPlayerAndNeutral
	/// 0x550980, towns from player +0xA50 by +0x75C) closer than maxDistance (strictly, 0x60219C; the distance is
	/// fn_00605CD0 = GetDistanceInMetres 0x74CD70, the same as MusicTown::distance) that has +0x9A4 set or
	/// fn_00741020 (a town centre among its buildings +0x754, or an entry of +0x9A8 whose GetComputerSeen is 0xC).
	/// Unset: nullopt (no tribe music until the towns have tribes). Pending (C2): openblack's towns get their tribe and
	/// the +0x9A4 / fn_00741020 test from ecs::map_cells (the session milagros2); until then the music is the generic one
	/// of the alignment (fn_00427460 0x427579).
	std::function<std::optional<MusicTown>(float maxDistance)> nearestTown;
	/// The town GAudio+0x18C keeps, again: nullopt when it is no longer available (IsAvailable, 0x4274AF). Unset:
	/// nullopt.
	std::function<std::optional<MusicTown>(uint32_t id)> town;
	/// The world position of a script object, nullopt when it is gone (GameThing::IsAvailable == 0, vtable +0x2C). The
	/// original builds it from the thing's MapCoords: (x, GetAltitude + altitude above the land, z) (0x42943F..0x429470).
	/// Unset: nullopt.
	std::function<std::optional<glm::vec3>(ThingId thing)> thingPosition;
	/// LH3DIsland::GetAltitude 0x803090 at a world x / z (SoundTag::Create(MapCoords&) 0x71EB71: the land under a
	/// MapCoords). Unset: 0.
	std::function<float(float x, float z)> landAltitude;
	/// GSoundMap::GetSurfaceType 0x71D8E0 at a world point (the "surface" attribute of the anim effect keys: the clips'
	/// events 0x51662B, the PSys sounds with USESURFACE 0x674661, the dump of GSoundMap): ecs::sea_cells::GetSurfaceType,
	/// the one reading of the map's cells. Unset: 6 (off the map, 0x71D950).
	std::function<int32_t(glm::vec3 point)> surfaceType;
	/// LH3DAtmos::GetWeatherSmooth 0x835180 (recalc) at a point: GCamera::Update fills GCamera+0x80 with it at the camera
	/// (GSoundMap's weather: the plan's weatherAt(camera); audio::CameraWeather asks it at the camera). Game:
	/// ecs::audio_queries (weather::atmos::GetWeatherSmooth, the storms and climates of src/ECS/Weather). Unset: all 0 (no
	/// rain, snow nor wind).
	std::function<CameraWeatherInfo(glm::vec3 point)> weatherSmooth;
	/// The animated thing of fn_00516510, nullopt when it is gone or has no position (nothing plays). Unset: nullopt.
	std::function<std::optional<AnimatedThing>(entt::entity thing)> animatedThing;
	/// The name of the animation clip ANM_ `index` (LoadAllAnimations 0x550180 matches Data\SmallSounds.SAS by it), nullopt
	/// when there is no such clip. Unset: nullopt (no clip has sounds).
	std::function<std::optional<std::string>(int32_t index)> animationClipName;
	/// Every street lantern (the list g_game+0x205C34), in the registry's order. Unset: none.
	std::function<std::vector<StreetLantern>()> streetLanterns;

	/// The branches of ProcessMusic 0x427DF0 that need systems openblack does not have yet. Each one is "the original
	/// function returned non-zero" (it took the music); unset = false, so ProcessMusic goes on to the next one.
	/// fn_00427660 (the local creature fighting; milestone C1)
	std::function<bool()> creatureFightMusic;
	/// ProcessChantMusic 0x427790 (a worship site's dance near the camera; milestone C3)
	std::function<bool()> chantMusic;
	/// ProcessCreatureDanceMusic 0x427EC0 (a creature leading a dance; milestone C1)
	std::function<bool()> creatureDanceMusic;

	/// g_game+0x205A28 == 1 (0x4282F0, misnamed HelpSystem::GetWideScreenControl; GoInsideCitadel 0x554004 sets 1,
	/// LeaveInsideCitadel 0x553B1F sets 0): inside the citadel GAudio::PlaySoundEffect plays only the samples of user
	/// parameter 2 (0x429F6D, SamplePlayAnimEffect 0x42A554), measures the 3D cull from LH3DTech::g_camera (0x429EB1) and
	/// ProcessMusic plays the citadel's music (ProcessCitadelMusic 0x427B60). Game: openblack's temple interior
	/// (Locator::temple, TempleInteriorInterface::Active: ENTER_EXIT_CITADEL and the debug window). Unset: false.
	std::function<bool()> insideCitadel;
	/// GPlayer::GetAlignmentValue 0x64D6A0 of the local player (g_game+0x205A59, ProcessCitadelMusic 0x427B9A..0x427BB8),
	/// -1..1. Game: ecs::audio_queries (ecs::effects::alignment::Get of PLAYER_ONE). Unset: 0 (neutral, a new game's,
	/// GGame::Init 0x54FEA0 without a profile).
	std::function<float()> localPlayerAlignment;
	/// GInterface+0x44 (GGame::MyInterface 0x555850): in the states 0x10, 0x16 and 0x17 the samples of user parameter 4
	/// do not play (0x429FA5..0x429FB8). Unset: 0, none of them (openblack has no GInterface states).
	std::function<int()> interfaceState;

	// ---- GGuidance and GSpookyVoices (milestones B9 / B10, Guidance.h, SpookyVoices.h) ----

	/// g_game+0x205A0C: a playground game (GGame::Init 0x54F75E sets it after ResetAndStartPlaygroundGame). Unset: false.
	std::function<bool()> playgroundGame;
	/// GGame::IsMultiplayerGame 0x552F80. Unset: false (openblack has no multiplayer).
	std::function<bool()> multiplayerGame;
	/// HelpSystem+0x45F8 ? +0x45F4 : 0 (GGuidance::PlayNow 0x71AF99..0x71AFB1): the help switch (SET_HELP_SYSTEM
	/// 0x6FC03D; HelpSystem::Reset 0x5C55FC sets 1) and the profile's HELP_LEVEL (fn_005C6CF0, 3 when the profile has none:
	/// 0x5C6DB6). Unset: 3.
	std::function<int()> helpLevel;
	/// GPlayer::GetPlayerNumber 0x64A790 (+0xB5) of the local interface's player: the owner of GGuidance's samples. Unset: 0.
	std::function<uint32_t()> localPlayerNumber;
	/// GGameInfo::IsVisualNight 0x5575E0. Unset: false.
	std::function<bool()> visualNight;
	/// GInterface+0x3B8 (the hand's MapCoords, inferred) as a world point. Unset: nullopt (no remark that needs it).
	std::function<std::optional<glm::vec3>()> handPosition;
	/// fn_0081F1D0: the point is inside the camera's view. Unset: false.
	std::function<bool(glm::vec3 point)> pointOnScreen;
	/// Every player's towns for CheckTownDesiresSFX (GetNextPlayer order). Unset: none (openblack's towns have no
	/// desires yet: the session mapa's V3).
	std::function<std::vector<DesireTown>()> desireTowns;
	/// The local player's citadel for CheckWorshipSiteDesiresSFX. Unset: nullopt (no citadel desires).
	std::function<std::optional<WorshipDesire>()> worshipSites;
	/// MapCoords::GetNearestTown 0x6020E0(maxDistance) (strictly nearer, every player and the neutral one) and that town's
	/// three values of a RESOURCE_RAIN_TYPE (GetResourceDropSample 0x71B5F0: food +0x19C + +0x108 + +0xC4, wood +0x1A0 +
	/// +0x10C + +0xC8, rain +0x1C4 + +0x130 + +0xEC), each summed, indexed food, wood, rain. Unset: nullopt.
	std::function<std::optional<std::array<float, 3>>(glm::vec3 point, float maxDistance)> townResourceNeeds;
	/// ProcessHeartBeatSFX's input. Unset: all 0 and no citadel heart (the beat is computed, nothing plays).
	std::function<HeartBeatInput()> heartBeat;
	/// HelpSystem::RunMessage 0x5C8CE0(first, last, script): nothing for first > last; StopRunningScripts (0x5C8C40:
	/// false while a task that is not a help one has the dialogue), +0x560 = turn, the two numbers pushed as floats and
	/// GScript::StartScript 0x6EB710 (types 0x7F in a single-player game). Unset: nothing.
	std::function<bool(uint32_t first, uint32_t last, std::string_view script)> helpRunMessage;
	/// HelpSystem::TriggerCategory 0x5C8280: +0x2D8 + 4 category = turn. Unset: nothing.
	std::function<void(int category)> helpTriggerCategory;
	/// GSpookyVoices::GetName 0x72E740's first name: the current profile's (PlayerProfile +0x200, [0xD4BF38]).
	/// (inferred) openblack has no profiles: the name of OPENBLACK_PLAYER_NAME, if set. Unset: empty.
	std::function<std::u16string()> profileName;
};

} // namespace openblack::audio
