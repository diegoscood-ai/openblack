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
#include <functional>
#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "GestureBuffer.h"
#include "GestureMatch.h"
#include "GestureTemplates.h"

// What the interface looks for, and when: the circle that sizes a storm or a shield, the power-up gestures of a seed
// in the hand, the scribble that cancels, the spiral that opens the miracle selection, the selection's stages and R,
// "repeat the last miracle", and the square spiral that opens the leash picker. The worship code registers the worship
// icons as an IconProvider (SetIconProvider); with none, the selection never opens.
// Wiki: docs/wiki/magic/gestures.md#gestures-the-buffer-and-the-recogniser.

namespace openblack::magic::gestures
{
/// One entry per gesture, only for the HUD gesture icons
struct LookingFor
{
	/// Display order: 2 leash start, 8 spiral, 9 inverse spiral, 0xA repeat, 0xB selection stage, 0xC circle,
	/// 0xD power down, 0xE + k power-up k (0 = not looked for)
	int type {0};
	int seed {-1};
	int leash {-1};
	int powerUp {-1};
};
using LookingForArray = std::array<LookingFor, k_GestureCount>;

/// What the scribble and the power-up gestures did to the charging seed in the hand
struct PowerUpStageOutcome
{
	enum class Kind
	{
		None,        ///< nothing recognised: the rest of the pass runs
		PoweredDown, ///< SCRIBBLE with a power-up asked for: the power-up is called off and the seed stays (help event 20)
		/// SCRIBBLE with no power-up asked for: the seed is dropped back where it came from, with no influence test
		/// (help event 22)
		ShakenOut,
		PoweredUp, ///< a power-up gesture: the icon is asked to charge that level (help event 18)
	};
	Kind kind {Kind::None};
	int level {-1};           ///< The power-up level asked for
	Gesture gesture {k_None}; ///< Its gesture
};

/// The power-up stage of a charging seed in the hand. The scribble comes first: with a power-up asked for (`current`)
/// its gesture icon shows the power down with the seed, else none. Then each of the seed's power-up gestures but the
/// one asked for already, its icon showing the seed and the level; one recognised is acted on only for a seed made at a
/// worship icon
[[nodiscard]] PowerUpStageOutcome PowerUpStage(Gesture current, const std::array<Gesture, 3>& powerUps, int seedIndex,
                                               bool fromIcon, const std::function<bool(Gesture)>& recognise,
                                               LookingForArray& lf);

/// Whether R, the repeat of the last miracle, is looked for: not while the player's creature fights, with an R gesture
/// in the tables, the miracle selection not open with the hand ready, the hand ready, and the icons able to repeat
[[nodiscard]] constexpr bool RepeatLookedFor(bool fighting, Gesture repeat, bool selectionOpenAndReady, bool handReady,
                                             bool canRepeat)
{
	return !fighting && repeat != k_None && !selectionOpenAndReady && handReady && canRepeat;
}
/// R looked for: its gesture icon shows the repeat with the last miracle called up. Whether it was drawn, after which
/// the icons are asked to repeat that miracle
[[nodiscard]] bool RepeatStage(Gesture repeat, int lastSeedType, const std::function<bool(Gesture)>& recognise,
                               LookingForArray& lf);

/// The local player's worship-site spell icons, as the interface asks for them. The worship code implements it and
/// registers it with SetIconProvider.
class IconProvider
{
public:
	IconProvider() = default;
	IconProvider(const IconProvider&) = default;
	IconProvider(IconProvider&&) = default;
	IconProvider& operator=(const IconProvider&) = default;
	IconProvider& operator=(IconProvider&&) = default;
	virtual ~IconProvider() = default;

	/// An icon whose seed's selectionGesture is `category` can be requested
	[[nodiscard]] virtual bool AnyRequestableIconOfCategory(Gesture category) const = 0;
	/// Walks the icons of the player's worship sites: every icon whose seed's selectionGesture is `category` and that is
	/// valid for a spell request
	virtual void ForEachRequestableIcon(Gesture category, const std::function<void(int seedType)>& visit) const = 0;
	/// The icon of this seed type can be requested
	[[nodiscard]] virtual bool IconValidForRequest(int seedType) const = 0;
	/// IconValidForRequest, then the best icon for that seed requests its spell
	virtual void RequestSpell(int seedType) = 0;

