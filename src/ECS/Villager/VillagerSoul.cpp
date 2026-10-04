/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSoul.h"

#include <string>
#include <vector>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "Common/GameRandom.h"
#include "ECS/Animations.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::villager_soul
{
using namespace components;

namespace
{
constexpr int32_t k_Dead1GotoHeaven = 244; ///< 0xEB9A80 = AnimPack[0xF4] (0x76A684)
constexpr int32_t k_Dead1GotoHell = 245;   ///< 0xEB9A88 = AnimPack[0xF5] (0x76A6B6)
constexpr int32_t k_Dead2GotoHeaven = 247; ///< 0xEB9A84 = AnimPack[0xF7] (0x76A69D)
constexpr int32_t k_Dead2GotoHell = 248;   ///< 0xEB9A8C = AnimPack[0xF8] (0x76A6CF)
constexpr uint8_t k_Alpha = 105;           ///< 0x8289D2 `mov edi, 0x69`

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The source's current clip name (LH3DObject vt +0x184 -> the anim's name) is "M_P_DEAD1" (0xC38664, the strcmp
/// 0x82882C..0x828856)
bool SourceClipIsDead1(entt::entity source)
{
	const auto* animation = Entities().TryGet<const SkeletalAnimation>(source);
	if (animation == nullptr || !animation->hasClip || !Locator::resources::has_value())
	{
		return false;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(animation->clip))
	{
		return false;
	}
	return animations.Handle(animation->clip)->GetName() == "M_P_DEAD1";
}

uint32_t ClipDuration(entt::id_type clip)
{
	if (!Locator::resources::has_value())
	{
		return 0;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(clip))
	{
		return 0;
	}
	return static_cast<uint32_t>(animations.Handle(clip)->GetDurationMs());
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

int32_t SoulClip(bool sourceIsDead1Name, bool heavenForced, float roll)
{
	// 0x828859..0x8288D1: the pair by the name, heaven when roll < 50 (fcomp [0x8C6CA4]; test ah, 1)
	const bool heaven = roll < 50.0f;
	int32_t clip = sourceIsDead1Name ? (heaven ? k_Dead2GotoHeaven : k_Dead2GotoHell)
	                                 : (heaven ? k_Dead1GotoHeaven : k_Dead1GotoHell);
	// 0x82888D..0x8288A0 / 0x8288D7..0x8288EA: heavenForced -> SetAnim(the heaven clip) again
	if (heavenForced)
	{
		clip = sourceIsDead1Name ? k_Dead2GotoHeaven : k_Dead1GotoHeaven;
	}
	return clip;
}

uint8_t SoulAlpha(uint32_t elapsed, uint32_t duration)
{
	// 0x8289DD..0x8289EA: eax = duration - 500 (0x1F4); elapsed > eax (signed jle) -> the fade
	const auto start = static_cast<int32_t>(duration) - 500;
	const auto now = static_cast<int32_t>(elapsed);
	if (now <= start)
	{
		return k_Alpha;
	}
	// 0x8289EC..0x828A08: fild (elapsed - start); fmul 0.002 (0x8C78E8); fsubr 1.0; fmul 105.0 (0x9A3960); __ftol (each
	// x87 step rounded to float: 24-bit control word, 0x7DEE0D)
	const float x = (1.0f - static_cast<float>(now - start) * 0.002f) * 105.0f;
	return static_cast<uint8_t>(static_cast<int32_t>(x));
}

bool SoulExpired(uint32_t elapsed, uint32_t duration)
{
	// 0x8289B0..0x8289B5: lea ecx, [edi + 0x6E]; cmp ecx, duration; jle -> stays
	return static_cast<int32_t>(elapsed) + 110 > static_cast<int32_t>(duration);
}

// ---- the souls ---------------------------------------------------------------------------------------------------

int32_t Create(entt::entity source, MeshId mesh, bool heavenForced)
{
	auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(source);
	if (transform == nullptr)
	{
		return -1;
	}
	// 0x828790..0x8287C7: the record at the head of 0xEB9A7C, LH3DObject::Create(2) 0x80B4D0
	const auto soul = registry.Create();
	// 0x8287D6..0x8287E6: the source's position and orientation (+0x38, +0x44, +0x48): the 3D object's, where it is drawn
	// (DrawPosition), else its Transform. (inferred) LH3DObject::Create's scale 1: the source's scale is not copied
	const auto* draw = registry.TryGet<const DrawPosition>(source);
	const glm::vec3 position = draw != nullptr && draw->started ? draw->position : transform->position;
	const glm::mat3 rotation = draw != nullptr && draw->started ? draw->rotation : transform->rotation;
	registry.Assign<Transform>(soul, position, rotation, glm::vec3(1.0f));
	// 0x8287E9..0x82880A: mesh = MeshPack[mesh] (0 out of range)
	registry.Assign<Mesh>(soul, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// 0x82881D..0x8288EA: the clip; Random(0, 100) (0x81D180, the CRT stream) drawn always, before the name test is used
	const bool dead1 = SourceClipIsDead1(source);
	const float roll = game_random::crt::Random(0.0f, 100.0f);
	const int32_t clip = SoulClip(dead1, heavenForced, roll);
	auto& animation = registry.Assign<SkeletalAnimation>(soul);
	animation.clip = ClipId(static_cast<uint32_t>(clip));
	animation.clipIndex = clip;
	animation.hasClip = true;
	animation.time = 0.0f;
	// fn_00828990's colour alpha << 24 | 0xFFFFFF (0x828A21..0x828A2E, vt +0x2C): white, alpha 105 / 255 in the
	// translucent pass. (inferred) the original puts the alpha in the object colour; here it goes through
	// components::Alpha (render_modes' GlobalAlpha, the shared translucent path), ObjectColour stays white
	registry.Assign<ObjectColour>(soul);
	registry.Assign<Alpha>(soul, static_cast<float>(k_Alpha) / 255.0f);
	registry.Assign<VillagerSoul>(soul, 0u, ClipDuration(animation.clip));
	return clip;
}

void Update(uint32_t milliseconds)
{
	auto& registry = Entities();
	std::vector<entt::entity> gone;
	registry.Each<VillagerSoul, SkeletalAnimation, Alpha>(
	    [&gone, milliseconds](entt::entity soul, VillagerSoul& record, SkeletalAnimation& animation, Alpha& alpha) {
		    // 0x828991..0x8289A2: elapsed += g_game_time_inc
		    record.elapsedMs += milliseconds;
		    // 0x8289A5..0x8289B5: elapsed + 110 > the clip's duration -> freed (fn_00828900: the LH3DObject released)
		    if (SoulExpired(record.elapsedMs, record.durationMs))
		    {
			    gone.push_back(soul);
			    return;
		    }
		    // 0x8289C0..0x8289C7: the clip's time = elapsed (vt +0x188)
		    animation.time = static_cast<float>(record.elapsedMs);
		    // 0x8289CD..0x828A2E: the alpha and SetColour(alpha << 24 | 0xFFFFFF); drawn (vt +0x100)
		    alpha.value = static_cast<float>(SoulAlpha(record.elapsedMs, record.durationMs)) / 255.0f;
	    });
	for (const auto soul : gone)
	{
		registry.Destroy(soul);
	}
	if (!gone.empty())
	{
		registry.SetDirty();
	}
}
} // namespace openblack::ecs::villager_soul
