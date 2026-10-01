/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PetitNavire.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>

#include <entt/core/hashed_string.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/SamplePlay.h"
#include "ECS/Animations.h"
#include "ECS/Components/DynamicShadow.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SmokyStuff.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::petit_navire
{
namespace
{
using namespace components;

// crt_xc_fn_JCMisc_005DFED0 / 005DFF00
constexpr glm::vec3 k_Dock {1881.0833f, 8.1316004f, 3154.1094f};   // [0xD19A08]: the dry dock
constexpr glm::vec3 k_SeaStart {1456.54f, 0.0f, 3263.06f};         // [0xD199F8]: where the crossing starts
constexpr float k_HalfPi = 1.5707964f;                             // 0x8C7B48 / 0x3FC90FDB
constexpr float k_QuarterPi = 0.78539819f;                         // 0x92B210
// 0x5E0224: InverseSquareRoot(2) (table 0xEEA394 + one Newton step) x -1, the speed's direction
constexpr float k_InvSqrt2 = 0.70710659f;
constexpr float k_Speed = 0.005f;                                  // 0x8CF1B8, units per ms
constexpr int32_t k_CrossingMs = 60000;                            // 0x5E01D8
constexpr int32_t k_HoldMs = 3000;                                 // 0x5DFF50
constexpr int32_t k_EndMargin = 400;                               // 0x5DFF6B: len(Boat1) - 400 turns it to mode 1
constexpr int32_t k_WakeCycle = 6000;                              // 0x5E0790
constexpr int32_t k_PuffMs = 200;                                  // 0x5E04CE
constexpr int32_t k_PushedMs = 850;                                // 0x5E062E
// 0xBF2B10..: the ScriptSFX samples at these clip times, 2D (LH_SamplePlayOptions +0xBC = 2)
constexpr std::array<int32_t, 3> k_SoundTimes = {100, 1500, 3900};
constexpr std::array<const char*, 3> k_Sounds = {"Scriptsfx.sad/62", "Scriptsfx.sad/61", "Scriptsfx.sad/60"};
// 0xBF2B1C, 0xBF2B30, 0xBF2B44: the five sailors on the dock
constexpr std::array<float, 5> k_SailorX = {5.2f, 5.3f, 5.5f, 5.0f, 5.0f};
constexpr std::array<int32_t, 5> k_SailorPhase = {5, 500, 1500, 455, 2000};
constexpr std::array<float, 5> k_SailorZ = {1.0f, -0.7f, 0.0f, 0.4f, -0.2f};

// the PetitNavire's animations (+0x08..+0x20, LH3DAnim::AnimPack) and meshes (LH3DMesh::MeshPack)
constexpr uint32_t k_PushObject = 346;       // +0x08 ANM_P_PUSH_OBJECT
constexpr std::array<uint32_t, 3> k_Pushed = {332, 235, 333}; // +0x0C OVERWORKED1, +0x10 CROWD_WON_2, +0x14 OVERWORKED2
constexpr uint32_t k_Titanic = 406;          // +0x18 ANM_P_TITANIC
constexpr uint32_t k_Sitting = 378;          // +0x1C ANM_P_SITTING_SWINGING_LEGS
constexpr uint32_t k_Scrubbs = 359;          // +0x20 ANM_P_SCRUBBS
constexpr uint32_t k_CowEat = 36;            // ANM_A_COW_EAT_2

/// One person, animal or thing drawn on deck in mode 1 (PostDraw 0x5E08E7..0x5E1015): the matrix L . hull (fn_007FAFF0),
/// L = RotY(angle) x scale + t in the hull's frame, the clip time (+0x34 + offset) % the clip's length
struct DeckPlace
{
	MeshId mesh;
	uint32_t clip; ///< 0: static
	float angle;
	float scale;
	glm::vec3 offset;
	int32_t phase;
};
const std::array<DeckPlace, 8> k_Deck = {{
    {MeshId::AnimalCow1, k_CowEat, -1.0f, 1.0f, {-1.778f, 10.78f, 1.83f}, 0},                      // 0x5E08E7 +0x40
    {MeshId::AnimalCow1, k_CowEat, -0.7f, 1.0f, {-1.778f, 10.78f, 3.83f}, 1255},                   // 0x5E09B0 +0x40
    {MeshId::SpellGrainPile, 0, 0.0f, 0.26f, {-5.708f, 10.854f, 3.199f}, 0},                       // 0x5E0A7E +0x44
    {MeshId::PersonNorseFemaleA1, k_Titanic, 0.0f, 1.0f, {-0.14f, 13.213f, -19.657f}, 0},          // 0x5E0B28 +0x3C
    {MeshId::PersonNorseSailor, k_Titanic, 0.0f, 1.0f, {-0.14f, 13.213f, -18.9f}, 500},            // 0x5E0C1B
    {MeshId::PersonNorseSailor, k_Sitting, 3.1415927f, 1.0f, {-6.25f, 11.424f, -7.227f}, 0},       // 0x5E0D03
    {MeshId::PersonNorseSailor, k_Sitting, 3.1415927f, 1.0f, {-5.25f, 11.424f, -7.227f}, 2345},    // 0x5E0E46
    {MeshId::PersonNorseSailor, k_Scrubbs, 3.1415927f, 1.0f, {5.881f, 10.741f, 0.174f}, 0},        // 0x5E0F4A
}};

struct Boat
{
	int32_t mode {0};         ///< +0x30
	int32_t animTime {0};     ///< +0x24: the hull's clip
	int32_t previousTime {0}; ///< +0x64: +0x24 before this frame (the sound thresholds)
	int32_t timer {0};        ///< +0x34
	int32_t puffTimer {0};    ///< +0x38
	bool hold {true};         ///< +0x48: the hull waits 3 s before sliding
	std::array<int32_t, 5> wakePhase {}; ///< +0x50
	glm::mat4 hull {1.0f};    ///< the hull's matrix (LH row matrix as a glm column one)
	bool reflected {false};   ///< PreDraw reached DrawUnderWater this frame
	entt::entity hullEntity {entt::null};
	std::vector<entt::entity> people; ///< the five sailors (mode 0) or the deck (mode 1)
};

std::optional<Boat> g_boat;
std::vector<WakeSprite> g_wake;
float g_carry = 0.0f; // the fraction of a millisecond not given yet (g_game_time_inc is whole)
// OPENBLACK_TEST_JC_SPECIAL's delay: the mode to make once that much game time has gone (-1: nothing pending)
int32_t g_pendingMode = -1;
int32_t g_pendingFrames = 0;
int32_t g_fastForwardMs = 0;
float g_traceClock = 0.0f;

/// Data\MISC\Boat1.anm / Boat2.anm (fn_00839900), one track matrix each
const L3DAnim* Clip(int32_t which)
{
	static std::array<std::unique_ptr<L3DAnim>, 2> s_clips;
	static std::array<bool, 2> s_tried {};
	const auto index = static_cast<size_t>(which);
	if (!s_tried.at(index))
	{
		s_tried.at(index) = true;
		auto clip = std::make_unique<L3DAnim>();
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / (which == 0 ? "boat1.anm" : "boat2.anm");
		if (clip->LoadFromFilesystem(path) && !clip->GetFrames().empty())
		{
			s_clips.at(index) = std::move(clip);
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "PetitNavire: cannot load {}", path.generic_string());
		}
	}
	return s_clips.at(index).get();
}

int32_t ClipLength(uint32_t clip)
{
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto id = ClipId(clip);
	return animations.Contains(id) ? std::max(animations.Handle(id)->GetDurationMs(), 1) : 1;
}

float Altitude(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// LH RotateY(angle) of a row matrix, as the glm matrix of the same transform
glm::mat4 RotY(float angle)
{
	return glm::eulerAngleY(-angle);
}

entt::entity MakeObject(MeshId mesh, uint32_t clip)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, k_Dock, glm::mat3(1.0f), glm::vec3(1.0f));
	const bool animated = clip != 0;
	registry.Assign<Mesh>(entity, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(animated ? 0 : 1));
	if (animated)
	{
		// the clip time is set by the boat every frame (vt+0x188), it does not run by itself
		auto& animation = registry.Assign<SkeletalAnimation>(entity);
		animation.clip = ClipId(clip);
		animation.clipIndex = static_cast<int32_t>(clip);
		animation.hasClip = true;
		animation.speed = 0.0f;
	}
	return entity;
}

