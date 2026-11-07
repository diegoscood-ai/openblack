/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Animations.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <map>
#include <ranges>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "3D/SkeletalPose.h"
#include "Audio/Services/AnimationSounds.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/VillagerClips.h"
#include "Graphics/PoseFit.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs
{

using components::Mesh;
using components::SkeletalAnimation;

namespace
{
/// What the animation trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct AnimationsDebugHooksState
{
	float traceClock {0.0f}; // OPENBLACK_ANIM_TRACE: game milliseconds since the last line
};

AnimationsDebugHooksState& AnimationsDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::animations: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<AnimationsDebugHooksState>();
}
} // namespace

entt::id_type ClipId(uint32_t index)
{
	return resources::HashIdentifier(index);
}

void UpdateAnimations(float milliseconds)
{
	// OPENBLACK_ANIM_TRACE=1: every 2 s of game time, how many play each clip (and at what walk speed)
	auto& traceClock = AnimationsDebugHooksData().traceClock;
	traceClock += milliseconds;
	const bool trace = traceClock >= 2000.0f && std::getenv("OPENBLACK_ANIM_TRACE") != nullptr;
	if (trace)
	{
		traceClock = 0.0f;
		std::map<int32_t, std::pair<int, float>> clips;
		Locator::entitiesRegistry::value().Each<const SkeletalAnimation>([&clips](const SkeletalAnimation& animation) {
			auto& entry = clips[animation.clipIndex];
			++entry.first;
			const float speed = animation.groundSpeed.has_value()
			                        ? map_coords::ToMetres(static_cast<int32_t>(*animation.groundSpeed)) * 10.0f
			                        : 0.0f;
			entry.second = std::max(entry.second, speed);
		});
		std::string line;
		for (const auto& [clip, entry] : clips)
		{
			line += fmt::format(" {}x{} ({:.2f} m/s)", clip, entry.first, entry.second);
		}
		if (auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_INFO(logger, "Anim trace:{}", line);
		}
	}

	audio::AnimationSounds::Update();
	auto& registry = Locator::entitiesRegistry::value();
	auto& resources = Locator::resources::value();
	const auto& animations = resources.GetAnimations();
	const auto& meshes = resources.GetMeshes();
	registry.Each<SkeletalAnimation, const Mesh>(
	    [&](entt::entity entity, SkeletalAnimation& animation, const Mesh& mesh) {
		    // one lookup of each (an empty handle when the cache has none)
		    const auto clip = animation.hasClip ? animations.Handle(animation.clip) : decltype(animations.Handle(0)) {};
		    const auto model = clip ? meshes.Handle(mesh.id) : decltype(meshes.Handle(0)) {};
		    if (!clip || !model)
		    {
			    animation.pose.clear();
			    animation.locals.clear();
			    animation.drawnPose.clear();
			    return;
		    }
		    const auto duration = static_cast<float>(clip->GetPlayTime());
		    const bool byGround =
		        animation.groundSpeed.has_value() && villager_clips::PlaysByGround(clip->GetCycleDistance(), clip->GetFlags());
		    // a villager's or an animal's clip that the test hook holds still (speed 0) stays out of the livings' play;
		    // looked up through the const registry, which makes no storage
		    const bool living = byGround || (animation.speed > 0.0f &&
		                                     std::as_const(registry).AnyOf<components::Villager, components::Animal>(entity));
		    if (living)
		    {
			    // a villager's or an animal's clip plays in whole milliseconds: a villager in a moving state, or an animal
			    // that moved during the last turn, by the ground its whole speed covers, whether or not that is what it
			    // moved; otherwise by the frame's milliseconds. The clip's sound events fire over the stretches played,
			    // a held one-shot clip's every frame
			    const auto* transform = registry.TryGet<const components::Transform>(entity);
			    const float scale = transform != nullptr ? transform->scale.x : 1.0f;
			    const auto playTime = static_cast<int32_t>(clip->GetPlayTime());
			    const auto played = villager_clips::FramePlay(animation.groundSpeed, static_cast<uint32_t>(milliseconds), scale,
			                                                  clip->GetCycleDistance(), clip->GetFlags(), playTime);
			    const auto step =
			        villager_clips::AdvanceClip(static_cast<int32_t>(animation.time), played, playTime, clip->IsLooping());
			    for (const auto& span : step.played)
			    {
				    audio::AnimationSounds::Fire(entity, animation.clipIndex, span.from, span.to);
			    }
			    animation.time = static_cast<float>(step.place);
		    }
		    else
		    {
			    // the other boned meshes: the time advances by the frame's milliseconds; looping clips wrap, one-shot ones
			    // stop at the end
			    const float advance = milliseconds * animation.speed;
			    const float before = animation.time;
			    animation.time += advance;
			    // the clip's sound events crossed this frame, both sides of a wrap
			    if (advance > 0.0f)
			    {
				    const auto from = static_cast<int32_t>(before);
				    const auto to = static_cast<int32_t>(animation.time);
				    if (clip->IsLooping() && duration > 0.0f && animation.time >= duration)
				    {
					    audio::AnimationSounds::Fire(entity, animation.clipIndex, from, static_cast<int32_t>(duration));
					    audio::AnimationSounds::Fire(entity, animation.clipIndex, 0,
					                                 static_cast<int32_t>(std::fmod(animation.time, duration)));
				    }
				    else
				    {
					    audio::AnimationSounds::Fire(entity, animation.clipIndex, from, to);
				    }
			    }
			    if (duration > 0.0f)
			    {
				    animation.time =
				        clip->IsLooping() ? std::fmod(animation.time, duration) : std::min(animation.time, duration);
			    }
		    }
		    graphics::ComputePoseInto(*model, *clip, animation.time, animation.pose, animation.locals);
		    // a SuperVillager (ECS/SuperVillager.h): the clip drawn before a change is blended in for crossFadeMs, into its own
		    // bone buffer only (drawnPose); `pose` above stays the plain one
		    if (animation.crossFadeMs > 0)
		    {
			    animation.drawnPose.clear();
			    if (animation.crossFadeFrozen)
			    {
				    return; // no fade step this frame
			    }
			    auto& fade = animation.crossFade;
			    const bool fading =
			        StepCrossFade(fade, animation.clip, static_cast<int32_t>(milliseconds), animation.crossFadeMs);
			    // while there is fade left and no snap (DrawPosition::followSnap, the same flag as the yaw stage's).
			    // (openblack) a fade from a clip that is not loaded draws the plain pose
			    const auto* draw = registry.TryGet<const components::DrawPosition>(entity);
			    const bool snap = draw != nullptr && draw->followSnap;
			    if (fading && !snap && animations.Contains(fade.oldClip))
			    {
				    graphics::ComputeBlendedPose(*model, *clip, animation.time, *animations.Handle(fade.oldClip), fade.oldTime,
				                                 fade.weight, animation.drawnPose);
			    }
			    fade.lastTime = animation.time;
		    }
	    },
	    entt::exclude<components::Unavailable>);
}

