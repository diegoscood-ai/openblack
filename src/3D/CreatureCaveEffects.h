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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack
{

struct TempleLight;

/// The effects of the creature's room, set up the first time the room is drawn in a visit and moved on each frame it
/// is drawn: the flames of its fire and the smoke over it, and the spray and mist at the foot of its waterfall. The
/// flames and smoke are sprites of the room (components::Sprite), the mist domes of mist.l3d
/// (components::MistDome), which the temple's pass draws while the room is drawn.
class CreatureCaveEffects
{
public:
	/// A smoke's particles each: they rise from the smoke's place over 900 steps of age, 255 a second
	static constexpr size_t k_SmokeParticles = 10;
	static constexpr int32_t k_SmokeLife = 900;

	/// A number between two bounds, as the C library's stream gives them
	using Random = std::function<float(float min, float max)>;
	/// A number from 0 up to a bound, as the game's local stream gives them
	using LocalRandom = std::function<float(float x)>;

	/// The two streams the room draws from, as the game: the C library's for the flames, the smokes' particles and the
	/// spray thrown up, and the game's local one for where the spray and the mists are
	struct Draws
	{
		Random crt;
		LocalRandom local;
	};
	/// The game's own streams
	[[nodiscard]] static Draws GameDraws();

	/// A smoke's particles and how they move
	struct Smoke
	{
		struct Particle
		{
			glm::vec3 position {0.0f};
			glm::vec3 velocity {0.0f};
			int32_t age {0};
			float angle {0.0f};
			bool spinsBack {false};
			bool hidden {true};
			entt::entity sprite {entt::null};
		};
		glm::vec3 place {0.0f};
		/// Which way the particles drift as they rise
		glm::vec3 drift {0.0f, 1.0f, 0.0f};
		/// RGB of the particles, which fade by age
		glm::vec3 colour {1.0f};
		float size {1.0f};
		/// Rising smoke is thrown up from its place and falls back, fading in as it starts
		bool rising {false};
		std::array<Particle, k_SmokeParticles> particles;
	};

	/// The flames on the fire, the place the room plays its sound at, and the mists in the temple's light of the moment
	/// the room starts
	CreatureCaveEffects(glm::vec3 fire, const TempleLight& light, Draws draws = GameDraws());
	~CreatureCaveEffects();
	CreatureCaveEffects(const CreatureCaveEffects&) = delete;
	CreatureCaveEffects& operator=(const CreatureCaveEffects&) = delete;
	CreatureCaveEffects(CreatureCaveEffects&&) = delete;
	CreatureCaveEffects& operator=(CreatureCaveEffects&&) = delete;

	/// Whether the textures the effects are drawn with are loaded, without which the room has none
	[[nodiscard]] static bool CanBeMade();

	/// A frame of the room drawn, of milliseconds, and the time in milliseconds the flames take their frames from
	void Update(uint32_t milliseconds, uint32_t tickCount);

	/// A smoke's step of its particles over milliseconds, without drawing them; a rising particle starting over is
	/// thrown up by the random given
	static void StepSmoke(Smoke& smoke, uint32_t milliseconds, const Random& random);
	/// The alpha, out of 255, of a smoke particle of an age
	[[nodiscard]] static uint8_t SmokeAlpha(int32_t age, bool rising);
	/// The frame of the flames' animation of a flame at a time, 0 to 31
	[[nodiscard]] static uint32_t FlameFrame(uint32_t tickCount, uint32_t flame);

	/// A flame's place over the fire and its size, the flame's number counting from 0. Its draws go to the depth first,
	/// then the height and the side, then the size.
	struct Flame
	{
		glm::vec3 place;
		float size;
	};
	[[nodiscard]] static Flame FlameOf(glm::vec3 fire, size_t flame, const Random& random);
	/// The place of a smoke over the fire, two above its draws, drawn across, then up, then in depth
	[[nodiscard]] static glm::vec3 FireSmokePlace(glm::vec3 fire, const Random& random);
	/// The first place of a spray at the waterfall's foot, drawn in depth, then down, then across
	[[nodiscard]] static glm::vec3 SprayFirstPlace(const LocalRandom& random);
	/// Which way a spray drifts as it rises, drawn in depth, then up, then across
	[[nodiscard]] static glm::vec3 SprayDrift(const LocalRandom& random);
	/// Where a spray is thrown up from in a frame, drawn in depth, then down, then across
	[[nodiscard]] static glm::vec3 SprayPlace(const LocalRandom& random);
	/// A mist's place at the waterfall's foot, drawn across, then down, then in depth
	[[nodiscard]] static glm::vec3 MistPlace(const LocalRandom& random);
	/// The mists' colour, ARGB: a faint blue, each byte, its alpha too, by the temple's light over 256
	[[nodiscard]] static uint32_t MistColour(const TempleLight& light);

private:
	[[nodiscard]] Smoke CreateSmoke();
	void ShowSmoke(const Smoke& smoke) const;

	Draws _draws;

	std::vector<entt::entity> _flames;
	std::vector<Smoke> _fireSmoke;
	std::vector<Smoke> _spray;
	std::vector<entt::entity> _mists;
};

} // namespace openblack