void SetClip(entt::entity entity, uint32_t clip, int32_t time)
{
	auto& animation = Locator::entitiesRegistry::value().Get<SkeletalAnimation>(entity);
	animation.clip = ClipId(clip);
	animation.clipIndex = static_cast<int32_t>(clip);
	animation.time = static_cast<float>(time);
}

void Place(entt::entity entity, const glm::mat4& matrix, float scale = 1.0f)
{
	auto& transform = Locator::entitiesRegistry::value().Get<Transform>(entity);
	transform.position = glm::vec3(matrix[3]);
	transform.rotation = glm::mat3(matrix);
	transform.scale = glm::vec3(scale);
}

/// fn_005E13C0 + delete
void Free()
{
	if (!g_boat.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(g_boat->hullEntity))
	{
		registry.Destroy(g_boat->hullEntity);
	}
	for (const auto entity : g_boat->people)
	{
		if (registry.Valid(entity))
		{
			registry.Destroy(entity);
		}
	}
	g_boat.reset();
	g_wake.clear();
	registry.SetDirty();
}

void PlaySample(const char* name)
{
	const auto id = entt::hashed_string(name).value();
	if (!Locator::audio::has_value() || !Locator::resources::value().GetSounds().Contains(id))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "PetitNavire: no sample {}", name);
		return;
	}
	// 0x5E0413..0x5E04B9: a default LH_SamplePlayOptions, bank Scriptsfx (GAudio+0x3BC), is3D 0 (+0x08), owner 0
	// (+0x20), mode 2 (+0x50), GAudio::PlaySoundEffect 0x429E30: one of LHaudio's 16 channels
	audio::sample_play::Options options;
	options.sound = id;
	options.mode = 2;
	audio::sample_play::PlaySoundEffect(options);
}