const std::vector<glm::mat4>& DrawnPose(const SkeletalAnimation& animation)
{
	return animation.drawnPose.empty() ? animation.pose : animation.drawnPose;
}

bool StepCrossFade(components::SkeletalAnimation::CrossFade& fade, entt::id_type clip, int32_t milliseconds, int32_t fadeMs)
{
	if (!fade.hasLast)
	{
		// the first clip is only kept
		fade.hasLast = true;
		fade.lastClip = clip;
	}
	else if (fade.lastClip != clip)
	{
		// another clip: a fade from the clip and time drawn last
		fade.oldClip = fade.lastClip;
		fade.oldTime = fade.lastTime;
		fade.leftMs = fadeMs;
		fade.lastClip = clip;
		fade.weight = 1.0f;
	}
	else
	{
		// the same clip: the fade counts down
		fade.leftMs -= milliseconds;
		if (fade.leftMs < 0)
		{
			fade.leftMs = 0;
		}
		else
		{
			fade.weight = static_cast<float>(fade.leftMs) / static_cast<float>(fadeMs);
		}
	}
	return fade.leftMs != 0;
}

PoseMap PosesByInstance(const std::unordered_map<entt::entity, systems::RenderContext::EntityInstance>& entityInstances)
{
	PoseMap poses;
	Locator::entitiesRegistry::value().Each<const SkeletalAnimation>(
	    [&](entt::entity entity, const SkeletalAnimation& animation) {
		    if (animation.pose.empty())
		    {
			    return;
		    }
		    if (const auto instance = entityInstances.find(entity); instance != entityInstances.end())
		    {
			    poses.emplace(instance->second.index, &DrawnPose(animation));
		    }
	    });
	// a creature is posed by its own animations, and drawn between its last two turn poses when it has them; looked up
	// through the const registry, which makes no storage on a land without a creature (the state hash counts the
	// storages)
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const components::CreatureAnimation>([&](entt::entity entity, const components::CreatureAnimation& animation) {
		    // a body not posed yet has no drawn bones either: they are only made from a posed body
		    if (animation.boneMatrices.empty())
		    {
			    return;
		    }
		    if (const auto instance = entityInstances.find(entity); instance != entityInstances.end())
		    {
			    poses.emplace(instance->second.index, &creature_turn_pose::BonesDrawn(animation));
		    }
	    });
	return poses;
}

PoseMap PosesByInstance(const systems::RenderContext& context)
{
	auto poses = PosesByInstance(context.entityInstances);
	for (const auto& [index, pose] : context.instancePoses)
	{
		poses.emplace(index, &pose);
	}
	return poses;
}

bool HasPose(const PoseMap& poses, uint32_t offset, uint32_t count)
{
	if (poses.empty())
	{
		return false;
	}
	return std::ranges::any_of(std::views::iota(0u, count),
	                           [&poses, offset](uint32_t i) { return poses.contains(offset + i); });
}

void UsePose(const PoseMap& poses, uint32_t instance, const graphics::L3DMesh& mesh, const glm::mat4*& matrices, uint8_t& count)
{
	const auto pose = poses.find(instance);
	// the pose is the object's own (std) mesh's; a finer drawn mesh takes its matrices by bone index, as long as it uses
	// none past them
	if (pose == poses.end() || !graphics::pose_fit::PoseFits(pose->second->size(), mesh.GetHighestSkinnedBone()))
	{
		return;
	}
	matrices = pose->second->data();
	count = static_cast<uint8_t>(pose->second->size());
}

} // namespace openblack::ecs
