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

/// The help system's profile of the player: how often and how lately the player has done each of the things the help
/// keeps count of (tapped a scroll, turned the camera, ...). The scripts read it to judge whether the player has
/// learnt a control yet.
namespace openblack::help::profile
{

/// The things counted, numbered as the scripts number them; 0 is never asked for by a script
constexpr uint32_t k_EventCount = 49;
/// The world camera's controls: turning it (and which way: positive is the way the turn-right key turns it), tilting,
/// zooming, a double click on the land or on an object, flying to the temple or over the realm, and dragging it across
/// the land
constexpr uint32_t k_CameraTurn = 25;
constexpr uint32_t k_CameraTurnPositive = 26;
constexpr uint32_t k_CameraTurnNegative = 27;
constexpr uint32_t k_CameraTilt = 28;
constexpr uint32_t k_CameraZoom = 29;
constexpr uint32_t k_CameraDoubleClickLand = 30;
constexpr uint32_t k_CameraDoubleClickObject = 31;
constexpr uint32_t k_CameraFlyToTemple = 32;
constexpr uint32_t k_CameraDrag = 33;
/// The hand tapped a scroll or sign with a challenge or tip, or one without
constexpr uint32_t k_TapWithText = 34;
constexpr uint32_t k_TapWithoutText = 35;
/// The player's creature starts a move in a fight from its stance: a block, an attack (a blow or the special move, also
/// each turn that a blow waits to be let go), a miracle, or a step or other animation
constexpr uint32_t k_FightBlock = 36;
constexpr uint32_t k_FightAttack = 37;
constexpr uint32_t k_FightCast = 38;
constexpr uint32_t k_FightStep = 39;
/// Events that also count as another: 14 to 23 count as 24, 9 to 11 as 11, 1 to 42 as 43, and every one as 48
constexpr uint32_t k_AnyEvent = 48;

/// The profile's clock runs 100 ms a game turn while it is kept
constexpr uint32_t k_MillisecondsPerTurn = 100;
/// The latest times of an event kept, for its rate
constexpr uint32_t k_TimesKept = 64;
/// How quickly the smoothed rate follows the turns the event happens on
constexpr float k_RateFollow = 0.005f;

/// One event's count: its total, the times it last happened, and a smoothed share of the turns it happens on. It
/// counts at most once a turn.
class EventCount
{
public:
	/// It happens at the clock's time, unless it already has this turn
	void Trigger(uint32_t clock);
	/// The turn ends: the smoothed share follows whether it happened, and it may happen again
	void EndTurn();

	[[nodiscard]] uint32_t Total() const { return _total; }
	[[nodiscard]] float SmoothedRate() const { return _smoothedRate; }
	/// The seconds since it last happened, 0 if it never has. A clock behind its times (a game loaded from an earlier
	/// point) pulls them back to the clock and reads 0.
	[[nodiscard]] float SecondsSince(uint32_t clock);
	/// How many times a second it happened, over the times kept: the gaps between them over the time since the oldest.
	/// 0 with fewer than two kept or none of the clock passed since the oldest; a clock behind its times as above.
	[[nodiscard]] float PerSecond(uint32_t clock);

	/// All that the count keeps, as a player's profile stores it
	struct Kept
	{
		uint32_t total {0};
		float smoothedRate {0.0f};
		/// The slot of the next time to keep, and how many slots have a time
		uint32_t head {0};
		uint32_t kept {0};
		bool triggeredThisTurn {false};
		std::array<uint32_t, k_TimesKept> times {};
	};
	[[nodiscard]] Kept GetKept() const;
	/// The count as a profile stored it. A slot or a number of times past the last is kept to the slots there are.
	void SetKept(const Kept& kept);

private:
	void PullTimesBackTo(uint32_t clock);

	uint32_t _total {0};
	float _smoothedRate {0.0f};
	uint32_t _head {0};
	uint32_t _kept {0};
	bool _triggeredThisTurn {false};
	std::array<uint32_t, k_TimesKept> _times {};
};

/// Every event's count and the profile's clock
class HelpProfile
{
public:
	/// An event happens; with it the events it also counts as. Nothing for a number past the last.
	void Trigger(uint32_t event);
	/// A game turn ends: each event may happen again, and the clock moves on
	void EndTurn();

	[[nodiscard]] uint32_t Clock() const { return _clock; }
	[[nodiscard]] const EventCount& Count(uint32_t event) const { return _counts.at(event); }
	[[nodiscard]] EventCount& Count(uint32_t event) { return _counts.at(event); }

	/// Whether the scripts may ask about an event: 1 to 48
	[[nodiscard]] static constexpr bool ScriptMayAsk(int32_t event) { return event > 0 && event < k_EventCount; }

private:
	std::array<EventCount, k_EventCount> _counts {};
	uint32_t _clock {0};
};

} // namespace openblack::help::profile