/// PreDraw 0x5DFF20. False when the boat was freed (or turned into a new one, which gets no PreDraw this frame).
bool PreDraw(int32_t dt)
{
	auto& boat = *g_boat;
	boat.reflected = false;
	if (boat.mode == 0)
	{
		boat.timer += dt;
		boat.previousTime = boat.animTime;
		if (!boat.hold || boat.timer > k_HoldMs)
		{
			boat.animTime += dt;
		}
		const auto* clip = Clip(0);
		if (clip == nullptr || boat.animTime > clip->GetDurationMs() - k_EndMargin)
		{
			Create(1);
			return false;
		}
		// M = Translate(dock); fn_0083AC70 (the track . M), RotateY(pi / 2), fn_007FAE60(diag(-1, 1, 1)) in front
		std::vector<glm::mat4> track;
		clip->SampleLocal(boat.animTime, track);
		boat.hull = glm::translate(k_Dock) * track.at(0) * RotY(k_HalfPi) * glm::scale(glm::vec3(-1.0f, 1.0f, 1.0f));
		// 0x5E00EB..0x5E0154: y += altitude under the hull - altitude of the dock
		boat.hull[3].y += Altitude(boat.hull[3].x, boat.hull[3].z) - Altitude(k_Dock.x, k_Dock.z);
		Place(boat.hullEntity, boat.hull);
		boat.reflected = true; // 0xFF303070, DrawUnderWater; then fn_00801C90 gives it the land light back
		return true;
	}
	// mode 1: Boat2's time, wrapped when the clip loops (flag 0x100), else held on its last millisecond
	const auto* clip = Clip(1);
	if (clip != nullptr)
	{
		const int32_t length = std::max(clip->GetDurationMs(), 1);
		boat.animTime = clip->IsLooping() ? (boat.animTime + dt) % length : std::min(boat.animTime + dt, length - 1);
	}
	boat.timer += dt;
	if (boat.timer > k_CrossingMs)
	{
		Free();
		return false;
	}
	const float travelled = static_cast<float>(boat.timer) * k_Speed;
	const glm::vec3 position = k_SeaStart + glm::vec3(-k_InvSqrt2, 0.0f, -k_InvSqrt2) * travelled;
	glm::mat4 track(1.0f);
	if (clip != nullptr)
	{
		std::vector<glm::mat4> bones;
		clip->SampleLocal(boat.animTime, bones);
		track = bones.at(0);
	}
	boat.hull = glm::translate(position) * RotY(k_QuarterPi) * track * RotY(k_HalfPi) *
	            glm::scale(glm::vec3(-1.0f, 1.0f, 1.0f));
	Place(boat.hullEntity, boat.hull);
	boat.reflected = true;
	return true;
}

