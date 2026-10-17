/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/TempleLight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureTattoo.h"

/// The rules of the tattoo editor, where the player drags the sixteen symbols onto the places on their creature's body
/// and off them again, with two colour pickers to colour them and a button to turn the view round the creature.
///
/// Everything here is pure: the tattoos being edited, what dropping and lifting a symbol does to them, the colour the
/// pickers make, which place the pointer is over, and how the view turns.
namespace openblack::creature_tattoo_editor
{
/// The tattoos as the editor changes them, and as they were when it opened, which Cancel puts back
struct Session
{
	creature_tattoo::Slots slots {};
	creature_tattoo::Slots opened {};
	/// Whether the player has put a tattoo on or lifted one off since the editor opened (or since being warned)
	bool changed {false};
};

[[nodiscard]] Session Open(const creature_tattoo::Slots& slots);

/// A symbol let go of over a place on the body, or over none. Over a place it goes in the slot that already holds that
/// symbol there (only its colour changes), else the first empty slot, else the slot holding something else there; when
/// every slot holds another place's tattoo nothing changes. Over a place the session counts as changed either way.
/// True when the symbol went on.
bool Drop(Session& session, uint8_t design, const glm::u8vec3& colour, std::optional<uint8_t> site);

/// Pressing on a place lifts the tattoo there off, the last slot first: the slot is emptied, keeping its symbol, and
/// the tattoo that came off is returned, for the player to drag on to another place or away. None when the place has
/// no tattoo.
std::optional<creature_tattoo::Slot> Lift(Session& session, uint8_t site);

/// What OK does: in a network game, once a change is made, it tells the player that others see the change only once
/// they leave the temple, and answering that clears the change so that OK then closes; otherwise the editor closes
enum class Accept : uint8_t
{
	Close,
	Warn,
};
[[nodiscard]] Accept Ok(const Session& session, bool networkGame);

/// The colour pickers' controls, in the dialogs' 800 by 600 space: the palette at the left and the brightness bar at
/// the right, each 52 wide and 519 high
struct PickerRect
{
	glm::ivec2 min;
	glm::ivec2 max;
};
constexpr PickerRect k_PaletteRect {.min = {5, 35}, .max = {57, 554}};
constexpr PickerRect k_BrightnessRect {.min = {704, 35}, .max = {756, 554}};

/// The palette's column and row under a point of the palette control. They span the whole control, wider than the
/// strip of colours drawn in its middle, and the edges are held to the first and last.
[[nodiscard]] glm::uvec2 PaletteCell(glm::ivec2 point, const PickerRect& rect = k_PaletteRect);

/// How far down a picker a point is, 0 at the top to 1 at the bottom
[[nodiscard]] float SliderPosition(int y, const PickerRect& rect);

/// A place on the body as the view shows it this frame
struct SiteOnScreen
{
	uint8_t site {0};
	/// Where it is on the screen, in pixels
	glm::ivec2 screen {0};
	/// The cosine between the way its surface faces and the way to the camera
	float facingCamera {0.0f};
};

/// A place counts while its surface is turned no further from the camera than a little past side on (a cosine above
/// -0.2), and only within 64 pixels of the pointer
constexpr float k_FacingLimit = -0.2f;
constexpr int k_PickDistanceSquared = 64 * 64;

/// The place nearest the pointer, in screen pixels, of those that count
[[nodiscard]] std::optional<uint8_t> SiteUnderPointer(std::span<const SiteOnScreen> sites, glm::ivec2 pointer);

/// A place on the body as the frame's pose has it: where it is in the world, and the way into the body there, of any
/// length
struct SitePoint
{
	uint8_t site {0};
	glm::vec3 position {0.0f};
	glm::vec3 inwardNormal {0.0f};
};

/// Where a point of the world falls on the screen, in pixels from the top left, through a perspective transform whose
/// w is the depth along the view: x is (x / w + 1) times half the width and y (1 - y / w) times half the height, each
/// cut towards zero. A point off the screen keeps its pixels; one nearer than the near plane has none.
[[nodiscard]] std::optional<glm::ivec2> ProjectToPixels(const glm::mat4& worldToClip, float nearPlane, glm::ivec2 screenSize,
                                                        const glm::vec3& point);

/// The view the places are seen in
struct SiteView
{
	glm::vec3 eye {0.0f};
	glm::mat4 worldToClip {1.0f};
	float nearPlane {0.0f};
	glm::ivec2 screenSize {0};
};

/// The places as a view shows them, for SiteUnderPointer: each with its pixels and the cosine between the way into
/// the body there and the way from the eye to it (a vector of no length is left as it is). Only a place nearer than
/// the near plane is left out; one off the screen, or turned away, stays.
[[nodiscard]] std::vector<SiteOnScreen> SitesOnScreen(std::span<const SitePoint> points, const SiteView& view);

/// A vertex of a mesh as the search for a place's triangle reads it: where it is on its skin, 0 to 1 across, and the bone
/// that moves it
struct TexturedVertex
{
	glm::vec2 uv {0.0f};
	uint32_t bone {0};
};

/// A primitive of a mesh: which of the mesh's skins it is painted with, in the order the mesh lists them (none when its
/// skin is not one of them), its vertices and its triangles, three of its vertices each
struct TexturedPrimitive
{
	std::optional<uint8_t> skin;
	std::span<const TexturedVertex> vertices;
	std::span<const std::array<uint16_t, 3>> triangles;
};

/// The triangle of the mesh a place on the body is on, and where on it: u of the way from the first vertex to the
/// second and v of the way to the third
struct SiteTriangle
{
	/// Counted in the primitives searched
	size_t primitive {0};
	/// Counted in the primitive
	std::array<uint16_t, 3> vertices {};
	std::array<uint32_t, 3> bones {};
	float u {0.0f};
	float v {0.0f};
};

/// A triangle whose texture coordinates are this close to a line (twice its area on the skin) holds no place
constexpr float k_FlatOnSkin = 1e-4f;

/// The triangle a place is on: the first, in the mesh's order, of the primitives painted with the place's skin whose
/// texture coordinates hold the place's centre, edges and corners included. Triangles flat on the skin are passed
/// over. None when no triangle holds it. The pose plays no part: the same triangle is found whatever the body does.
[[nodiscard]] std::optional<SiteTriangle> FindSiteTriangle(std::span<const TexturedPrimitive> primitives,
                                                           const creature_tattoo::Site& site);

/// The view of the creature: the camera's eye and the point it looks at, and how fast it is turning round that point
/// and tipping up and down
struct Orbit
{
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	float turnSpeed {0.0f};
	float tipSpeed {0.0f};
};

/// The view the editor has of its own, as from the main menu
constexpr glm::vec3 k_MenuEye {0.0f, 20.0f, -30.0f};
constexpr glm::vec3 k_MenuFocus {0.0f, 7.0f, 0.0f};

/// Where the creature stands in the Creature Cave: on the line from the first of the temple's movement places to the
/// third, as far along it as two less its size, halved, held between none and all of it. A coin tossed as the cave is
/// set up, which counts only for a creature smaller than 1.25, takes it twice as far from the middle towards the first.
[[nodiscard]] glm::vec3 CavePlace(const glm::vec3& first, const glm::vec3& third, float size, bool coin);
constexpr float k_CaveCoinBelowSize = 1.25f;

/// What the Creature Cave's copy of the creature does at rest, chosen as the room starts: it sleeps when the coin falls
/// one way (for a creature smaller than 1.25 only), else it sits, and a second toss gives the rest a flag of its own
struct CopyRest
{
	bool sleeping {false};
	bool restFlag {false};
};
/// The draws the copy makes as it is set up, in their order on the game's local stream (`localRand(n)`, 0 to n - 1):
/// the coin, kept only below a size of 1.25; a first moment of the copy's time; the rest's flag; and a second moment
/// once it faces the fire. In each moment the sparkles of a good (alignment over 0.2: three of six draws) or evil (under
/// -0.2: three of five draws) creature draw, though openblack does not show them yet. `bones` is how many bones the body
/// has, which one draw of each sparkle picks from
[[nodiscard]] CopyRest DrawCopyRest(float size, float alignment, int32_t bones,
                                    const std::function<uint32_t(int32_t)>& localRand);

/// The way the copy faces as the room starts: towards the fire from where it stands, kept within half a turn either way
[[nodiscard]] float CopyHeading(const glm::vec3& place, const glm::vec3& fire);

/// The draws of one moment of the copy's time on the game's local stream: the sparkles of a good or an evil creature,
/// as DrawCopyRest makes them in each of its moments
void DrawCopyMoment(float alignment, int32_t bones, const std::function<uint32_t(int32_t)>& localRand);

/// The copy moves on by the real milliseconds of the frame, which do not stop while the world is paused in the temple,
/// held to a fifth of a second
constexpr uint32_t k_CopyMaxFrameMs = 200;
[[nodiscard]] uint32_t CopyFrameMs(uint32_t frameRealMs);

/// The animations of the copy's rest, as the spec file numbers them: the start, loop and end around the sleep's loop
/// when it sleeps, else around the sit's
[[nodiscard]] std::array<size_t, 3> CopyRestAnimations(bool sleeping);

/// Each frame the room is drawn, before the copy moves on: sitting, it starts a yawn one time in a thousand, a draw on
/// the game's local stream. Nothing is drawn while it sleeps, nor when the room is drawn in the main room's reflection
[[nodiscard]] bool CopyStartsYawn(bool sleeping, bool reflection, const std::function<uint32_t(int32_t)>& localRand);

/// The colour the copy is drawn in, 0xAARRGGBB: a light grey in the temple's light (`citadelColour`: each channel, the
/// alpha too, times the light's over 256), then faded with its depth from the camera as the temple fades. It is drawn
/// with no specular
constexpr uint32_t k_CopyColour = 0xFFE0E0E0u;
[[nodiscard]] uint32_t CopyColour(uint32_t citadelColour, float depth, const TempleFade& fade);

/// The copy turns on the spot to a heading. It plays two of its spins blended, the 0, 90 and 180 degree turns one way
/// (animations 6, 7 and 8) for a turn the heading's way round, the other way's (3, 4 and 5) otherwise: the first two up
/// to a quarter turn, the last two beyond, blended by how far between their angles the turn is. Its heading stays as it
/// was while they play and takes the new one at their end. A species without the side's three spins turns its heading
/// round over its stand's length instead.
struct CopyTurn
{
	float target {0.0f};
	struct Spins
	{
		size_t from;
		size_t to;
		/// How far from the first to the second
		float weight;
		/// The blend's length, between the two spins', to the nearest millisecond
		int32_t durationMs;
	};
	std::optional<Spins> spins;
	/// How far through it is, in the animations' milliseconds
	int32_t timeMs {0};
};
/// The first spins turning one way and the other
constexpr size_t k_CopySpinsAround = 6;
constexpr size_t k_CopySpinsBack = 3;
/// The turn from `heading` to `target`, by the species' animations' lengths in milliseconds (none for one it lacks)
[[nodiscard]] CopyTurn StartCopyTurn(float heading, float target,
                                     const std::function<std::optional<int32_t>(size_t)>& durationMs);
/// One moment of the copy's time while it turns, `step` milliseconds of its animations: once its heading is within
/// 1e-4 of the turn's it takes it and the turn is over; else the spins move on, the heading taking the turn's at their
/// end, or without spins the heading comes round by the share of the stand's length (`standMs`) gone so far of what is
/// left
void StepCopyTurn(std::optional<CopyTurn>& turn, float& heading, int32_t step, int32_t standMs);
/// The milliseconds the copy's animations move on in a moment of its time: a bigger body plays more slowly, by
/// 1.6 - size times 0.425, cut to whole milliseconds
[[nodiscard]] int32_t CopyAnimationStep(float size, uint32_t milliseconds);

/// What the creature's room keeps of its copy from one frame to the next, for the whole game: a turn that waits for
/// the body to be free and the way it goes, since when a mouse button has been held with the editor open, whether the
/// editor had the copy last frame, and the editor's wait for the copy's next idle action and how many it has had
struct CopyLife
{
	bool turnWanted {false};
	float turnHeading {0.0f};
	uint32_t heldSince {0};
	bool editorHadCopy {false};
	int32_t idleMs {0};
	int32_t idles {0};
};
/// What the copy's frame in the room goes by
struct CopyFrame
{
	/// The camera is on the copy with the tattoo editor open
	bool editorOpen {false};
	/// It rests asleep rather than sitting (the coin tossed as the room started)
	bool sleeping {false};
	float heading {0.0f};
	bool turning {false};
	/// Where it stands, the fire, and the editor's eye and the point it looks at
	glm::vec3 place {0.0f};
	glm::vec3 fire {0.0f};
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	/// A mouse button is held, and the point under the pointer three units into the screen; the left button is held
	bool buttonHeld {false};
	glm::vec3 pointer {0.0f};
	bool leftHeld {false};
	/// The system's milliseconds, and the frame's
	uint32_t ticks {0};
	int32_t frameMs {0};
};
/// While the editor is open the copy has an idle action every 20 to 40 seconds: a show-off, a scratch or a yawn, and
/// from the tenth on it sits down
constexpr int32_t k_CopyIdleWaitMs = 20000;
constexpr size_t k_CopyIdleShowOff = 65;
constexpr size_t k_CopyIdleScratch = 60;
constexpr int32_t k_CopyIdlesBeforeSitting = 10;
/// While the editor is open it looks at the pointer for the first second and a half of a press, three units into the
/// screen
constexpr uint32_t k_CopyLooksForMs = 1500;
constexpr float k_CopyLookDepth = 3.0f;
/// Once the editor is closed it rests when it faces the fire this closely, in radians, and else turns to it
constexpr float k_CopyFacingWithin = 0.01f;
/// What the frame did that the copy keeps: the flag a rest that started was given, and a turn to start
struct CopyStepped
{
	std::optional<bool> restFlag;
	std::optional<float> turn;
};
/// The copy's frame in the room, after it was drawn. With the editor open: the frame the editor first has it, its rest
/// ends and a turn to face the editor's eye is wanted, and the editor being shown draws the wait for an idle action
/// and starts their count again; when the wait for an idle action has run out by the frame's
/// milliseconds, a draw of 3 on the game's local stream picks the show-off or the scratch (each with a draw of 2 for
/// its flag, played once only while the body plays nothing) or the yawn, and from the tenth on, with the body free, it
/// sits (a draw of 2 for the flag), then the next wait is drawn; the left button held draws the wait again, starts the
/// count again and ends its rest; while a button is held it looks at the pointer for 1.5 s of the system's time, else
/// it looks nowhere. With the editor closed: facing the fire within 0.01 it rests again, a draw of
/// 2 on the game's local stream each such frame for the rest's flag (the rest starts only while the body plays
/// nothing and the species has its three animations), and looks at the fire sitting; else its rest ends and a turn to
/// face the fire is wanted. A turn wanted starts once the body plays nothing and is not turning.
[[nodiscard]] CopyStepped CopyStep(CopyLife& life, creature_layers::BodyAction& body, creature_layers::GestureLayer& gesture,
                                   std::optional<glm::vec3>& lookAt, const CopyFrame& frame,
                                   const std::function<bool(size_t)>& hasAnimation,
                                   const std::function<uint32_t(int32_t)>& localRand);

/// The Creature Cave's view of its creature as the player goes in: looking at a point half the creature's reach above
/// its feet, from one and a half reaches away and three quarters of one up, the reach being 15 times its scale held
/// between 1 and 20
[[nodiscard]] Orbit CaveView(const glm::vec3& creature, float scale);

/// Holding the rotation button down and moving away from where it was pressed speeds the turning up for as long as it
/// is held: sideways turns it round, up and down tips it, by how far from that point the pointer is
void Steer(Orbit& orbit, glm::ivec2 grab, glm::ivec2 pointer, float milliseconds);

/// Every frame the view turns round its focus and tips up or down by its speeds, staying between an eighth of and as
/// high above the focus as it is far from it across the ground; both speeds die away
void Turn(Orbit& orbit, float milliseconds);
} // namespace openblack::creature_tattoo_editor
