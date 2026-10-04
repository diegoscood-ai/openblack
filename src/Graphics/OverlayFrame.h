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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"
#include "Help/HelpTextDisplay.h"

// What the end of the frame draws over the scene, decided before the draw (Motor M2, dev\documentacion\motor\
// M2_snapshot.md section 2: the "overlays" part of the FrameSnapshot): the screen fade and the cinema bars
// (LH3DRender::FinishFrame 0x82F460 (e) / (h)), HelpText's callback 0x5CD020 (the box, the words, the click cue of
// HelpSystem::Draw3D 0x5C59D0) and the advisor spirits (fn_005C0700 and the trail callback fn_005C3850). Values only:
// Game.cpp fills it once a frame before DrawScene (FillOverlayFrame) and the Renderer reads it through
// DrawSceneDesc::overlay; nothing in the draw asks help::Get(), help::spirits::Get() or the Game's ScreenFade.
namespace openblack::graphics
{
class L3DMesh;

/// A world-space vertex of the WorldQuad program (position, uv, ABGR): the spirits' sprites and trails
struct SpiritQuadVertex
{
	float x, y, z, u, v;
	uint32_t abgr;
};

/// The script fade and the bars of ScreenFade (3D/ScreenFade.h) at this frame's resolution
struct FadeOverlay
{
	/// ScreenFade::GetColour, ARGB: fn_0086FEE0 (0x82F753) draws nothing at alpha 0
	uint32_t colour {0};
	/// fn_005C5780 = ScreenFade::LetterboxHeight(W, H, wide screen fraction): each bar's height, 0 = no bars
	int barPixels {0};
};

/// A cue drawn next to the dialogue box (today the text of Draw3D's KMIcon 0x5C59D0..0x5C5ACE; Mano's DrawKeyOrMouse
/// 0x447EA0 port will replace the text with the KMIcon and may add its own fields): where and how, already laid out
struct ScreenCue
{
	bool waiting {false};    ///< HelpSystem::IsWaitingForClick (+0x45EC): the cue exists only then
	float alpha {0.0f};      ///< HelpSystem::GetClickCueAlpha, its 1.0 s fade in ([0x92A444])
	std::u16string text;     ///< HELP_TEXT_TOOLTIP_07 (0xE79 "Continuar")
	float x {0.0f};          ///< the text's left edge in pixels (its right edge at W - 4, align 0x11)
	float y {0.0f};          ///< its top in pixels (centre fn_005C5970 - size / 2)
	float size {0.0f};       ///< j0's size, 2/3 of trunc((boxH + 1) / 3)
};

/// HelpText's callback 0x5CD020 for this frame
struct HelpTextOverlay
{
	/// help::Get() != nullptr (fn_005CCAB0's own gate g_game+0x250188 is not ported)
	bool active {false};
	/// HelpTextDisplay::Layout (fn_005C57B0, fn_005CCE60, fn_005CC760) at this frame's resolution and bar
	help::TextFrame text;
	ScreenCue cue;
};

/// One spirit as fn_005C0700 draws it this frame
struct SpiritOverlay
{
	int dude {0};
	/// true: the FinishFrame callback 0x5C2E10 in the FinishFrame3D view (in-world blend < 0.5, fn_005C3920); false:
	/// Draw3D 0x5C5B26 in the frame (>= 0.5)
	bool overlay {false};
	/// The LH3DMesh of the .hd's L3D0 (immutable once made; shared so the draw keeps it alive, not game state)
	std::shared_ptr<const L3DMesh> mesh;
	/// FinishAnimStack 0x5C0610: world space, the dude's scale included; empty when not posed
	std::vector<glm::mat4> bones;
	/// The instance's model matrix: the identity (the bones are in the world); its [0][3] the fade is the Renderer's
	glm::mat4 model {1.0f};
	/// obj +0x4C's alpha byte, +0x2C1C x 255 with the GoInvisible flicker (0x5C080F); 0 draws no model (0x5C0952)
	uint8_t alpha {255};
	/// SetColorSpecular vt+0x2C (Update1 step 9) before the land light: white and 0 out of the world
	uint32_t colour {0xFFFFFFFFu};
	uint32_t specular {0u};
	/// +0x35DC, the in-world blend (0..1), and the point (+0x3374's x, z) where fn_00801C90 samples the land light;
	/// set when the blend is not 0. The Renderer applies help::spirits::Runtime::WorldColour with its own table
	/// (the table of this frame is built inside DrawScene, UpdateLandLight; M2 step (b) moves it out)
	float inWorld {0.0f};
	std::optional<glm::vec2> landLightPoint;
	/// fn_0081E1F0's light 0 position for the overlay (0x5C0732..0x5C080C), nullopt in the frame. fn_0081E1F0 moves
	/// only the position: the light's colour stays model_light's
	std::optional<glm::vec3> lightPosition;
	/// The halo (good only, +0x34DC != 0) then the puff (while +0x2C18), LH3DSprite::Draw 0x840530 already turned into
	/// world triangles (three vertices each) with the camera of the spirits' Update; smoke.raw in mode 6
	std::vector<SpiritQuadVertex> sprites;
};

/// The intro light of PLAY_JC_SPECIAL 0 (ecs/IntroSpecial.h) this frame: fn_00828300's Z object (one entry of the
/// transparency queue, key |keyPoint - g_camera|^2 in SumOrder::XYZ, the light's head before this frame's step) and what
/// its callback 0x8283D0 draws there, in order: LH3DSprite::Draw 0x840530 (screen sprites with their near test) in the
/// material [0xD19C8C] (CreateMaterial(13, misc0.raw)), ZFUNC ALWAYS around them when depthAlways. Not a screen
/// overlay: it rides here as the frame's value copy (Motor M2), the Renderer submits it in the main view's queue
struct IntroLightOverlay
{
	bool active {false};
	glm::vec3 keyPoint {0.0f};
	bool depthAlways {false};
	std::vector<billboard::Sprite> sprites;
};

/// The overlays of one frame
struct OverlayFrame
{
	/// The resolution everything below was laid out for (the Renderer's Main view, ConfigureView); 0 x 0 = nothing
	int width {0};
	int height {0};
	FadeOverlay fade;
	HelpTextOverlay helpText;
	/// The dudes out of home, frame ones first (DrawSpirits(false) then (true)), each once
	std::vector<SpiritOverlay> spirits;
	/// fn_005C3850 -> fn_005B90C0: the rainbow trails, world triangles (three vertices each) of both dudes out of home
	/// and not in the world; rainbow.raw in mode 15
	std::vector<SpiritQuadVertex> spiritTrails;
	/// The intro light (ecs::intro_special::FillFrame)
	IntroLightOverlay introLight;
};

} // namespace openblack::graphics
