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

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/DrawList/ObjectOnScreen.h"
#include "Enums.h"

namespace openblack::particles
{
struct ObjectShower;
}

/// The script highlights (state in ECS/Components/ScriptHighlight.h): the challenge scrolls and the did-you-know
/// signs. A script makes one with CREATE_HIGHLIGHT 272, names it with HIGHLIGHT_PROPERTIES 334, lifts it with
/// SET_PROPERTY YPOS, lights it with SET_ACTIVE 255 and waits for GAME_THING_CLICKED 016 on it; SET_DRAW_HIGHLIGHT 306
/// hides the scrolls. The finders see it as SCRIPT_OBJECT_TYPE 37 with its info row as the sub-type, so FollowUs'
/// CALL_NEAR(HIGHLIGHT, 1, ...) at CHL L53068 / L53085 finds DidYouKnow's sign. See
/// docs/wiki/interface/script-highlights.md#script-highlights-1.
namespace openblack::ecs::script_highlight
{

/// The four script highlight info rows (info.dat order).
/// The CHL HIGHLIGHT_INFO enum of the later games (bronze, silver, gold, scoreboard...) does not match them
enum class Info : uint32_t
{
	Bronze = 0,     ///< "Script Hightlight": MSH_I_MINISCROLL / _ACTIVE, no particles
	DidYouKnow = 1, ///< "Script Did You Know Sign": MSH_O_INFO_SIGN (both), no particles
	Silver = 2,     ///< "Script Hightlight Silver inactive": MINISCROLL_SILVER / _ACTIVE, glints 100, active 103
	Gold = 3,       ///< "Script Hightlight gold inactive": MINISCROLL_GOLD / _ACTIVE, glints 99, active 102
};
constexpr uint32_t k_InfoCount = 4;
/// The save type
constexpr uint32_t k_SaveType = 0x3F;
/// The glow object's mesh (MSH_S_BLAST_CENTRE)
constexpr uint32_t k_GlowMesh = 0x20F;
/// The sprite's texture (".\data\textures\S_SpriteSheet3.raw", render mode 13), cell 0 (one sprite)
constexpr const char* k_SpriteTexture = "S_SpriteSheet3";
/// The sprite's render mode
constexpr uint32_t k_SpriteRenderMode = 13;

// ---- Pure rules (test_script_highlight) -----------------------------------------------------------------------------

/// The row index == 1
[[nodiscard]] constexpr bool IsDidYouKnowInfo(uint32_t index)
{
	return index == static_cast<uint32_t>(Info::DidYouKnow);
}
/// The in-game bank's "click on scroll" sample, heard as a highlight is activated
inline constexpr int k_ActivatedSample = 134;
/// The sample a highlight's activation plays, 2D and owned by the highlight: none for a "did you know" sign
[[nodiscard]] constexpr std::optional<int> ActivatedSample(uint32_t index)
{
	return IsDidYouKnowInfo(index) ? std::nullopt : std::optional<int> {k_ActivatedSample};
}
/// The sprite's alpha by row: 1 -> 0x32, 2 -> 0x96, 3 -> 0x64, else 0
[[nodiscard]] uint8_t SpriteAlpha(uint32_t index);
/// d = the truncated distance from the camera to the point, bound to [10, 30] (unsigned compares); a scroll's scale
/// is scale x d x (1 / 30), a did-you-know keeps its scale
[[nodiscard]] float DistanceScale(float scale, float distanceToCamera, bool didYouKnow);
/// For a turning highlight: the 3D object's Y angle + the frame's game ms x 0.00314159, then minus
/// trunc(a x (1 / 2 pi)) x 2 pi
[[nodiscard]] float SpinAngle(float angle, uint32_t frameMs);
/// The 3D object's Y angle and scale that one Draw sets
struct DrawPose
{
	float angle {0.0f};
	float scale {1.0f};
};
/// One drawn frame's pose, from the angle the last drawn frame left. `facesEye` (a lit did-you-know): the yaw from
/// `point` to `eye`, wrapped as SpinAngle wraps; otherwise SpinAngle by this frame's game ms only, so the frames that
/// were not drawn add nothing. The scale is DistanceScale of the highlight's own `scale` from `eye` to `point`, never
/// of the last pose's scale, so it does not build up
[[nodiscard]] DrawPose PoseOfDraw(float angle, bool facesEye, const glm::vec3& point, const glm::vec3& eye, float scale,
                                  bool didYouKnow, uint32_t frameMs);
/// The challenge ids whose scroll saves the game when it is clicked (GAME_THING_CLICKED): 0x38, 0x3B, 0x3C, 0x3D
[[nodiscard]] bool SavesGameWhenClicked(uint32_t scriptId);
/// A did-you-know with a script id; a scroll only while active and with a script id
[[nodiscard]] bool ValidToTap(bool didYouKnow, bool active, uint32_t scriptId);
/// The tap tooltip override: 0xEF2 when the info's OBJECT_TYPE is 1, else 0 (every row is 35: 0)
[[nodiscard]] uint32_t OverwriteTapToolTip(ObjectType infoType);
/// What a tap on a highlight does, in this order
struct TapPlan
{
	/// The help event a tap counts
	enum class Event
	{
		None,
		Reminder,
		ScriptActivate,
	};
	/// What follows the activation
	enum class Then
	{
		Nothing,
		Bubble,
		Reminder,
	};
	/// Counted for the local player's tap only: the reminder with a script id, else the script activation
	Event event {Event::None};
	/// Lit (SetActivated), only with a script id
	bool activates {false};
	/// The did-you-know's tap sound, for the local player's tap
	bool tapSound {false};
	/// With a script id: a did-you-know's bubble, else the challenge's reminder, whoever tapped
	Then then {Then::Nothing};
};
[[nodiscard]] TapPlan PlanTap(bool hasScriptId, bool didYouKnow, bool byLocalPlayer);

/// What one Draw does once the object draw list calls it
struct DrawStep
{
	/// How many times the glints are stepped: none after an early exit or without glints, two on the glints' first
	/// step, one otherwise
	uint32_t glintSteps {0};
	/// The on-screen flag the list keeps for the still passes
	bool onScreen {true};
};
/// An early exit (hidden, the scrolls hidden by the script, or no mesh) leaves the flag the list set before the Draw,
/// true, and steps nothing. Otherwise the glints step, and the flag is the mesh's on-screen test or `drawnBesides`
[[nodiscard]] DrawStep HighlightDrawStep(bool earlyExit, bool onScreen, bool drawnBesides, bool hasGlints,
                                         bool firstStepPending);
/// How many times one Draw steps the active effect: twice at its first step, once otherwise
[[nodiscard]] uint32_t OwnedEffectSteps(bool firstStepPending);
/// Whether the Draw draws something besides the mesh that keeps the highlight on screen for the list: the glow of an
/// active scroll, or the gold scroll's sparkles (row 3), which every gold scroll has from its creation
[[nodiscard]] bool DrawsBesidesTheMesh(uint32_t index, bool active);

/// The mesh's centre and radius as the Draw takes them, after it has placed the 3D object
struct MeshBounds
{
	glm::vec3 centre {0.0f};
	float radius {0.0f};
};
/// The box centre c through the object's matrix (rows m0..m8, translation m9..m11), each sum in the game's order:
/// x = ((cz m6 + cy m3) + cx m0) + m9, y = ((cx m1 + cz m7) + cy m4) + m10, z = ((cy m5 + cx m2) + cz m8) + m11; the
/// radius is |the 3x3 part times the half extents h|, the same orders without the translation, then
/// sqrt((X^2 + Z^2) + Y^2)
[[nodiscard]] MeshBounds DrawnMeshBounds(const affine::AffineMatrix& object, const glm::vec3& boxCentre,
                                         const glm::vec3& halfExtents);

/// The sparkles over a gold scroll (components::HighlightSparkles). The random draws are the caller's: the game passes
/// this computer's (LocalFloatRand, LocalRand), the tests fakes
using FloatRandFn = std::function<float(float)>;
using RandFn = std::function<uint32_t(int32_t)>;
/// The sparkles' texture (".\data\textures\S_SpriteSheet1.raw" with its alpha file, render mode 13)
constexpr const char* k_SparklesTexture = "S_SpriteSheet1";
/// Sparkle i starts at -400 i ms with the angle floatRand(2 pi), in order; then the cell offset is rand(0x20)
[[nodiscard]] components::HighlightSparkles MakeSparkles(const FloatRandFn& floatRand, const RandFn& rand);
/// Where a gold scroll's Draw puts its sparkles: over the mesh's centre by its radius
[[nodiscard]] glm::vec3 SparklesPoint(const glm::vec3& centre, float radius);
/// Each sparkle's clock moves on by the frame's game ms; one past 8000 goes back by 8000 and draws a new angle
/// floatRand(2 pi)
void StepSparkles(components::HighlightSparkles& sparkles, uint32_t frameMs, const FloatRandFn& floatRand);
/// One sparkle's sprite: none while its clock is below 0. With s = ms^2 x 1.5625e-8 it stands 20 s over the point,
/// its half width is 2 + (1.2 - 2) s, its alpha is the greatest alpha x ms x 0.0002 below 5000 ms, else x
/// (1 - (ms - 5000) x 0.000333333), truncated, and its cell is (ms / 50 + i + the cell offset) mod 32
struct SparkleSprite
{
	glm::vec3 position {0.0f};
	float size {1.0f};
	float angle {0.0f};
	uint8_t alpha {0};
	uint32_t cell {0};
};
[[nodiscard]] std::optional<SparkleSprite> SparkleSpriteOf(const components::HighlightSparkles& sparkles, size_t index);

/// The shared pulse of ProcessHighlights (phase, value, last value; all 0 from OnClearMap)
struct Pulse
{
	float phase {0.0f};
	float value {0.0f};
	float previous {0.0f};
};
/// phase += msPerTurn x 5 x 0.001, less 2 pi once when above; previous = value; value = (1 - cos phase) x 0.5
void StepPulse(Pulse& pulse, uint32_t msPerTurn);
/// lerp(previous, value, the turn fraction), bound to [0, 1], x 0.6 + 0.4
[[nodiscard]] float ActivePulse(const Pulse& pulse, float turnFraction);
/// The glow's ARGB, 0x14B4DCFF for Silver (row 2), else ((byte)trunc(ActivePulse) x 0x50) << 24
/// | 0xFFFF00 (literal: the pulse is 0.4..1, so the alpha is 0x50 only at 1.0, 0 otherwise)
[[nodiscard]] uint32_t ActiveGlowArgb(uint32_t index, float activePulse);

/// The did-you-know texts already read (the help bubble asks and adds): one list of at most 48 per DYK_CATEGORY 0..4.
/// (pending) where the original saves them (the help profile)
namespace did_you_know_read
{
[[nodiscard]] bool IsRead(uint32_t text, DykCategory category);
/// (inferred) an append when the list has room
void MarkRead(uint32_t text, DykCategory category);
/// The texts read of a category, in the order that the player read them; none for a category past the last
[[nodiscard]] std::span<const uint32_t> In(DykCategory category);
/// The sum of the five counts (0 -> the help bubble starts "FirstDYKExplained")
[[nodiscard]] uint32_t Total();
void Clear();
} // namespace did_you_know_read

// ---- The highlights ------------------------------------------------------------------------------------------------

/// A new highlight at the head of the list, with its script id, then its mesh, map cell and effects. entt::null for
/// an info index past the 4 rows (openblack: the original reads past the array)
entt::entity Create(const glm::vec3& position, uint32_t infoIndex, uint32_t scriptId, float yAngle, float scale);

[[nodiscard]] bool IsHighlight(entt::entity thing);
/// The info row (the script sub-type); script_type::k_NoSubtype for a thing that is not a highlight
[[nodiscard]] uint32_t InfoIndexOf(entt::entity thing);
[[nodiscard]] bool IsDidYouKnow(entt::entity thing);
[[nodiscard]] bool IsActive(entt::entity thing);
/// The script id (0 for a thing that is not a highlight)
[[nodiscard]] uint32_t ScriptIdOf(entt::entity thing);
/// The script id and the did-you-know category
void SetScriptId(entt::entity thing, uint32_t scriptId, DykCategory category);
/// SET_PROPERTY YPOS on a highlight: the draw height h is set, then the altitude = h and the 3D object moved there
void SetYPos(entt::entity thing, float height);
/// GET_PROPERTY YPOS: the altitude
[[nodiscard]] float GetYPos(entt::entity thing);
/// Lights or dims it: its mesh and its active effect (a scroll's sound is pending)
void SetActivated(entt::entity thing, bool on);

/// Each game turn (after the bookmarks and before the climate): the pulse, then each available highlight of the
/// list, from its head
void ProcessHighlights();
/// Every frame: the meshes of the highlights that are hidden (or scrolls the script hides) taken away, and given back
/// once they are not. openblack's stand-in for "no Draw": it is not gated by the object draw list
void SyncMeshes();
/// The highlight's Draw, which only the object draw list calls, on the frames it draws the highlight. After the early
/// exits, the pose (PoseOfDraw, with the view's eye) on the 3D object, the mesh's on-screen test, then the glints' step by the
/// frame's game time (power 1, enabled), then the active effect moved to the mesh's centre and stepped the same way, then a
/// gold scroll's sparkles placed over the centre and queued with the eye's distance. Returns the on-screen flag
/// (HighlightDrawStep). docs/wiki/rendering/object-draw-list.md#the-script-highlight-in-the-list
bool Draw(entt::entity thing, const draw_list::OnScreenView& view, uint32_t frameMs);
/// Once a frame, after the object draw list: the glints and the active effect of a highlight whose Draw did not step
/// them this frame are not drawn, as the original draws them only from that Draw
void EndDrawnFrame();
/// At the frame's end, when the blended things are queued for the draw (the renderer's PreDraw, before it collects the
/// particles): the sparkles the Draws queued this frame step by the frame's game ms, far from the eye first, and only
/// they are drawn this frame. The queue is then emptied for the next frame
void DrainSparkles(uint32_t frameMs);
/// An object's golden shower (particles::ObjectShower: the same sparkles, over the object an effect's atom is kept on) as
/// the particle draw finds it, before DrainSparkles, on the same queue. The draw makes the sparkles the first time
/// (MakeSparkles), then, while the shower has an object, gives them the alpha and stands them at the object's place
/// (ShowerPoint). In a Sorted effect that is done now and the shower queued at that place; in an effect drawn as one Z
/// object (`sorted` false) it is all done as the queue drains, the shower queued at the effect's origin
void QueueShower(const std::shared_ptr<particles::ObjectShower>& shower, uint8_t alpha, bool sorted, const glm::vec3& origin,
                 const glm::vec3& eye);
/// Where an object's golden shower stands: its 3D object's place (its Transform), else the point of its map coordinates
[[nodiscard]] std::optional<glm::vec3> ShowerPoint(entt::entity object);
/// The golden showers over objects drawn this frame (the debug readout)
[[nodiscard]] size_t DrawnShowers();
/// How many highlights' activations have played their click on scroll since the game started (the debug readout)
[[nodiscard]] uint32_t ActivationClicks();

/// What Draw adds around the mesh this frame, for the renderer and the interface (pending: nothing draws these yet)
struct DrawExtras
{
	bool drawn {false};                                ///< false: Draw returned before the mesh is drawn
	glm::vec3 centre {0.0f};                           ///< the mesh's centre in the world
	float radius {0.0f};                               ///< |the 3D object's matrix x the mesh's half extents|
	std::optional<graphics::billboard::Sprite> sprite; ///< Screen mode
	std::optional<glm::mat4> glow;                     ///< Mesh k_GlowMesh, an active scroll only
	uint32_t glowArgb {0};                             ///< its colour (ActiveGlowArgb)
	std::optional<float> clickRadius;                  ///< The invisible click sphere at the centre, r + 0.5
};
[[nodiscard]] DrawExtras ExtrasOf(entt::entity thing, const glm::vec3& eye);

/// A hand tap on it (through the tap packet); `byLocalPlayer`: the tapping interface is the local one and its player
/// the local player. Always true
bool InterfaceTap(entt::entity thing, bool byLocalPlayer);
/// ValidToTap of a thing's did-you-know info, active flag and script id; false for a thing that is not a highlight
[[nodiscard]] bool InterfaceValidToTap(entt::entity thing);

/// When it is deleted: out of the list, the two effects closed and its sparkles gone
void OnToBeDeleted(entt::entity thing);
/// When the map is cleared (after the bookmarks): the pulse back to 0; openblack also empties its list
void OnClearMap();
/// The highlights' list (head first)
[[nodiscard]] const std::vector<entt::entity>& All();

} // namespace openblack::ecs::script_highlight
