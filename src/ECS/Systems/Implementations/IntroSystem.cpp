/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "IntroSystem.h"

#include <cmath>

#include <exception>
#include <string>
#include <vector>

#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "3D/SkeletalPose.h"
#include "Camera/ScriptCamera.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/IntroRules.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/OverlayFrame.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::intro_special
{
namespace
{
using namespace components;
namespace light = intro_rules::light;

/// The intro hand, an animated 3D object (one render flag pending, as for the SuperVillager eyes)
struct HandObject
{
	entt::entity entity {entt::null};
	/// The HD mesh "Data\\MISC\\hand_intro.l3d"
	const super_villager::HdModel* model {nullptr};
	/// The clip's id in the animation manager (0: none)
	entt::id_type clip {0};
	/// the object's cycle time
	int32_t time {0};
	/// the last SetPosition (T(p) Ry(-a) S(s))
	glm::vec3 position {0.0f};
	float yaw {0.0f};
	float scale {1.0f};
};

struct Runtime
{
	int32_t state {-1};
	std::optional<light::Light> beam; ///< The light
	bool material {false};            ///< The misc0 material made (no GPU object in openblack)
	bool cameraFollow {false};        ///< The debug camera's focus kept on the Son's spot each frame
	bool pickUpClipFinished {false};
	HandObject hand;
	bool playing {false};
	float pickUpYaw {0.0f}; ///< Never written (0)
	int32_t pickUpWait {0}; ///< 1000 once the light is gone, then the pick-up clip runs
	bool followSon {false};
	/// this frame: the light's Z object and its callback's draws
	bool lightQueued {false};
	glm::vec3 lightKey {0.0f};
	light::Frame lightFrame;
	/// the last grip written (the value the landscape draw reads)
	std::optional<glm::vec3> grip;
};

/// The opening specials' state (Locator::worldEffects)
Runtime& IntroSpecialData()
{
	return openblack::Locator::worldEffects::value().Get<Runtime>();
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

float CrtRandom(float a, float b)
{
	return game_random::crt::Random(a, b); // the CRT rand() stream
}

/// Data\MISC\<file>, loaded once into the animation manager. (approximate) the original frees the
/// clip and reads the file again at each special 4; the data is the same
entt::id_type LoadClip(const char* file)
{
	const auto id = resources::HashIdentifier(std::string("misc/") + file);
	auto& animations = Locator::resources::value().GetAnimations();
	if (animations.Contains(id))
	{
		return id;
	}
	try
	{
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / file;
		animations.Load(id, resources::L3DAnimLoader::FromDiskTag {}, path);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Intro: cannot load {}: {}", file, e.what());
		return 0;
	}
	return id;
}

const L3DAnim* Clip(entt::id_type id)
{
	if (id == 0)
	{
		return nullptr;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	return animations.Contains(id) ? &*animations.Handle(id) : nullptr;
}

/// The animated object with the HD mesh: an entity with no Mesh yet (Update gives it one on the frames the hand is
/// drawn)
void CreateHand()
{
	auto& hand = IntroSpecialData().hand;
	hand = HandObject {};
	hand.model = super_villager::LoadHdModel("hand_intro", "hand_intro");
	auto& registry = Entities();
	hand.entity = registry.Create();
	registry.Assign<Transform>(hand.entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& animation = registry.Assign<SkeletalAnimation>(hand.entity);
	animation.speed = 0.0f; // the time is the Intro's, it does not run by itself
}

/// The hand's current clip
void SetClip(entt::id_type clip)
{
	auto& state = IntroSpecialData();
	state.hand.clip = clip;
	if (state.hand.entity != entt::null && Entities().Valid(state.hand.entity))
	{
		auto& animation = Entities().Get<SkeletalAnimation>(state.hand.entity);
		animation.clip = clip;
		animation.hasClip = clip != 0;
	}
}

/// The hand's position, yaw and scale
void SetPosition(const glm::vec3& position, float yaw, float scale)
{
	auto& hand = IntroSpecialData().hand;
	hand.position = position;
	hand.yaw = yaw;
	hand.scale = scale;
}

/// The hand object, its clip and its mesh go. (approximate) the mesh and the clip stay in their managers
void FreeHand()
{
	auto& hand = IntroSpecialData().hand;
	if (hand.entity != entt::null && Locator::entitiesRegistry::has_value() && Entities().Valid(hand.entity))
	{
		Entities().Destroy(hand.entity);
		Entities().SetDirty();
	}
	hand = HandObject {};
}

/// The draw of this frame (one render flag pending): the animated object drawn at once at `time`; on the other
/// frames the hand is not drawn
void ShowHand(bool drawn, int32_t time)
{
	auto& hand = IntroSpecialData().hand;
	if (hand.entity == entt::null || !Entities().Valid(hand.entity))
	{
		return;
	}
	auto& registry = Entities();
	const bool hasMesh = registry.AllOf<Mesh>(hand.entity);
	const bool wantMesh = drawn && hand.model != nullptr;
	if (wantMesh && !hasMesh)
	{
		registry.Assign<Mesh>(hand.entity, hand.model->mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	else if (!wantMesh && hasMesh)
	{
		registry.Remove<Mesh>(hand.entity);
	}
	auto& transform = registry.Get<Transform>(hand.entity);
	const auto rotation = affine::AngleY(hand.yaw);
	if (transform.position != hand.position || transform.rotation != rotation || transform.scale.x != hand.scale)
	{
		transform.position = hand.position;
		transform.rotation = rotation;
		transform.scale = glm::vec3(hand.scale);
		registry.SetDirty();
	}
	registry.Get<SkeletalAnimation>(hand.entity).time = static_cast<float>(time);
}

/// The light's Z object for this frame, keyed at its head as it is now
void QueueLight()
{
	auto& state = IntroSpecialData();
	state.lightQueued = true;
	state.lightKey = state.beam->head;
}

void Play(int32_t special)
{
	auto& intro = IntroSpecialData();
	switch (special)
	{
	case 0:
		intro.state = 0;
		intro.material = true; // the misc0 material, once
		// a new light; a light still there is dropped without its delete, as in the original
		intro.beam = light::Make(intro_rules::k_BoySpot, intro_rules::k_LightWay, CrtRandom);
		intro.followSon = false;
		return;
	case 1: // state 0 only
		if (intro.state != 0)
		{
			return;
		}
		script_camera::SetDebugCameraFocus(intro_rules::k_BoySpot);
		intro.cameraFollow = true;
		script_camera::SetDebugCameraMode(2);
		if (intro.beam.has_value()) // (approximate) the original starts a null light too
		{
			script_camera::SetDebugCameraPosition(light::Start(*intro.beam));
		}
		return;
	case 2: // state 0 only
		if (intro.state != 0)
		{
			return;
		}
		intro.cameraFollow = false;
		script_camera::SetDebugCameraMode(0);
		return;
	case 4:
	{
		const bool had = intro.hand.entity != entt::null;
		if (!had)
		{
			CreateHand();
		}
		// else the original frees the hand's clip
		SetClip(LoadClip("hand_intro.anm"));
		intro.hand.time = 0;
		SetPosition(intro_rules::k_PutDownSpot, intro_rules::PutDownYaw(), intro_rules::k_HandScale);
		intro.state = 4;
		return;
	}
	case 5: // state 4 only
		if (intro.state != 4)
		{
			return;
		}
		if (intro.playing) // the jump to 2379 ms, the Son let go
		{
			intro.hand.time = intro_rules::k_LetGoMs;
			intro.followSon = false;
			return;
		}
		intro.playing = true;
		intro.followSon = true;
		return;
	default: // 3, 6 and above: not here (the script's special table)
		return;
	}
}

void Update(uint32_t milliseconds)
{
	auto& intro = IntroSpecialData();
	intro.lightQueued = false;
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	switch (intro.state)
	{
	case 0: // the light falls
		if (!intro.beam.has_value())
		{
			break;
		}
		if (intro.beam->state == light::State::Gone) // deleted
		{
			intro.beam.reset();
			break;
		}
		QueueLight();
		if (intro.beam->state == light::State::Holding) // landed last frame, the pick-up hand comes
		{
			intro.state = 12;
		}
		if (intro.cameraFollow)
		{
			script_camera::SetDebugCameraFocus(intro_rules::k_BoySpot);
		}
		break;
	case 4: // the put-down hand
	{
		ShowHand(true, intro.hand.time);
		if (!intro.playing)
		{
			break;
		}
		const auto* clip = Clip(intro.hand.clip);
		bool wrapped = false;
		const int32_t next =
		    clip != nullptr ? intro_rules::AdvanceClip(intro.hand.time, milliseconds, static_cast<int32_t>(clip->GetPlayTime()),
		                                               clip->IsLooping(), wrapped)
		                    : intro.hand.time;
		if (wrapped) // all of it goes (not followSon)
		{
			FreeHand();
			intro.playing = false;
			intro.state = -1;
			break;
		}
		intro.hand.time = next;
		break;
	}
	case 12: // the pick-up hand
	{
		if (intro.hand.entity == entt::null)
		{
			intro.pickUpWait = 0; // (and one more field that is never read)
			CreateHand();
			SetClip(LoadClip("hand_intro2.anm"));
			SetPosition(intro_rules::k_BoySpot, intro.pickUpYaw, intro_rules::k_HandScale); // written again below
		}
		if (intro.beam.has_value())
		{
			if (intro.beam->state == light::State::Gone) // the light goes, the clip may run
			{
				intro.beam.reset();
				intro.pickUpWait = intro_rules::k_LiftGo;
			}
			else
			{
				QueueLight();
			}
		}
		if (intro.pickUpWait != 0)
		{
			const auto* clip = Clip(intro.hand.clip);
			bool wrapped = false;
			const int32_t next = clip != nullptr ? intro_rules::AdvanceClip(intro.hand.time, milliseconds,
			                                                                static_cast<int32_t>(clip->GetPlayTime()),
			                                                                clip->IsLooping(), wrapped)
			                                     : intro.hand.time;
			if (wrapped)
			{
				intro.pickUpClipFinished = true; // (hand_intro2.anm is one shot: never)
			}
			else
			{
				intro.hand.time = next;
			}
		}
		SetPosition(intro_rules::k_PickUpSpot, intro_rules::k_PickUpYaw, intro_rules::k_HandScale);
		ShowHand(true, intro.hand.time);
		break;
	}
	default:
		break;
	}
	if (intro.state != 4 && intro.state != 12)
	{
		ShowHand(false, intro.hand.time); // drawn only by states 4 and 12 (state 0 keeps an old hand undrawn)
	}
	// the light's Z object is drawn in the main view's queue: its callback runs once this frame. Its CRT Random draws
	// happen here: (approximate, pending) the original draws in the Z drain, after the clouds, night lights and smoke;
	// to move to the end of Renderer::PreDraw
	if (intro.lightQueued)
	{
		light::Step(*intro.beam, milliseconds, CrtRandom, intro.lightFrame);
		if (intro.lightFrame.cameraAt.has_value())
		{
			script_camera::SetDebugCameraPosition(*intro.lightFrame.cameraAt);
		}
	}
}

std::optional<glm::vec3> Grip()
{
	auto& intro = IntroSpecialData();
	if (!intro.followSon)
	{
		return std::nullopt;
	}
	const auto& hand = intro.hand;
	// a hand, playing, its time <= 1817, its mesh with an EBone block
	if (hand.entity != entt::null && intro.playing && hand.model != nullptr && hand.model->host.has_value())
	{
		const auto& meshes = Locator::resources::value().GetMeshes();
		const auto* clip = Clip(hand.clip);
		if (clip != nullptr && meshes.Contains(hand.model->mesh))
		{
			const auto model = affine::Model(hand.position, affine::AngleY(hand.yaw), glm::vec3(hand.scale));
			const auto& host = *hand.model->host;
			// the pose at the clip's time, every bone in the world; none is needed past the time the hand lets go
			std::vector<glm::mat4> pose;
			if (static_cast<float>(hand.time) <= intro_rules::k_HoldsUntilMs)
			{
				graphics::ComputePose(*meshes.Handle(hand.model->mesh), *clip, static_cast<float>(hand.time), pose);
			}
			// the grip point is the first EBone matrix's translation
			const auto& eBone = host.matrices.at(0);
			if (const auto point = intro_rules::GripPoint(hand.time, pose, static_cast<uint32_t>(host.bones.at(0)),
			                                              glm::vec3(eBone.at(9), eBone.at(10), eBone.at(11)), model);
			    point.has_value())
			{
				intro.grip = point;
			}
		}
	}
	return intro.grip;
}

void FillFrame(graphics::IntroLightOverlay& out)
{
	auto& state = IntroSpecialData();
	out.active = state.lightQueued;
	out.sprites.clear();
	if (!out.active)
	{
		return;
	}
	out.keyPoint = state.lightKey;
	out.depthAlways = state.lightFrame.throughEverything;
	out.sprites = state.lightFrame.drawn;
}

bool HasPickUpClipFinished()
{
	return IntroSpecialData().pickUpClipFinished;
}

void ReleaseAll()
{
	auto& intro = IntroSpecialData();
	intro.beam.reset();
	FreeHand(); // the object, the mesh, the clip
	intro.material = false;
	intro.playing = false;
	script_camera::SetDebugCameraMode(0);
	intro.state = -1;
	intro.lightQueued = false;
	// kept, as in the original: followSon, cameraFollow, pickUpClipFinished, pickUpWait
}

} // namespace
} // namespace openblack::ecs::intro_special

using namespace openblack;
using namespace openblack::ecs::systems;

void IntroSystem::Play(int32_t special)
{
	ecs::intro_special::Play(special);
}

void IntroSystem::ReleaseAll()
{
	ecs::intro_special::ReleaseAll();
}

bool IntroSystem::HasLiftFinished() const
{
	return ecs::intro_special::HasPickUpClipFinished();
}

void IntroSystem::Update(uint32_t milliseconds)
{
	// in the landscape's draw, before the SuperVillager loop: the intro hand's grip, from the hand as the last frame
	// left it; then, just before the boat's after-draw, the light's draw of this frame: the intro light, the chasing
	// camera's points and the intro hand
	ecs::super_villager::SetIntroHandGrip(ecs::intro_special::Grip());
	ecs::intro_special::Update(milliseconds);
}

void IntroSystem::FillFrame(graphics::IntroLightOverlay& out) const
{
	ecs::intro_special::FillFrame(out);
}
