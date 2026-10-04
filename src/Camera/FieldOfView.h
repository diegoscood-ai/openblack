/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Graphics/RegionOnScreen.h"

/// The scripts' "is it on the screen" tests of runblack.exe W120 (research dev\documentacion\intro\spec_timers_events.md
/// §3): GAME_THING_FIELD_OF_VIEW 011 (GScript::IsGameThingFieldOfView 0x6F8130) and POS_FIELD_OF_VIEW 012
/// (GScript::IsPosFieldOfView 0x6F8060). Both go through the drawn camera's LH3DTech::g_world_to_clipping [0xEA9E40]
/// (openblack: Camera::GetViewProjectionMatrix, as ShadowMath::BlockVisible), the near plane [0xE839E0]
/// (billboard::CameraFrame::nearZ) and the screen of g_info_transform.
namespace openblack::field_of_view
{

/// The drawn camera's screen tests are graphics::region_on_screen's (Graphics/RegionOnScreen.h: ScreenView,
/// PointOnScreen = fn_0081F1D0, SphereOnScreen = LH3DBoundingBox::CheckRegionOnScreen 0x868C80)
using View = graphics::region_on_screen::ScreenView;

/// The main camera of openblack as the view (Locator::camera, Locator::windowing); false when there is none
[[nodiscard]] bool CurrentView(View& out);
/// POS_FIELD_OF_VIEW 012: inside the temple (g_game +0x205A28 == 1) false (pending, Edificios: openblack has no temple
/// interior state, taken as outside); else PointOnScreen
[[nodiscard]] bool PosInView(const glm::vec3& point);
/// GAME_THING_FIELD_OF_VIEW 011: nothing, or inside the temple: false. An Object (dynamic_cast, 0x6F81BA): its 3D
/// object's (+0x40) mesh (vt +0xF8, none -> false) bounding sphere through SphereOnScreen (fn_0081F1A0). Any other
/// GameThingWithPos: the point (x, GetAltitude + its +0x1C, z) of its MapCoords through PointOnScreen (0x6F81F4..
/// 0x6F822B). (approximate) openblack's "Object" is an entity with a Mesh: its box centre through the Transform and the
/// box's half diagonal x the largest scale as LH3DBoundingBox +0x1C (as Renderer.cpp's culling); without a Mesh its
/// Transform position (the land height plus the offset are already in it)
[[nodiscard]] bool ThingInView(entt::entity thing);

} // namespace openblack::field_of_view
