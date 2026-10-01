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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureBuffer.h"
#include "GestureMatch.h"
#include "GestureTemplates.h"

// What the interface looks for, and when (GInterface::ProcessPowerUpSystem 0x5CF300 and its helpers 0x5CE420..0x5D0200):
// the circle that sizes a storm or a shield, the power-up gestures of a seed in the hand, the scribble that cancels,
// the spiral that opens the miracle selection, the selection's stages and R, "repeat the last miracle". The worship
// icons are M7's: they register an IconProvider (SetIconProvider); with none, the selection never opens.
// Wiki: docs/bw1-notes/magic.md, "Gestos".

namespace openblack::magic::gestures
{
/// LookingFor (16 bytes): one entry per gesture, only for the HUD icons (DisplayGesture::Update fn_0068ABA0)
struct LookingFor
{
	/// GESTURE_DISPLAY_ORDER: 2 leash start, 8 spiral, 9 inverse spiral, 0xA repeat, 0xB selection stage, 0xC circle,
	/// 0xD power down, 0xE + k power-up k (0 = not looked for)
	int type {0};
	int seed {-1};
	int leash {-1};
	int powerUp {-1};
};
using LookingForArray = std::array<LookingFor, k_GestureCount>;

/// The local player's worship-site spell icons, as the interface asks for them. M7 implements it (WorshipSpellIcon,
/// GPlayer 0x64BAB0..0x64BF40) and registers it with SetIconProvider.
class IconProvider
{
public:
	IconProvider() = default;
	IconProvider(const IconProvider&) = default;
	IconProvider(IconProvider&&) = default;
	IconProvider& operator=(const IconProvider&) = default;
	IconProvider& operator=(IconProvider&&) = default;
	virtual ~IconProvider() = default;

	/// GPlayer fn_0064BE40 (status, category) == 1: an icon whose seed's selectionGesture is `category` can be requested
	[[nodiscard]] virtual bool AnyRequestableIconOfCategory(Gesture category) const = 0;
	/// OpenSelection's walk fn_005CF040 (player +0xA48 -> the six worship sites +0x34 -> their icon list +0xE0, linked by
	/// +0x110): every icon whose seed's selectionGesture is `category` and ValidForRequestSpell(status, -1, 1) 0x77FBA0
	virtual void ForEachRequestableIcon(Gesture category, const std::function<void(int seedType)>& visit) const = 0;
	/// GPlayer fn_0064BEC0 (status, seed type)
	[[nodiscard]] virtual bool IconValidForRequest(int seedType) const = 0;
	/// Packet 0x25 (0x5DABA0): IconValidForRequest, FindBestSpellIconForSpellSeed 0x64BF40, then the icon's
	/// RequestSpell 0x77FB40(status, -1, 1)
	virtual void RequestSpell(int seedType) = 0;

	/// GPlayer fn_0064BD90 (status): R may repeat the last miracle
	[[nodiscard]] virtual bool CanRepeat() const { return false; }
	/// Packet 0x26 (0x5DABF0): the same request with the interface's last seed type
	virtual void RepeatLast(int /*lastSeedType*/) {}
	/// GPlayer fn_0064BAB0 (status): one of the player's icons is charging for this hand
	[[nodiscard]] virtual bool AnyIconChargingForHand() const { return false; }
	/// fn_0064BB10: the largest charge fraction of those icons (fn_0077FE80), for PHandFX's charge bands
	[[nodiscard]] virtual float MaxChargeFraction() const { return 0.0f; }
	/// Packet 0x1E (0x64BCC0): CancelCharge of the icon charging for this hand with the highest +0x138
	virtual void CancelMostChargedIcon() {}
	/// WorshipSpellIcon fn_0077FBF0 (status, pu): the icon can charge that power-up level
	[[nodiscard]] virtual bool PowerUpAvailable(entt::entity /*icon*/, int /*powerUp*/) const { return false; }
	/// Packet 0x6A (0x5DAC30): WorshipSpellIcon 0x77FC30(status, pu), the power-up level being charged (-1 = down)
	virtual void SetPowerUpCharge(entt::entity /*icon*/, int /*powerUp*/) {}
};

/// nullptr (the default) = no worship icons
void SetIconProvider(IconProvider* provider);
[[nodiscard]] IconProvider* GetIconProvider();

/// The seed info fields the selection reads (GSpellSeedInfo +0x100/+0x104/+0x108 exe), so that it can run without the
/// info tables in tests
struct SelectionTables
{
	std::function<Gesture(int seedType)> selectionGesture;
	std::function<Gesture(int seedType)> gesture;
	std::function<Gesture(int seedType)> gestureStage2;
	/// From info.dat's GSpellSeedInfo
	[[nodiscard]] static SelectionTables FromInfo();
};

/// The miracle selection (GInterface +0x1C0..+0x36C)
struct Selection
{
	static constexpr size_t k_Seeds = 30;
	std::array<std::array<Gesture, 3>, k_Seeds> seedStage {}; ///< +0x1C0 {selection, gesture, stage 2} per seed type
	std::array<bool, k_Seeds> candidate {};                  ///< +0x328
	std::array<bool, k_GestureCount> activeGesture {};       ///< +0x346
	uint32_t stage {0};                                      ///< +0x360
	bool open {false};                                       ///< +0x364
	float timer {0.0f};                                      ///< +0x368
	Gesture category {k_None};                               ///< +0x36C

