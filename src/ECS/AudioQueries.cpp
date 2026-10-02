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
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Weather/Atmos.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

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
	// GAudio+0x190, written by fn_005E2240 (ProcessAtmosBanks' group 0x428FFA, the alignment music fn_00427460)
	queries.cameraAlignment = &CameraAlignment;
	// GPlayer::GetAlignmentValue 0x64D6A0 of the local player (ProcessCitadelMusic 0x427BB8): openblack's is PLAYER_ONE
	queries.localPlayerAlignment = []() { return ecs::effects::alignment::Get(PlayerNames::PLAYER_ONE); };
	queries.animatedThing = &AnimatedThing;
	queries.animationClipName = &AnimationClipName;
	queries.streetLanterns = &StreetLanterns;
}

void ecs::audio_queries::RunTestHooks(uint32_t turn)
{
	RunViewHook(turn);
	RunLanternHook();
	RunCitadelHook();
}