/// The player's help statistics: the help profile and the counters of the camera help.
///
/// - The 49 counts and their clock are a help::profile::HelpProfile; this module adds what blocks the counting, the
///   looks at the land and the sky, and the camera help's own 12 counts.
/// - A trigger counts at most once a turn: the "triggered this turn" flag blocks it until Process clears it (once a
///   game turn, after the scripts and the help system), which also advances the accumulated time by 100 ms. So
///   GET_TOTAL_EVENTS(n) is "the turns in which the event happened".
/// - Nothing is counted while the game is paused or a script holds the wide screen.
/// - The counts belong to the player profile, not to the game: the original saves and loads the 49 accumulators to
///   "<user path>\helpstats.dat" with the profile. (not ported) openblack has no profiles: the counts start at 0 every
///   run.
namespace openblack::help_profile
{

/// HELP_EVENT_TYPE, by the original's names
enum class Event : int32_t
{
	Dummy = 0,
	HandMove = 1,
	PickUp = 2,
	Catch = 3,
	Throw = 4,
	Give = 5,
	Supply = 6,
	Sacrifice = 7,
	Tap = 8,
	CastSpell = 9,
	CastCreatureSpell = 10,
	CastAll = 11,
	GetSpell = 12,
	StopSpell = 13,
	RepeatGesture = 14,
	SelectGesture = 15,
	StageGesture = 16,
	GetSpellGesture = 17,
	PowerUpGesture = 18,
	GestureCreatureSpecial = 19,
	GestureCancelPowerUp = 20,
	GestureCancelSelect = 21,
	GestureCancelHeld = 22,
	GestureOnCast = 23,
	GestureTotal = 24,
	Rotate = 25,
	RotateCW = 26,
	RotateCCW = 27,
	Pitch = 28,
	Zoom = 29,
	DoubleClickPos = 30,
	DoubleClickObject = 31,
	ZoomToCitadel = 32,
	Drag = 33,
	Reminder = 34,
	ScriptActivate = 35,
	FightBlock = 36,
	FightAttack = 37,
	FightSpell = 38,
	FightStep = 39,
	AttachLeash = 40,
	DetachLeash = 41,
	HelpQuery = 42,
	AllInterface = 43,
	LookAtLand = 44,
	LookAtLandTooClose = 45,
	LookAtSky = 46,
	FocusLeash = 47,
	AllEvents = 48,
};
/// Why the camera help is called: the high byte picks the table, the low byte the entry
enum class CameraReason : int32_t
{
	InsideExclusionZone = 0x100, ///< inside an exclusion zone
	InsideInclusionZone = 0x101, ///< inside an inclusion zone
	HandTooFarAway = 0x102,
	HitGround = 0x200,
	HitExclusion = 0x201,
	HitAllowedAreaEdge = 0x202,
	HitCeiling = 0x203,
	Rotate = 0x300,            ///< -> Event::Rotate (25)
	RotateCW = 0x301,          ///< -> 26
	RotateCCW = 0x302,         ///< -> 27
	Pitch = 0x303,             ///< -> 28
	Zoom = 0x304,              ///< -> 29
	DoubleClickPos = 0x305,    ///< -> 30
	DoubleClickObject = 0x306, ///< -> 31
	ZoomToCitadel = 0x307,     ///< -> 32
	Drag = 0x308,              ///< -> 33
};
/// The player camera's input mask, as the camera update builds it
enum class CameraInput : uint32_t
{
	Keyboard = 0x01,         ///< any other camera input of the frame; a movement key alone only without the land gripped
	MouseButton = 0x02,      ///< the land gripped
	BothMouseButtons = 0x04, ///< both buttons held
	WheelSpin = 0x08,        ///< the wheel turned, or a zoom key stood for a notch of it
	WheelDown = 0x10,        ///< the middle button held: the turn round the mouse
};
/// Where a frame's camera input came from, as the player's camera reads its controls
struct CameraInputSources
{
	/// The wheel turned, or a zoom key stood for a notch of it
	bool wheel {false};
	/// The middle button held, with the feature that lets it turn the camera round the mouse
	bool middleButton {false};
	/// A turn, tilt or zoom input: the keys, the wheel, both buttons or the middle button
	bool turnTiltOrZoom {false};
	/// The movement keys move the camera over the land
	bool moveKeys {false};
	/// The land gripped, which the middle button and both buttons let go of
	bool landGripped {false};
	/// Both buttons held, with their feature
	bool bothButtons {false};
};
/// The input mask of the player's turn, tilt, zoom and drag reports for a frame's sources: WheelSpin, WheelDown, Keyboard
/// for any other input but the middle button (a movement key alone only without the land gripped), MouseButton and
/// BothMouseButtons
[[nodiscard]] uint32_t InputMask(const CameraInputSources& sources);
/// The reports a frame's turn, tilt and zoom of the player make, in the order the camera update makes them
struct PlayerCameraReports
{
	std::array<CameraReason, 4> reasons {};
	size_t count {0};
};
/// Zoom when the zoom is not 0; Rotate when |yaw| is above 0.01, then RotateCW for a positive turn or RotateCCW for a
/// negative one; Pitch when the pitch is not 0. Pure
[[nodiscard]] PlayerCameraReports ReportsOfPlayerCamera(float yaw, float pitch, float zoom);

/// What ProcessSpecialTriggers reads of the player's camera mode (nothing for another mode): whether the middle pixel of
/// the screen meets the land, the distance from the eye it heads for to that point (at least 10) and the pitch
struct CameraView
{
	bool screenCentreOnLand {false};
	float headingDistance {0.0f};
	float pitch {0.0f};
};
struct Queries
{
	/// The game is paused. Unset: game_clock::IsPaused()
	std::function<bool()> paused;
	/// A script holds the wide screen. Unset: help::Get()->IsScriptWideScreen() (false without a help system)
	std::function<bool()> scriptWideScreen;
	/// A box holds the mouse (interface_active::BoxHasMouse; the hand is not drawn while it is set). Unset: false
	std::function<bool()> processBlocked;
	/// The player's camera mode, nullopt while another mode (a script's, a path's) is current: the game gives it from
	/// its camera model's middle of the screen (CameraModel::GetScreenCentre). Unset: nullopt (events 44..46 never
	/// come)
	std::function<std::optional<CameraView>()> playerCamera;
};
void SetQueries(Queries queries);

/// Nothing paused or in a script's wide screen; the type's accumulator, then 14..23 also GestureTotal (24), 1..42
/// AllInterface (43), 9..11 CastAll (11) and every type AllEvents (48)
void Trigger(Event type);
/// Once a game turn: nothing paused, in a script's wide screen or with processBlocked; else ProcessSpecialTriggers,
/// the 49 EndTurn, accumulated time += 100 and the same EndTurn for the camera help's 12 accumulators
void Process();
/// With the player's camera mode: centre on the land -> LookAtLand (44), then heading distance < 15 and pitch > 0.55
/// -> LookAtLandTooClose (45); not on the land and pitch < -0.3 -> LookAtSky (46)
void ProcessSpecialTriggers();
/// The view of the player's camera mode as the queries give it (Queries::playerCamera); none while another mode is
/// current or with no query set
[[nodiscard]] std::optional<CameraView> PlayerCameraView();

/// The camera help callback (reason, inputs): 0x3nn -> Trigger(25 + nn) and, for each bit of `inputs`, the camera
/// help's input accumulator (5); 0x2nn -> the move accumulator nn (4); 0x1nn -> the exclusion accumulator nn (3).
/// The original's point argument is not read
void CameraHelpCallback(CameraReason reason, uint32_t inputs);
/// The camera help callback for each of ReportsOfPlayerCamera's reports, with the frame's input mask. From openblack's
/// DefaultWorldCameraModel, in every update that read the frame's controls: `yaw` is its turn input (turned into
/// radians as yaw x pi / width), `pitch` its tilt input (x 0.002) and `zoom` its zoom step, after the camera features
/// zeroed them, leaving out the edge drag's turn and the tilt drag's tilt (they report on their own) and the
/// self-tilting camera's tilt
void OnPlayerCameraMove(float yaw, float pitch, float zoom, uint32_t inputs);

/// GET_TOTAL_EVENTS 237 and the other two readers: nullopt for a type outside 1..48 (the script error "Invalid event",
/// the script gets 0.0)
[[nodiscard]] std::optional<float> TotalEvents(int32_t type);
/// GET_EVENTS_PER_SECOND 235: PerSecond
[[nodiscard]] std::optional<float> EventsPerSecond(int32_t type);
/// GET_TIME_SINCE 236: SecondsSince
[[nodiscard]] std::optional<float> TimeSince(int32_t type);

[[nodiscard]] const help::profile::EventCount& Get(Event type);
/// The 49 counts and their clock (the help profile system's Get)
[[nodiscard]] help::profile::HelpProfile& Profile();
/// The profile's accumulated time, ms
[[nodiscard]] uint32_t AccumulatedTime();
/// Every count back to none, then the camera help's 12. The accumulated time keeps going
void SetToZero();
/// The tests: SetToZero, AccumulatedTime 0 and no queries
void Reset();

} // namespace openblack::help_profile