	/// OpenSelection fn_005CF010 / fn_005CF040: the requestable icons of the category become the candidates; stage 1.
	/// Returns whether any was found (then +0x40 |= 8 in the caller).
	bool Open(Gesture category, const IconProvider& icons, const SelectionTables& tables);

	enum class Outcome
	{
		None,      ///< nothing recognised (0)
		Cancelled, ///< SCRIBBLE: Success(0), help 0x15 (1)
		StageOk,   ///< a stage gesture: Success(1), help 0x10; the next stage waits (0)
		Requested, ///< the last stage: Success(1), help 0x10 and 0x11, packet 0x25 sent (1)
	};
	/// SelectionStage 0x5CFAE0: the 30 s timeout (dt = the frame's game time, not while paused), SCRIBBLE, then the first
	/// active gesture 1..23 recognised narrows the candidates
	Outcome Stage(float dt, float timeOut, const std::function<bool(Gesture)>& recognise, IconProvider& icons,
	              LookingForArray* lookingFor = nullptr);
};

/// GInterface's gesture fields and the game's GestureSystem
struct InterfaceGestures
{
	GestureSystem system;                      ///< g_game +0x25006C
	Result result;                             ///< g_game +0x250070
	Packet gesture;                            ///< +0x1A4 m_Gesture (its size is the next cast's magnitude)
	std::array<Gesture, 3> powerUpGestures {}; ///< +0x16C
	Gesture currentPowerUpGesture {k_None};    ///< +0x178
	bool circlePending {false};                ///< +0x17C
	glm::vec3 circlePosition {0.0f};           ///< +0x180 world point
	float circleSize {0.0f};                   ///< +0x18C
	Gesture circleGesture {k_None};            ///< +0x190
	float circleTimer {0.0f};                  ///< +0x194 (5 s)
	Selection selection;
	float cooldown {0.0f};                     ///< +0x378 (0.4 s after each Success)
	/// m_Held (+0x40) bit 8: set by SetupPowerUpGestures and by an opened selection (the gesture trail shows while a seed
	/// is held with it)
	bool heldFlag8 {false};
	LookingForArray lookingFor {};             ///< the last table given to DisplayGesture::Update
	int lastSeedType {-1};                     ///< GInterfaceStatus fn_005DCA20 / fn_005DCA40: the R repeat's seed
};

[[nodiscard]] InterfaceGestures& State();
/// A land is loaded: everything cleared
void Reset();

/// count = head = stationary = 0
void ClearBuffer();
/// fn_005CEAD0 (a mouse-move message, msg 0 post 0x5D99E0): nothing while paused or in the 0.4 s cooldown; the land
/// point under the cursor, or the newest sample's again if it is off the land
void FeedSample(glm::ivec2 mouse);
/// fn_005D2770: clear, then the current mouse position again (BeginApplyOnRelease, SetupPowerUpGestures)
void ReseedBuffer();
/// fn_005CF220: BuildFromSystem, then MatchGesture into the game's result
bool Recognise(Gesture gesture);
/// fn_005CE420: success -> the recognised sparkles (fn_00689790) and immersion 3; always the buffer wiped and the
/// 0.4 s cooldown
void Success(bool success);

/// SetupPowerUpGestures 0x5CEE30 (the seed's first turn in the hand): the inner part, m_Held |= 8, the buffer reseeded,
/// currentPUGesture = the seed's level's GMagicInfo.gestureType
void SetupPowerUpGestures();
/// fn_005CEE80: the circle forgotten; puGesture[k] = the seed's powerUpGestures[k] when the player has that level's magic
/// and the icon can charge it. False without a seed in the hand.
bool SetupPowerUpGesturesInner();
/// fn_005CEF50 HoldingChargingSeed: a ready seed in the hand, not cast yet, from a worship icon
[[nodiscard]] bool HoldingChargingSeed();
/// fn_005CF270: currentPUGesture while holding a charging seed, else 0
[[nodiscard]] Gesture PowerUpLevelGesture();

/// GInterface::ProcessPowerUpSystem 0x5CF300; dt = g_game_time_inc x 0.001 (the frame's game time, 0 while paused).
/// It runs at the end of every InterfaceActionProcess: once per frame (ProcessFrameInputs) and once per game turn
/// (GInterface::Process from ProcessGameInputs).
void ProcessPowerUpSystem(float dt);

/// What the hand tells the interface (GInterfaceStatus / GInterface fields); HandSpellSeed.cpp fills it every frame
struct HandStatus
{
	entt::entity heldSeed {entt::null}; ///< fn_005D3880: the SpellSeed in the hand
	bool holdingSomething {false};      ///< status +0x90
	bool validToShake {false};          ///< held->ValidToShakeFromHand() (Object 0x636AA0: 1)
	bool handReady {false};             ///< GInterfaceStatus::IsHandReadyForObject 0x5DC890
	bool inInfluence {false};           ///< GInterface +0x48 m_InInfluence (fn_005D1120)
	bool actionLatched {false};         ///< m_Buttons & 0x200: the action press not released yet
	bool paused {false};                ///< g_game +0x59A4
	glm::ivec2 mouse {0};               ///< GInterface +0x428
	std::function<void()> forceDropHeld;    ///< GInterface::ForceDropHeld 0x5D4350
	std::function<void()> removeFromHandFx; ///< PHandFX::DoRemoveFromHandVisual 0x68CE90 (vt 4)
};
void SetHandStatus(HandStatus status);
[[nodiscard]] const HandStatus& GetHandStatus();
} // namespace openblack::magic::gestures