	/// R may repeat the last miracle
	[[nodiscard]] virtual bool CanRepeat() const { return false; }
	/// The same request with the interface's last seed type
	virtual void RepeatLast(int /*lastSeedType*/) {}
	/// One of the player's icons is charging for this hand
	[[nodiscard]] virtual bool AnyIconChargingForHand() const { return false; }
	/// The largest charge fraction of those icons, for the hand effect's charge bands
	[[nodiscard]] virtual float MaxChargeFraction() const { return 0.0f; }
	/// Cancels the charge of the most charged icon charging for this hand
	virtual void CancelMostChargedIcon() {}
	/// The icon can charge that power-up level
	[[nodiscard]] virtual bool PowerUpAvailable(entt::entity /*icon*/, int /*powerUp*/) const { return false; }
	/// Sets the power-up level being charged (-1 = power down)
	virtual void SetPowerUpCharge(entt::entity /*icon*/, int /*powerUp*/) {}
};

/// nullptr (the default) = no worship icons. The gesture state owns the provider
void SetIconProvider(std::unique_ptr<IconProvider> provider);
[[nodiscard]] IconProvider* GetIconProvider();

/// The seed info fields the selection reads, so that it can run without the info tables in tests
struct SelectionTables
{
	std::function<Gesture(int seedType)> selectionGesture;
	std::function<Gesture(int seedType)> gesture;
	std::function<Gesture(int seedType)> gestureStage2;
	/// From info.dat's GSpellSeedInfo
	[[nodiscard]] static SelectionTables FromInfo();
};

/// The miracle selection
struct Selection
{
	static constexpr size_t k_Seeds = 30;
	std::array<std::array<Gesture, 3>, k_Seeds> seedStage {}; ///< {selection, gesture, stage 2} per seed type
	std::array<bool, k_Seeds> candidate {};
	std::array<bool, k_GestureCount> activeGesture {};
	uint32_t stage {0};
	bool open {false};
	float timer {0.0f};
	Gesture category {k_None};

	/// The requestable icons of the category become the candidates; stage 1.
	/// Returns whether any was found (then the caller sets showHeldGestureTrail).
	bool Open(Gesture category, const IconProvider& icons, const SelectionTables& tables);

	enum class Outcome
	{
		None,      ///< nothing recognised (0)
		Cancelled, ///< SCRIBBLE: Success(0), help event 0x15 (1)
		StageOk,   ///< a stage gesture: Success(1), help event 0x10; the next stage waits (0)
		Requested, ///< the last stage: Success(1), help events 0x10 and 0x11, spell requested (1)
	};
	/// The 30 s timeout (dt = the frame's game time, not while paused), SCRIBBLE, then the first
	/// active gesture 1..23 recognised narrows the candidates
	Outcome Stage(float dt, float timeOut, const std::function<bool(Gesture)>& recognise, IconProvider& icons,
	              LookingForArray* lookingFor = nullptr);
};

/// The leash of the player's creature, as the gestures see it
struct LeashHand
{
	std::optional<PlayerNames> holder;  ///< The player whose hand holds the leash, tied or not; none with no leash on
	bool tied {false};                  ///< The leash is tied to something, so it is not in the hand
	bool creature {false};              ///< The player has a creature
	std::array<bool, 3> known {};       ///< Whether the creature knows the evil, the rope and the good leash
	LeashType picked {LeashType::None}; ///< The leash it wears, or else the one to put on next
};

/// How many of the three leashes the creature knows
[[nodiscard]] int KnownLeashCount(const LeashHand& leash);

/// The info fields the leash picker reads, so that it can run without the info tables in tests
struct LeashSelectionTables
{
	float timeOut {0.0f};   ///< The picker closes by itself after this many seconds
	Gesture start {k_None}; ///< The gesture that opens it
	/// The gesture that picks each leash, by leash type (1 evil, 2 rope, 3 good)
	std::array<Gesture, 4> pickGestures {};
	/// From info.dat's GSpellSystemInfo
	[[nodiscard]] static LeashSelectionTables FromInfo();
};

/// Whether the leash picker looks for the square wave that starts the first land's secret script: only on the first
/// land, in a single player game that is not a playground
[[nodiscard]] constexpr bool SecretScriptLand(int32_t landNumber, bool multiplayer, bool playground)
{
	return landNumber == 1 && !multiplayer && !playground;
}

/// Whether the square spiral is looked for, by what the creature knows: the leash in this player's hand and more than
/// one leash known, or no leash on and one known. The caller also wants a creature, no fight, the leash picker and the
/// miracle selection closed, and a start gesture.
[[nodiscard]] bool LeashStartWanted(const LeashHand& leash, PlayerNames player);

/// The leash picker, which the square spiral opens to offer the creature's other leashes as gestures
struct LeashSelection
{
	bool open {false};
	bool pending {false}; ///< The square spiral sent the click that puts the leash on, and its turn has not come yet
	float timer {0.0f};

	/// The timer back to 0 and the picker open; the miracle selection closes
	void Open(Selection& selection);
	/// The picker stays open only while the player has a creature and either a leash put on by the square spiral is
	/// pending or the creature's leash is in this player's hand; otherwise it closes. Returns whether it is open
	bool Active(const LeashHand& leash, PlayerNames player);
	/// The square spiral was recognised. With no leash on, the click on the creature's packet is sent (returns true)
	/// and, with more than one leash known, the picker opens with that leash-on pending; with the leash in the hand and
	/// more than one known, it opens with nothing pending.
	bool Start(const LeashHand& leash, Selection& selection);
	/// The click on the creature's packet reached its turn: no leash-on is pending any more
	void LeashClickArrived();