/// PostDraw 0x5E03F0
void PostDraw(int32_t dt)
{
	auto& boat = *g_boat;
	g_wake.clear();
	const glm::vec3 hullPosition(boat.hull[3]);
	if (boat.mode == 0)
	{
		for (size_t i = 0; i < k_SoundTimes.size(); ++i)
		{
			if (boat.previousTime < k_SoundTimes.at(i) && boat.animTime > k_SoundTimes.at(i))
			{
				PlaySample(k_Sounds.at(i));
			}
		}
		boat.puffTimer += dt;
		if (boat.puffTimer > k_PuffMs)
		{
			if (boat.animTime > 3900)
			{
				if (boat.animTime < 6500)
				{
					// 0x5E04F5: two sprays 0xFEFFFFFF of size 7 at hull + (Random(-2, 2) - 10, 7, Random(-20, 20))
					for (int k = 0; k < 2; ++k)
					{
						const float r1 = smoky_stuff::Random(-20.0f, 20.0f);
						const float r2 = smoky_stuff::Random(-2.0f, 2.0f);
						smoky_stuff::Create(hullPosition + glm::vec3(r2 - 10.0f, 7.0f, r1), 0, 7.0f, 0xFEFFFFFFu);
					}
				}
			}
			else if (boat.animTime > 1130)
			{
				// 0x5E0598: two sand-coloured dusts 0xFFB88C38 of size 5 at hull + (Random(-2, 2), 0, Random(-20, 20))
				for (int k = 0; k < 2; ++k)
				{
					const float r1 = smoky_stuff::Random(-20.0f, 20.0f);
					const float r2 = smoky_stuff::Random(-2.0f, 2.0f);
					smoky_stuff::Create(hullPosition + glm::vec3(r2, 0.0f, r1), 0, 5.0f, 0xFFB88C38u);
				}
			}
			boat.puffTimer = 0;
		}
		// the one sailor object drawn five times at the dock, turned -pi/2
		for (size_t i = 0; i < boat.people.size(); ++i)
		{
			const auto entity = boat.people[i];
			auto& animation = Locator::entitiesRegistry::value().Get<SkeletalAnimation>(entity);
			uint32_t clip = static_cast<uint32_t>(animation.clipIndex);
			if (boat.animTime > k_PushedMs)
			{
				if (boat.hold)
				{
					boat.timer = 0;
				}
				clip = k_Pushed.at(i % 3);
				boat.hold = false;
			}
			glm::vec3 at(k_Dock.x + k_SailorX.at(i), 0.0f,
			             (static_cast<float>(i) - 2.5f) * 3.0f + k_SailorZ.at(i) + k_Dock.z + 10.0f);
			at.y = Altitude(at.x, at.z);
			Place(entity, glm::translate(at) * RotY(-k_HalfPi));
			SetClip(entity, clip, (k_SailorPhase.at(i) + boat.timer) % ClipLength(clip));
		}
		return;
	}
	// mode 1: the wake, five flat sprites 6 s apart behind the hull (0x5E0785..0x5E08E1)
	for (auto& phase : boat.wakePhase)
	{
		phase = (phase + dt) % k_WakeCycle;
		const float t = static_cast<float>(phase) * 0.000166667f;
		glm::vec3 at = glm::vec3(boat.hull * glm::vec4(0.0f, 0.0f, t * 100.0f - 15.0f, 1.0f));
		at.y = 0.2f;
		// the fade starts at [0xD19CB8], a float nothing ever writes: 0
		const float f = t;
		const auto alpha = static_cast<uint32_t>(static_cast<int32_t>((1.0f - f) * 255.0f)) & 0xFFu;
		if (f > 0.2)
		{
			g_wake.push_back({at, t * 30.0f + 10.0f, 0.5f, 3.9269910f, 0x31, (alpha << 24) | 0x00FFFFFFu});
		}
	}
	// the deck: each thing's matrix L . hull, its clip at +0x34 + phase (the hull's colour: see the wiki)
	for (size_t i = 0; i < boat.people.size() && i < k_Deck.size(); ++i)
	{
		const auto& place = k_Deck.at(i);
		const glm::mat4 local = glm::translate(place.offset) * RotY(place.angle);
		Place(boat.people[i], boat.hull * local, place.scale);
		if (place.clip != 0)
		{
			SetClip(boat.people[i], place.clip, (boat.timer + place.phase) % ClipLength(place.clip));
		}
	}
}
} // namespace

