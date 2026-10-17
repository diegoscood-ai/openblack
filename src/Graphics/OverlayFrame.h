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

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"
#include "3D/OrientedText.h"
#include "Graphics/InputPromptFrame.h"
#include "Help/HelpTextDisplay.h"

// What the end of the frame draws over the scene, decided before the draw (the "overlays" part of the frame
// snapshot): the screen fade and the cinema bars, the help text (the box, the words, the click cue of the help
// system's draw) and the advisor spirits (and their trails). Values only: Game.cpp fills it once a frame before
// DrawScene (FillOverlayFrame) and the Renderer reads it through DrawSceneDesc::overlay; nothing in the draw asks
// help::Get(), Locator::advisorSystem or the script fade (Locator::cinematicDirectorSystem).
namespace openblack::graphics
{
class L3DMesh;

/// A world-space vertex of the WorldTextured program (position, uv, ABGR): the spirits' sprites and trails
struct SpiritQuadVertex
{
	float x, y, z, u, v;
	uint32_t abgr;
};

/// One building's row of the population read-out: its vertices in OverlayFrame::knockReadout
struct KnockRow
{
	entt::entity building {entt::null};
	uint32_t first {0};
	uint32_t count {0};
};

/// A number floating up from a point (ecs::components::FloatingNumber) as it is drawn this frame: in font j0 at 15
/// pixels, its text ending at its point's pixel, over a black copy one pixel left and one down
struct FloatingNumberOverlay
{
	/// The whole pixel its point falls in, y from the top
	glm::vec2 screen {0.0f};
	std::u16string text;
	/// Its colour, 0xAARRGGBB; its alpha is the one below
	uint32_t colour {0xFFFFFFFF};
	uint8_t alpha {0xFF};
};

/// The script fade and the cinema bars (Gui/ScriptFade.h, Gui/CinemaBars.h) at this frame's resolution
struct FadeOverlay
{
	/// ScriptFade::GetColour, ARGB: nothing is drawn at alpha 0
	uint32_t colour {0};
	/// CinemaBars::BarHeight(W, H, wide screen fraction): each bar's height, 0 = no bars
	int barPixels {0};
};

/// The help text for this frame
struct HelpTextOverlay
{
	/// help::Get() != nullptr and no full screen film open (video::IsPlaying)
	bool active {false};
	/// HelpTextDisplay::Layout at this frame's resolution and bar
	help::TextFrame text;
	/// The box behind the song's sung word (help::karaoke), drawn right before that word
	std::optional<help::karaoke::Box> songBox;
};

/// The game speed's words (paused, slow or fast) this frame: each letter a quad standing in the world near the top of
/// the screen, in white, in the help text's font j0. Drawn right after the dialogue texts, tested against the depth
/// left by the end of the frame's Z reset
struct SpeedIndicatorOverlay
{
	struct Letter
	{
		char16_t character;
		/// Along the text, down the letter and off the page
		openblack::TextFrame frame;
		/// Half the letter's width taken off along the text, so that the letter is centred on the frame's origin
		float offsetX;
	};
	std::vector<Letter> letters;
	/// The eased fade x 255, truncated
	uint8_t alpha {0};
};

/// One spirit as it is drawn this frame
struct SpiritOverlay
{
	int advisor {0};
	/// true: drawn at the end of the frame, over the scene (in-world blend < 0.5); false: drawn in the frame (>= 0.5)
	bool overlay {false};
	/// The mesh of the .hd's L3D0 (immutable once made; shared so the draw keeps it alive, not game state)
	std::shared_ptr<const L3DMesh> mesh;
	/// The posed bones: world space, the dude's scale included; empty when not posed
	std::vector<glm::mat4> bones;
	/// The instance's model matrix: the identity (the bones are in the world); its [0][3] the fade is the Renderer's
	glm::mat4 model {1.0f};
	/// The object colour's alpha byte, the visibility x 255 with the go-invisible flicker; 0 draws no model
	uint8_t alpha {255};
	/// The explicit colour and specular set before the land light: white and 0 out of the world
	uint32_t colour {0xFFFFFFFFu};
	uint32_t specular {0u};
	/// The in-world blend (0..1), and the point (x, z) where the land light is sampled; set when the blend is not 0.
	/// The Renderer applies help::spirits::Runtime::WorldColour with its own table (the table of this frame is built
	/// inside DrawScene, UpdateLandLight)
	float inWorld {0.0f};
	std::optional<glm::vec2> landLightPoint;
	/// Light 0's position for the overlay, nullopt in the frame. Only the position moves: the light's colour stays
	/// model_light's
	std::optional<glm::vec3> lightPosition;
	/// The pupils' texture moved over the eyes' vertices (vs_object's USE_ADVISOR_PUPILS), each eye's x its bone, y
	/// and z how far the texture moves across and down, w 1 once the face has set them (0: the mesh's own texture
	/// coordinates); and the texture's centre (xy), z 1 when the pupils' down runs along the eye's z (the evil advisor)
	std::array<glm::vec4, 2> pupils {};
	glm::vec4 pupilCentre {0.0f};
	/// The halo (good only) then the puff (while it lasts), sprites already turned into world triangles (three
	/// vertices each) with the camera of the spirits' Update; smoke.raw in mode 6
	std::vector<SpiritQuadVertex> sprites;
};

/// The intro light of PLAY_JC_SPECIAL 0 (Locator::introSystem) this frame: one entry of the transparency queue (key
/// |keyPoint - camera|^2 in SumOrder::XYZ, the light's head before this frame's step) and what its callback draws
/// there, in order: screen sprites with their near test in the misc0.raw additive material (mode 13), ZFUNC ALWAYS
/// around them when depthAlways. Not a screen overlay: it rides here as the frame's value copy, the Renderer submits it
/// in the main view's queue
struct IntroLightOverlay
{
	bool active {false};
	glm::vec3 keyPoint {0.0f};
	bool depthAlways {false};
	std::vector<billboard::Sprite> sprites;
};

/// The help system's did-you-know bubble as it is laid out this frame (FillOverlayFrame)
struct DidYouKnowOverlay
{
	bool active {false};
	/// The bubble's 9-slice and tail (help::BubbleShape), screen pixels, three vertices each
	std::vector<SpiritQuadVertex> shape;
	/// One line of the text: the run (its alpha the line's top) and the alpha of its bottom edge
	struct Line
	{
		help::TextRun run;
		uint8_t alphaBottom;
	};
	/// The text's lines (shadows first), in the bubble's font j0
	std::vector<Line> lines;
	/// The $g<n> gesture symbols: screen quads of S_Gesture0 / S_Gesture1 (n / 16), three vertices each
	std::array<std::vector<SpiritQuadVertex>, 2> gestures;
	/// The blinking scroll arrows (boxes in the atmosphere material), pixels and uv
	struct Arrow
	{
		float x0, y0, x1, y1, u0, v0, u1, v1;
	};
	std::vector<Arrow> arrows;
	/// The $m<n> icons: key or mouse icon draws (Renderer::DrawKeyOrMouseIn)
	struct KeyIcon
	{
		int32_t animType;
		int32_t clickType;
		int32_t row;
		std::u16string keyName;
		int32_t x;
		int32_t y;
		int32_t size;
		uint8_t alpha;
	};
	std::vector<KeyIcon> keyIcons;
	/// The draw alpha for them: the dark colour's alpha byte / 2, always 0x7F
	uint8_t arrowAlpha {0};
};

/// The overlays of one frame
struct OverlayFrame
{
	/// The resolution everything below was laid out for (the Renderer's Main view, ConfigureView); 0 x 0 = nothing
	int width {0};
	int height {0};
	FadeOverlay fade;
	HelpTextOverlay helpText;
	/// The game speed's words, none when nothing is shown
	SpeedIndicatorOverlay speedIndicator;
	/// The dudes out of home, frame ones first (DrawSpirits(false) then (true)), each once
	std::vector<SpiritOverlay> spirits;
	/// The rainbow trails, world triangles (three vertices each) of both dudes out of home
	/// and not in the world; rainbow.raw in mode 15
	std::vector<SpiritQuadVertex> spiritTrails;
	/// The read-out of how many people live in the knocked town's drawn buildings, world triangles (three vertices
	/// each) of little people in the plane of the screen; misc0.raw in mode 15
	std::vector<SpiritQuadVertex> knockReadout;
	/// Each building's row in knockReadout, in the order the object list drew them
	std::vector<KnockRow> knockRows;
	/// The key and mouse icons after the frame's two updates (newest first), the hand's point and the tick count, drawn
	/// twice by Renderer::DrawInputPrompts (pass 0 under the bars, pass 1 after the help text). The click cue of the help
	/// system's draw is one of them
	InputPromptFrame inputPrompts;
	/// The intro light (the intro system's FillFrame)
	IntroLightOverlay introLight;
	/// The did-you-know bubble (drawn by the help system)
	DidYouKnowOverlay didYouKnow;
	/// The floating numbers on the screen, the furthest from the camera first
	std::vector<FloatingNumberOverlay> floatingNumbers;
};

} // namespace openblack::graphics
