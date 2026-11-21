/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TipBubbleSystem.h"

#include "Camera/FieldOfView.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "Help/HelpSystem.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The bubble points at three quarters of the sign's height
constexpr float k_AnchorHeightShare = 0.75f;
} // namespace

void TipBubbleSystem::Show(entt::entity sign, uint32_t text)
{
	_text = text;
	_sign = sign;
	_bubble.Restart();
}

void TipBubbleSystem::Hide()
{
	_sign = entt::null;
}

void TipBubbleSystem::ProcessTurn()
{
	if (_sign == entt::null)
	{
		return;
	}
	// no mesh counts as off the screen; there is no temple test
	const auto& registry = Locator::entitiesRegistry::value();
	ProcessSign(registry.Valid(_sign) && field_of_view::ObjectOnScreen(_sign));
}

void TipBubbleSystem::ProcessSign(bool shown)
{
	if (_sign == entt::null)
	{
		return;
	}
	if (!shown)
	{
		Hide();
		return;
	}
	_bubble.KeepAlive();
}

void TipBubbleSystem::UpdateFrame(float gameMilliseconds)
{
	_bubble.Age(gameMilliseconds); // the frame's game ms x 0.0002, not below 0
}

void TipBubbleSystem::Hover()
{
	_bubble.KeepAlive();
}

bool TipBubbleSystem::IsUp() const
{
	const auto* helpSystem = help::Get();
	return _sign != entt::null && helpSystem != nullptr && !helpSystem->IsScriptWideScreen();
}

std::optional<glm::vec3> TipBubbleSystem::GetAnchor() const
{
	if (_sign == entt::null)
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(_sign) || !registry.AllOf<ecs::components::Transform>(_sign))
	{
		return std::nullopt;
	}
	// (approximate) the Transform's point stands for the MapCoords' x, z and altitude + the thing's height
	return registry.Get<ecs::components::Transform>(_sign).position +
	       glm::vec3(0.0f, ecs::object::GetHeight(_sign) * k_AnchorHeightShare, 0.0f);
}

void TipBubbleSystem::Reset()
{
	_sign = entt::null;
	_text = 0;
	_bubble = help::Bubble {};
}