void Create(int32_t mode)
{
	Free();
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	Boat boat;
	boat.mode = mode;
	// +0x28: LH3DObject(static) MSH_O_ARK at the dock, angle 0, scale 1
	boat.hullEntity = MakeObject(MeshId::ObjectArk, 0);
	boat.hull = glm::translate(k_Dock);
	Place(boat.hullEntity, boat.hull);
	auto& registry = Locator::entitiesRegistry::value();
	if (mode == 0)
	{
		// +0x2C: its dynamic shadow (fn_008745A0), which falls on objects too ([holder]+0xC = 0)
		registry.Assign<DynamicShadow>(boat.hullEntity, true);
		// +0x3C: the sailor with ANM_P_PUSH_OBJECT, drawn five times
		for (int i = 0; i < 5; ++i)
		{
			boat.people.push_back(MakeObject(MeshId::PersonNorseSailor, k_PushObject));
		}
	}
	else
	{
		// +0x40 the cow (ANM_A_COW_EAT_2) twice, +0x44 the grain pile, the sailor object five times; +0x4C the wake
		for (const auto& place : k_Deck)
		{
			boat.people.push_back(MakeObject(place.mesh, place.clip));
		}
		for (size_t i = 0; i < boat.wakePhase.size(); ++i)
		{
			boat.wakePhase.at(i) = static_cast<int32_t>(i) * k_WakeCycle / 5;
		}
	}
	g_boat = boat;
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "PetitNavire: mode {}", mode);
}

void Step(int32_t dt);

void Update(float gameMilliseconds)
{
	if (g_pendingMode >= 0 && --g_pendingFrames <= 0)
	{
		Create(std::exchange(g_pendingMode, -1));
		// test only: that much game time at once, in steps of 33 ms
		for (; g_fastForwardMs > 0 && g_boat.has_value(); g_fastForwardMs -= 33)
		{
			Step(33);
		}
	}
	if (g_boat.has_value() && std::getenv("OPENBLACK_BOAT_TRACE") != nullptr)
	{
		g_traceClock += gameMilliseconds;
		if (g_traceClock >= 500.0f)
		{
			g_traceClock = 0.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Boat: mode {} clip {} timer {} hold {} hull ({:.2f}, {:.2f}, {:.2f}) wake {} puffs {}",
			                   g_boat->mode, g_boat->animTime, g_boat->timer, g_boat->hold, g_boat->hull[3].x,
			                   g_boat->hull[3].y, g_boat->hull[3].z, g_wake.size(), smoky_stuff::Get().size());
		}
	}
	g_carry += std::max(gameMilliseconds, 0.0f);
	const auto dt = static_cast<int32_t>(g_carry);
	g_carry -= static_cast<float>(dt);
	Step(dt);
}

void Step(int32_t dt)
{
	// GLandscape::Draw 0x5E490F
	if (g_boat.has_value())
	{
		PreDraw(dt);
	}
	// fn_005E5CD0: fn_00824140 (0x5E619C), then PostDraw of whatever boat there is now (0x5E6250)
	smoky_stuff::Update(static_cast<float>(dt) * 0.001f);
	if (g_boat.has_value())
	{
		PostDraw(dt);
	}
	else
	{
		g_wake.clear();
	}
}

entt::entity GetReflectedHull()
{
	return g_boat.has_value() && g_boat->reflected ? g_boat->hullEntity : entt::null;
}

const std::vector<WakeSprite>& GetWake()
{
	return g_wake;
}

void RunDebugHook()
{
	const char* test = std::getenv("OPENBLACK_TEST_JC_SPECIAL");
	if (test == nullptr)
	{
		return;
	}
	int special = 0;
	int mode = 0;
	int frames = 0;
	int fastForward = 0;
	if (std::sscanf(test, "%d,%d,%d,%d", &special, &mode, &frames, &fastForward) < 1 || special != 6)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"),
		                   "JC special test: OPENBLACK_TEST_JC_SPECIAL=\"6[,mode[,frames[,fast forward ms]]]\", got \"{}\"", test);
		return;
	}
	g_pendingMode = mode;
	g_pendingFrames = frames;
	g_fastForwardMs = fastForward;
}

} // namespace openblack::ecs::petit_navire
