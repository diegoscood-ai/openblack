/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureKnock.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include "Creature/CreatureFight.h"

namespace openblack::creature_knock
{
namespace
{
/// The size the head and feet scales are taken at: the drawn size kept to 0..2
float ProportionSize(float drawnSize)
{
	return drawnSize > 0.0f ? (drawnSize < k_MaxProportionSize ? drawnSize : k_MaxProportionSize) : 0.0f;
}
} // namespace

Bones Initialise(const Bones& before, const BoneIndices& indices, float drawnSize)
{
	const float height = drawnSize * k_StandingHeightPerSize;
	const float size = ProportionSize(drawnSize);
	const float headScale = k_HeadScaleAtSizeZero - (size * k_HeadScalePerSize);
	const float feetScale = k_FeetScaleAtSizeZero - (size * k_FeetScalePerSize);
	const float head = (headScale * height) * k_BoneShare;
	const float rump = height * k_RumpShare;
	const float limb = (feetScale * height) * k_BoneShare;
	const float armpit = height * k_BoneShare;
	const std::array<std::pair<uint32_t, float>, k_BoneCount> set {{
	    {indices.head, head},
	    {indices.rump, rump},
	    {indices.rightHand, limb},
	    {indices.leftHand, limb},
	    {indices.rightFoot, limb},
	    {indices.leftFoot, limb},
	    {indices.rightArmpit, armpit},
	    {indices.leftArmpit, armpit},
	}};
	Bones bones = before;
	for (size_t i = 0; i < k_BoneCount; ++i)
	{
		bones.at(i).bone = set.at(i).first;
		bones.at(i).radius = set.at(i).second;
	}
	return bones;
}

float MoveThresholdSq(float drawnSize)
{
	const float radius = drawnSize * k_MovePerSize;
	return radius * radius;
}

Move MoveOf(glm::vec3 last, glm::vec3 now, float thresholdSq)
{
	const glm::vec3 by {now.x - last.x, now.y - last.y, now.z - last.z};
	const float xx = by.x * by.x;
	const float zz = by.z * by.z;
	const float yy = by.y * by.y;
	const float lengthSq = (xx + zz) + yy;
	return {.by = by, .knocks = lengthSq > thresholdSq};
}

glm::ivec2 CellOf(glm::vec3 place)
{
	return {static_cast<int32_t>(place.x * k_CellsPerMetre), static_cast<int32_t>(place.z * k_CellsPerMetre)};
}

bool CanKnock(const Facts& facts)
{
	if (!facts.hasModel || facts.indestructible || facts.creature || facts.templeHeart || facts.tree || facts.storagePit ||
	    facts.townCentre || facts.fragment)
	{
		return false;
	}
	if (facts.villager || (facts.animal && !facts.dove))
	{
		return !facts.immovable;
	}
	if (facts.living || facts.animatedModel)
	{
		return false;
	}
	if (facts.home)
	{
		return !facts.field;
	}
	return facts.canBecomePhysicsObject && !facts.immovable;
}

Kind KindOf(const Facts& facts)
{
	if (!CanKnock(facts))
	{
		return Kind::None;
	}
	if (facts.villager || facts.animal)
	{
		return Kind::Living;
	}
	if (facts.home)
	{
		return Kind::Home;
	}
	return facts.canBecomePhysicsObject && !facts.immovable ? Kind::Thing : Kind::None;
}

bool WithinReach(Kind kind, glm::vec2 at, float radius, glm::vec3 bone, float boneRadius)
{
	if (kind == Kind::None)
	{
		return false;
	}
	const float reach = kind == Kind::Home ? radius + boneRadius : radius + (boneRadius * k_ReachShare);
	const float dx = at.x - bone.x;
	const float dz = at.y - bone.z;
	const float distanceSq = (dz * dz) + (dx * dx);
	return distanceSq < reach * reach;
}

glm::vec3 ToWorld(const Rows& rows, glm::vec3 point)
{
	const auto axis = [&](int i) {
		const float yx = (rows.y[i] * point.y) + (rows.x[i] * point.x);
		const float yxz = yx + (rows.z[i] * point.z);
		return yxz + rows.place[i];
	};
	return {axis(0), axis(1), axis(2)};
}

bool MeetsAnyPoint(const Rows& rows, std::span<const glm::vec3> points, glm::vec3 at, float radius)
{
	const float radiusSq = radius * radius;
	return std::ranges::any_of(points, [&](const glm::vec3& point) {
		const auto world = ToWorld(rows, point);
		const float dx = world.x - at.x;
		const float dy = world.y - at.y;
		const float dz = world.z - at.z;
		const float distanceSq = ((dz * dz) + (dy * dy)) + (dx * dx);
		return distanceSq < radiusSq;
	});
}

glm::vec3 Push(glm::vec3& move)
{
	move = {move.x * k_PushPerMove, move.y * k_PushPerMove, move.z * k_PushPerMove};
	return move;
}

glm::vec3 HomeBlow(glm::vec3& move)
{
	float length = 0.0f;
	if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f)
	{
		const float xx = move.x * move.x;
		const float zz = move.z * move.z;
		const float yy = move.y * move.y;
		length = std::sqrt((xx + zz) + yy);
		const float scale = 1.0f / length;
		move = {scale * move.x, scale * move.y, scale * move.z};
	}
	const float speed = (length * k_HomeBlowPerMove) * k_HomeBlowShare;
	return {speed * move.x, speed * move.y, speed * move.z};
}

std::optional<Mark> FightMark(creature_fight::State state)
{
	using creature_fight::State;
	switch (state)
	{
	case State::Start:
	case State::Action:
	case State::CastStart:
		return Mark::Start;
	case State::Faint:
		return Mark::SetUpAndStart;
	case State::Idle:
	case State::Stance:
	case State::Lying:
		return Mark::Stop;
	default:
		return std::nullopt;
	}
}

BodyState BodyStateOf(const creature_layers::BodyAction& body)
{
	using creature_layers::BodyAction;
	if (body.timedByPlayer)
	{
		return BodyState::Fighting;
	}
	if (body.kind == BodyAction::Kind::Stand)
	{
		return BodyState::Standing;
	}
	if (body.kind != BodyAction::Kind::Sequence)
	{
		return BodyState::Other;
	}
	if (body.animations[1] == creature_layers::animations::k_Sleep)
	{
		switch (body.phase)
		{
		case BodyAction::Phase::Start:
			return BodyState::SleepStart;
		case BodyAction::Phase::Loop:
			return BodyState::Sleeping;
		case BodyAction::Phase::End:
			return BodyState::SleepEnd;
		}
	}
	namespace fight = creature_fight::animations;
	if (body.animations[0] == fight::k_Faint && body.animations[1] == fight::k_Faint && body.animations[2] == fight::k_GetUp)
	{
		switch (body.phase)
		{
		case BodyAction::Phase::Start:
			return BodyState::Fainting;
		case BodyAction::Phase::Loop:
			return BodyState::LyingFainted;
		case BodyAction::Phase::End:
			return BodyState::GettingUp;
		}
	}
	return BodyState::Other;
}

std::optional<Mark> BodyMark(BodyState state)
{
	switch (state)
	{
	case BodyState::Standing:
	case BodyState::Sleeping:
	case BodyState::LyingFainted:
		return Mark::Stop;
	case BodyState::SleepStart:
	case BodyState::SleepEnd:
	case BodyState::Fainting:
		return Mark::SetUpAndStart;
	default:
		return std::nullopt;
	}
}
} // namespace openblack::creature_knock
