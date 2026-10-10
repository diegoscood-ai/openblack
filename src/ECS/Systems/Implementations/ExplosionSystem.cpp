/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ExplosionSystem.h"

#include "Camera/CameraShake.h"
#include "ECS/GroundMarks.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void ExplosionSystem::AddRubble(const glm::vec3& centre, float yaw)
{
	ecs::ground_marks::CreateExplosionMark(centre, yaw);
}

void ExplosionSystem::AddShake(const glm::vec3& position, float radius, float strength, float seconds)
{
	camera_shake::StartCameraShake(position, radius, strength, seconds);
}

bool ExplosionSystem::IsShaking() const
{
	return !camera_shake::Checkers().empty();
}

void ExplosionSystem::Update(float milliseconds)
{
	ecs::ground_marks::Update(milliseconds);
}

void ExplosionSystem::Reset()
{
	ecs::ground_marks::Clear();
}
