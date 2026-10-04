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

#include <optional>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/SuperVillagerEyes.h"

namespace openblack::graphics
{
struct SuperVillagerFrame;
}

/// The intro family's high detail models (research dev\documentacion\intro\spec_hd_models.md). SET_HIGH_GRAPHICS_DETAIL
/// (op 290) makes a villager a SuperVillager (engine class, list g_first [0xEB9A08], 0x34 bytes): drawn by its own path
/// (fn_008254A0 -> fn_00825530) with the HD mesh of Data\MISC\Intro, a 300 ms clip cross-fade and a second yaw smoothing,
/// and mesh eyes (ECS/SuperVillagerEyes.h). THING_JC_SPECIAL (op 349) sets its flags.
///
/// openblack draws the body with the villager entity itself (RenderingSystem, the swapped Mesh); the fade and the yaw
/// stage are parameters of the villager's smooth drawing (components::SkeletalAnimation::crossFade*, DrawPosition::
/// follow*, ECS/Animations.h and ECS/MobileDrawing.h), set here every frame.
///
/// They live only while a script holds the wide screen (GLandscape::Draw 0x5E4B3A..0x5E4B93). With the mod
/// game.skip-intro "free start" SET_WIDESCREEN is skipped (CHLApi.cpp), so the family is released every frame: test and
/// take screenshots with --mod game.skip-intro=off.
namespace openblack::ecs::components
{
struct SuperVillager
{
	/// the eye type of op 290 (0 man, 1 woman or the animal trainer, 2 boy; -1 another mesh)
	int32_t eyeType {-1};
	/// +0x30: bit 1 drawn at the intro hand's grip (feature 7), bit 2 no smoothing and no fade (8, 9, 16, 17)
	uint32_t flags {0};
	/// +0x2C: the HD mesh (0: none, the thing's own high mesh is drawn)
	entt::id_type hdMesh {0};
	/// +0x20..+0x28: the meshes put back by Release. openblack draws one: the drawn Mesh id (or the hidden one)
	entt::id_type savedMesh {0};
	/// +0x1C
	std::optional<super_villager::eyes::Eyes> eyes;
	/// fn_00825400 0x82541D, this frame: LH3DBoundingBox::CheckRegionOnScreen of the drawn mesh's box passed. Only then
	/// fn_00825530 runs (0x825422 je 0x82543D): the fade, the yaw stage and the eyes (their Random draws included)
	bool onScreen {true};
};
} // namespace openblack::ecs::components

namespace openblack::ecs::super_villager
{
constexpr uint32_t k_FollowHand = 1; ///< +0x30 bit 1
constexpr uint32_t k_Snap = 2;       ///< +0x30 bit 2

/// GScript::SetHighGraphicsDetail 0x708CE0 after its POPs: on, the HD mesh of the thing's high mesh (MeshPack 501 / 498 /
/// 439 / 420 -> nors_man / nors_woman / nors_boy / sable) and fn_00825F20; off, fn_00825440 (Release)
void SetHighGraphicsDetail(entt::entity thing, bool on);
/// GScript::ThingJCSpecial 0x709000 after its POPs (object, feature, bool)
void ThingJcSpecial(entt::entity thing, int32_t feature, bool on);

/// fn_00825440 / SuperVillager::Release 0x826180: false when the thing is not one. The eyes go whatever the thing's
/// state (fn_00825E70 deletes the Eyes unconditionally, fn_00884570): the list entry keeps their ids, so a thing destroyed
/// while it is a SuperVillager (its component gone with it) leaves nothing drawn
bool Release(entt::entity thing);
/// Every one Released and the list emptied (GGame::ClearMap 0x552BE7, CleanGameForScriptReboot 0x6EBBC2, the script
/// camera release 0x6ECEA6, GLandscape::Draw 0x5E4B6D)
void ReleaseAll();
/// g_first [0xEB9A08] order: the newest first
[[nodiscard]] std::vector<entt::entity> List();

/// Once a frame before ECS/MobileDrawing: GLandscape::Draw 0x5E4B3A..0x5E4B93 (no script wide screen: all go; a thing
/// gone: it goes), the swim rings of 0x5E4C7D..0x5E4D4B (a swimmer, one clock for all, Random(0, 2 pi) every 1000 ms,
/// ecs::AddWaterRing), fn_00825400's on-screen test (SuperVillager::onScreen: LH3DBoundingBox::CheckRegionOnScreen
/// 0x868C80 = region_on_screen::SphereOnScreen) and this frame's parameters of the villagers' smooth drawing (frozen
/// off screen)
void Update();
/// Once a frame right after ECS/MobileDrawing, before the poses and the carried props: the feature-7 position of the
/// driver loop (GLandscape::Draw 0x5E4BC8..0x5E4BEE, after Villager::Draw 0x5E4BC2)
void FollowHand();
/// Once a frame after the poses and after the fish shoals (fn_008254A0 runs from fn_005E5CD0 0x5E6255, after the fish
/// 0x5E4B2B): fn_00883560 for each one on screen, in list order; the others' eyes hidden
void Draw(int32_t milliseconds);
/// What the draw needs of the list (Graphics/SuperVillagerFrame.h), filled once a frame before DrawScene (Game::Run,
/// next to FillOverlayFrame; Motor M2): the renderer reads only that copy, never the registry or the list.
/// - litByDefaultSun, fn_008254A0 0x8254C3..0x8254D1 / 0x82551F: every SuperVillager draw (the body, 0x825E1F, and the
///   eyes, 0x825E28) is lit by the default sun [0xEA1C88] (model_light::k_DefaultSun), not by the frame's light: the HD
///   meshes of the list and the eye meshes (the renderer's draws of those meshes take model_light::ScopedLight).
///   (approximate) a SuperVillager drawn with its own high mesh (no HD file) shares it with the other villagers: that
///   mesh keeps the frame light;
/// - swimmers, in list order: the ones animated "M_P_Swim2" (0xBF3598 compared at 0x5E4C07), which GLandscape::Draw
///   cuts under the water.
/// The containers keep their capacity across frames; with no SuperVillager both are just cleared
void FillFrame(graphics::SuperVillagerFrame& out);

/// fn_005DFCE0's grip of the intro hand while [0xD19C34] != 0 (nullopt else): Game.cpp sets it once a frame, before
/// Update, from ecs::intro_special::Grip()
void SetIntroHandGrip(std::optional<glm::vec3> grip);

/// What LH3DMesh::CreateFromHD(path, 0) (0x8067F0) gives, loaded once into the mesh manager: op 290's HD bodies and the
/// intro hand (ecs/IntroSpecial.h)
struct HdModel
{
	entt::id_type mesh {0};
	/// the EBone block (flag 0x200000 and a block, 0x826004 / 0x826122) and the skin, for the eyes and the hand's grip
	std::optional<eyes::HostModel> host;
};
/// Data\MISC\<relative>.l3d as the mesh "misc/<relative>" (once; null when the file cannot be read)
const HdModel* LoadHdModel(const std::string& relative, const std::string& name);

namespace testing
{
/// (openblack) a SuperVillager on `thing` with six bare eye entities (no files, no meshes) of that type, linked at the
/// head of the list as fn_00825F20 does: for the tests of the list's bookkeeping
void Adopt(entt::entity thing, int32_t eyeType);
} // namespace testing

} // namespace openblack::ecs::super_villager
