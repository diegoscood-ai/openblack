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

#include <glm/vec3.hpp>

#include "Enums.h"
#include "LeftClickCapture.h"
#include "MiraclesModel.h"
#include "Particles/ParticleEffect.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// Shows the miracle tables (costs, timers and seeds of every magic type), and starts any particle type at the hand or
/// where the camera looks. Its Running tab casts any miracle as any player, at any power-up level and charge: where
/// the camera looks, on the land under a click, or on the thing nearest a click. It makes a one-shot orb, a seed in the
/// hand or a miracle dispenser of the chosen miracle where the camera looks, at the next click on the land or under the
/// hand, only on its button. It shows the seed in the hand and its power-up, the villagers' reactions to the miracles,
/// and switches infinite prayer power on at every worship site. Closed with every switch off, the window changes
/// nothing; infinite prayer power stays on with the window closed until it is turned off.
class Magic final: public Window
{
public:
	Magic() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void UpdateAlways() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;
	[[nodiscard]] bool TakesEvent(const SDL_Event& event) const noexcept override;

private:
	/// What a left click on the land does while the window is open
	enum class ClickCast : uint8_t
	{
		Off,
		OnLand,
		OnNearestThing,
	};

	/// Where a dispenser from the window goes
	enum class DispenserPlace : uint8_t
	{
		CameraFocus,
		NextClick,
		Hand,
	};

	void DrawSelected() noexcept;
	void DrawTable() noexcept;
	void DrawParticles() noexcept;
	void DrawRunningEffects() noexcept;
	/// What the last frame's particles drew, by kind
	void DrawParticleStats() const noexcept;
	/// Where a started effect goes: the hand, or what the camera looks at, on the land
	[[nodiscard]] glm::vec3 SpawnPoint() const noexcept;
	/// Queues a belief sprite where the camera looks, and shows how many are queued
	void DrawBeliefSprites() noexcept;
	/// Gives an effect the creatures and villagers nearest its origin to act on
	void GiveNearestTargets(uint32_t effect, const glm::vec3& origin) const noexcept;
	/// The started effects given a time close down when it is up
	void UpdateTimedEffects(float seconds) noexcept;

	// The Running tab
	/// The miracle chosen, how it is cast and made, the hand's seed, prayer power, and what is running
	void DrawMiracles() noexcept;
	void DrawChoice() noexcept;
	void DrawCast() noexcept;
	void DrawDispenser() noexcept;
	void DrawHand() noexcept;
	void DrawPrayer() noexcept;
	/// Each worship site's worshippers: dancing, at the site, hungry, eating at the site, hiding; its pot's food, the food
	/// on its way and the food it wants from its towns
	void DrawWorshippers() noexcept;
	/// Each worship icon charging or lit by its charge: its store, the colour it is drawn with and the one set for the
	/// next frame, its sheet of light's charge and its pulse ring
	void DrawIconCharges() noexcept;
	void DrawReactions() noexcept;
	/// The reaction the turn's walk offered last, read only
	void DrawReactionTurn() noexcept;
	/// The running miracles, the dispensers and the creatures' spells
	void DrawRunning() noexcept;

	void CastAt(const miracles::Target& target) noexcept;
	void CastAtClick() noexcept;
	void CreateDispenserAt(glm::vec3 point) noexcept;
	void CreateDispenserAtClick() noexcept;
	/// The land under the hand, if the hand is on the land
	[[nodiscard]] static std::optional<glm::vec3> HandLandPoint() noexcept;
	/// Each site's cheats back as they were before the switch
	void TurnPrayerOff() noexcept;
	/// Where the camera looks, on the land
	[[nodiscard]] static glm::vec3 CameraPoint() noexcept;

	MagicType _selected {MagicType::Fireball};

	// The Computer players tab
	/// Every player's alliance strength towards each player, and a button that sets two players' alliance as the
	/// script's SET_PLAYER_ALLY does
	void DrawAlliances() noexcept;
	/// The two players and the strength the button sets
	std::array<int, 2> _allyPlayers {0, 1};
	float _allyStrength {1.0f};

	// The particles' tab
	enum class SpawnAt : uint8_t
	{
		CameraFocus,
		Hand,
	};
	/// By particle type, or by any file
	bool _byFile {false};
	ParticleType _particleType {ParticleType::Smoke};
	int _particleFile {0};
	SpawnAt _spawnAt {SpawnAt::CameraFocus};
	float _spawnHeight {0.0f};
	float _magnitude {1.0f};
	/// The player whose effect it is
	int _particlePlayer {0};
	/// Seconds before it closes down, none to run until closed
	float _closeAfter {0.0f};
	bool _synced {false};
	particles::DrawPath _drawPath {particles::DrawPath::Sorted};
	/// The creatures and villagers nearest a started effect it is given to act on, as the heal miracle is given people
	int _targets {0};
	/// The belief a queued sprite shows, and its colour
	int _beliefAmount {50};
	std::array<float, 3> _beliefColour {1.0f, 1.0f, 1.0f};
	std::vector<std::string> _particleFiles;
	struct TimedEffect
	{
		uint32_t id;
		float secondsLeft;
	};
	std::vector<TimedEffect> _timedEffects;

	// The Running tab
	std::unique_ptr<miracles::CasterInterface> _caster;
	miracles::Choice _choice;
	/// How charged the cast is, as a seed's multiplier
	float _multiplier {1.0f};
	PlayerNames _player {PlayerNames::PLAYER_ONE};
	ClickCast _clickCast {ClickCast::Off};
	/// A click taken, to cast at the next update
	bool _clicked {false};
	/// The left press the window took, whose release it takes too
	debug::LeftClickCapture _leftCapture;
	std::unique_ptr<miracles::DispenserCreatorInterface> _dispenserCreator;
	miracles::DispenserKind _dispenserKind {miracles::DispenserKind::OneShot};
	DispenserPlace _dispenserPlace {DispenserPlace::CameraFocus};
	std::optional<AbodeInfo> _dispenserAbode;
	int _dispenserPeriod {0};
	/// The next left click on the land makes the dispenser, instead of casting
	bool _dispenserAtClick {false};
	miracles::InfinitePrayer _prayer;
	/// Every reaction, not only the miracles'
	bool _allReactions {false};
	/// What became of the last cast
	std::string _last;
};

} // namespace openblack::debug::gui
