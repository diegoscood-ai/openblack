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

#include <functional>
#include <optional>

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

struct GameQueries
{
	/// g_game+0x250188 != 0 (a full screen video: ProcessMusic 0x427DF8, ProcessAudioGameTurn 0x4270B1) (inferred:
	/// nobody has read who writes it). Unset: false (openblack plays no video).
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
	/// GAudio+0x190: the alignment (-1..1) of the player with the most influence where the camera is (written by
	/// fn_005E2240 from fn_0064AC30, tmp_dis\agua\re\NOTES.md). Unset: 0, neutral (GAudio::Reset 0x426CC2 sets 0).
	std::function<float()> cameraAlignment;
	/// fn_00602160(camera, maxDistance): the nearest town of every player and the neutral one (GetNextPlayerAndNeutral
	/// 0x550980, towns from player +0xA50 by +0x75C) closer than maxDistance (strictly, 0x60219C; the distance is
	/// fn_00605CD0 = GetDistanceInMetres 0x74CD70, the same as MusicTown::distance) that has +0x9A4 set or
	/// fn_00741020 (a town centre among its buildings +0x754, or an entry of +0x9A8 whose GetComputerSeen is 0xC).
	/// Unset: nullopt (no tribe music until the towns have tribes).
	std::function<std::optional<MusicTown>(float maxDistance)> nearestTown;
	/// The town GAudio+0x18C keeps, again: nullopt when it is no longer available (IsAvailable, 0x4274AF). Unset:
	/// nullopt.
	std::function<std::optional<MusicTown>(uint32_t id)> town;
	/// The world position of a script object, nullopt when it is gone (GameThing::IsAvailable == 0, vtable +0x2C). The
	/// original builds it from the thing's MapCoords: (x, GetAltitude + altitude above the land, z) (0x42943F..0x429470).
	/// Unset: nullopt.
	std::function<std::optional<glm::vec3>(ThingId thing)> thingPosition;

	/// The branches of ProcessMusic 0x427DF0 that need systems openblack does not have yet. Each one is "the original
	/// function returned non-zero" (it took the music); unset = false, so ProcessMusic goes on to the next one.
	/// ProcessCitadelMusic 0x427B60 (inside the citadel, 0x4282F0; milestone C4)
	std::function<bool()> citadelMusic;
	/// fn_00427660 (the local creature fighting; milestone C1)
	std::function<bool()> creatureFightMusic;
	/// ProcessChantMusic 0x427790 (a worship site's dance near the camera; milestone C3)
	std::function<bool()> chantMusic;
	/// ProcessCreatureDanceMusic 0x427EC0 (a creature leading a dance; milestone C1)
	std::function<bool()> creatureDanceMusic;

	/// g_game+0x205A28 == 1 (0x4282F0, misnamed HelpSystem::GetWideScreenControl; GoInsideCitadel 0x554004 sets 1,
	/// LeaveInsideCitadel 0x553B1F sets 0): inside the citadel GAudio::PlaySoundEffect plays only the samples of user
	/// parameter 2 (0x429F6D) and measures the 3D cull from LH3DTech::g_camera (0x429EB1). Unset: false (openblack has no
	/// citadel interior yet).
	std::function<bool()> insideCitadel;
	/// GInterface+0x44 (GGame::MyInterface 0x555850): in the states 0x10, 0x16 and 0x17 the samples of user parameter 4
	/// do not play (0x429FA5..0x429FB8). Unset: 0, none of them (openblack has no GInterface states).
	std::function<int()> interfaceState;
};

} // namespace openblack::audio