	struct Outcome
	{
		enum class Kind
		{
			GoesOn,     ///< the rest of the pass runs
			NoCreature, ///< closed: the pass ends
			Cancelled,  ///< SCRIBBLE: the remove-from-hand visual, Success(0), closed, the pass ends
			Picked,     ///< a leash's gesture: Success(1), the leash sent to the next turn, closed, the pass ends
		};
		Kind kind {Kind::GoesOn};
		LeashType leash {LeashType::None}; ///< The leash picked
		/// The square wave was drawn: the pass wipes the gesture buffer, starts the secret script and goes on
		bool startSecretScript {false};
	};
	/// No creature closes it. The timer adds the frame's game time unless paused, and past the timeout the picker
	/// closes, though this pass still looks for its gestures. With the leash not in this player's hand nothing is
	/// looked for. Else SCRIBBLE; then, on the secret script's land (SecretScriptLand), the square wave, after which
	/// the buffer is wiped and nothing more is recognised this pass; then the gesture of each leash the creature knows
	/// but has not picked: evil, rope, good
	Outcome Stage(float dt, bool paused, LeashSelectionTables tables, const std::function<bool(Gesture)>& recognise,
	              const LeashHand& leash, PlayerNames player, bool secretScriptLand, LookingForArray* lf = nullptr);
};

/// The gesture stage the pass runs first, before the scribble's cancels
enum class GestureStage
{
	None,        ///< none: the rest of the pass runs
	PowerUp,     ///< the power-up gestures of the charging seed in the hand
	Selection,   ///< the miracle selection's next stage
	LeashPicker, ///< the leash picker
	/// the creature's fight with the move button held: the gesture path is wiped and the pass ends
	FightMoveHeld,
	/// the creature's fight: its own gestures (not ported: nothing of them is looked for), then the scribble
	Fight,
};

/// The creature's fight as the pass sees it
struct FightGestures
{
	bool fighting {false};   ///< The local player's creature fights a duel
	bool controlsUp {false}; ///< The interface is in its creature fight state
	bool moveHeld {false};   ///< The move button is held
};

/// The first of the pass's gesture stages that applies: the creature's fight, while it fights with the fight controls
/// up, which closes both the miracle selection and the leash picker unless the move button is held; else a charging
/// seed in the hand, which closes both; else the miracle selection, which stays open only while the hand is ready and
/// closes the leash picker; else the leash picker, while LeashSelection::Active keeps it
GestureStage FirstStage(Selection& selection, LeashSelection& picker, bool charging, bool ready, const LeashHand& leash,
                        PlayerNames player, const FightGestures& fight = {});
/// The local player's creature's fight now, from the fight system, the interface and the move button
[[nodiscard]] FightGestures CurrentFight();

/// The interface's gesture fields and the game's gesture system
struct InterfaceGestures
{
	GestureSystem system;
	Result result;
	Packet gesture; ///< Its size is the next cast's magnitude
	std::array<Gesture, 3> powerUpGestures {};
	Gesture currentPowerUpGesture {k_None};
	bool circlePending {false};
	glm::vec3 circlePosition {0.0f}; ///< World point
	float circleSize {0.0f};
	Gesture circleGesture {k_None};
	float circleTimer {0.0f}; ///< The circle is forgotten after 5 s
	Selection selection;
	LeashSelection leashSelection;
	float cooldown {0.0f}; ///< 0.4 s after each Success
	/// Set by SetupPowerUpGestures and by an opened selection (the gesture trail shows while a seed
	/// is held with it)
	bool showHeldGestureTrail {false};
	LookingForArray lookingFor {}; ///< The last table for the HUD gesture icons
	int lastSeedType {-1};         ///< The R repeat's seed
};

[[nodiscard]] InterfaceGestures& State();
/// A new game: everything cleared (a land change keeps it)
void Reset();

/// count = head = stationary = 0
void ClearBuffer();
/// A mouse move: nothing while paused or in the 0.4 s cooldown; the land
/// point under the cursor, or the newest sample's again if it is off the land
void FeedSample(glm::ivec2 mouse);
/// Clear, then the current mouse position again (BeginApplyOnRelease, SetupPowerUpGestures)
void ReseedBuffer();
/// BuildFromSystem, then MatchGesture into the game's result
bool Recognise(Gesture gesture);
/// Success -> the recognised sparkles and force feedback; always the buffer wiped and the
/// 0.4 s cooldown
void Success(bool success);

/// The seed's first turn in the hand: TrySetupPowerUpGestures, showHeldGestureTrail set, the buffer reseeded,
/// currentPowerUpGesture = the gesture of the seed's power-up level's magic
void SetupPowerUpGestures();
/// The circle forgotten; puGesture[k] = the seed's powerUpGestures[k] when the player has that level's magic
/// and the icon can charge it. False without a seed in the hand.
bool TrySetupPowerUpGestures();
/// A ready seed in the hand, not cast yet, from a worship icon
[[nodiscard]] bool HoldingChargingSeed();
/// currentPowerUpGesture while holding a charging seed, else 0
[[nodiscard]] Gesture PowerUpLevelGesture();
/// How much longer the circle drawn for the seed in the hand is remembered, in seconds; 0 without one
[[nodiscard]] float CircleSecondsLeft(const InterfaceGestures& state);
/// The game expects a gesture of the hand, so the gesture trail behind it shows: a charging seed in the hand, a seed
/// held with the gesture trail shown, the selection open with the hand ready, or a seed sized by a circle in the hand
[[nodiscard]] bool IsGesturing();

/// What the interface's power-up pass checks before anything else
struct PowerUpPassState
{
	bool insideCitadel {false};
	/// the hand's interface holds a locked select: the creature locked for the hand, or its locked object
	bool lockedSelect {false};
	bool interfaceActive {true};
	bool boxHasMouse {false}; ///< a box holds the mouse (the escape menu, a dialog)
};

/// The interface's power-up pass goes on only outside the citadel, with no locked select, an active interface and no
/// box holding the mouse. Otherwise it wipes the gesture buffer and stops
[[nodiscard]] constexpr bool PowerUpPassGoesOn(const PowerUpPassState& state)
{
	return !state.insideCitadel && !state.lockedSelect && state.interfaceActive && !state.boxHasMouse;
}
/// The pass's state now, from the game clock, the hand and the interface
[[nodiscard]] PowerUpPassState CurrentPassState();

/// A moving camera wipes the gesture path being drawn, unless a camera shake is going on (with the shakes switched on,
/// as they always are) or the camera's mode lets the player gesture while it moves
[[nodiscard]] constexpr bool CameraMoveWipesPath(bool cameraMoving, bool shaking, bool modeLetsGesture)
{
	return cameraMoving && !shaking && !modeLetsGesture;
}

/// dt = the frame's game time in seconds, 0 while paused.
/// It runs at the end of every interface action pass: once per frame and once per game turn.
void ProcessPowerUpSystem(float dt);

/// What the hand tells the interface; HandSpellSeed.cpp fills it every frame
struct HandStatus
{
	entt::entity heldSeed {entt::null}; ///< The SpellSeed in the hand
	bool holdingSomething {false};
	bool validToShake {false};  ///< The held object can be shaken out of the hand
	bool handReady {false};     ///< The hand is ready for an object
	bool inInfluence {false};   ///< The hand is inside the player's influence
	bool actionLatched {false}; ///< The action press not released yet
	bool paused {false};
	glm::ivec2 mouse {0};
	entt::entity cursorObject {entt::null}; ///< The object under the cursor (the interface's collided object)
	std::function<void()> forceDropHeld;    ///< Drops the held object
	std::function<void()> removeFromHandFx; ///< The hand effect's remove-from-hand visual
	/// Sends the apply of the held object to the object under the cursor, as the hand's own apply to an object
	std::function<void()> applyHeldToCursorObject;
};

/// The start of every power-up pass: a FIRE seed in the hand with a magic fireball under the cursor is applied to the
/// ball, with no button (the seed takes the ball in)
[[nodiscard]] constexpr bool FeedsFireBall(bool heldIsSeed, SpellSeedType heldType, bool cursorIsFireBall)
{
	return heldIsSeed && heldType == SpellSeedType::Fire && cursorIsFireBall;
}
void SetHandStatus(HandStatus status);
[[nodiscard]] const HandStatus& GetHandStatus();

/// What a scribble does when there is no charging seed in the hand
enum class ScribbleCancel
{
	None,       ///< nothing waits for a scribble
	ShakeHeld,  ///< the object in the hand is shaken out
	ShakeLeash, ///< the leash held in the hand is shaken off, at the next game turn
	CancelIcon, ///< the most charged icon's charge is cancelled
};

/// Which of the scribble's branches waits for it, the first that applies. An object in the hand comes first: it is
/// shaken out only inside the player's influence and when it can be shaken, and otherwise nothing waits. With an empty
/// hand, a leash this player holds and has not tied comes next, with no influence test; a tied leash stays. Last, an
/// icon charging for the hand.
[[nodiscard]] ScribbleCancel ScribbleCancelFor(const HandStatus& hand, bool iconCharging, PlayerNames player,
                                               const LeashHand& leash);
} // namespace openblack::magic::gestures
