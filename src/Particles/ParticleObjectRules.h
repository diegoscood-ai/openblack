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
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The rules that tie an effect to the game objects it is given (ParticleObjectRules.cpp): an unseen atom kept on each
// object, and atoms let out from random points of the surface of the object under that atom. The pieces here are
// free of the game's state, so they are tested with fakes, except the last two, which read the registry. Wiki:
// docs/bw1-notes/particles.md, "The object rules".

namespace openblack::psys
{
class Effect;
struct Atom;

/// An atom whose rule can give a random point of what the atom stands for, the way an emitter under it asks its parent
/// for one. The other atoms give their own place, and an atom with no creator gives nothing
class SurfacePointSource
{
public:
	SurfacePointSource() = default;
	SurfacePointSource(const SurfacePointSource&) = default;
	SurfacePointSource(SurfacePointSource&&) = default;
	SurfacePointSource& operator=(const SurfacePointSource&) = default;
	SurfacePointSource& operator=(SurfacePointSource&&) = default;
	virtual ~SurfacePointSource() = default;

	/// What came of asking: a point, nothing (the point asked for is left as it was), or a case the port does not cover
	enum class Result : uint8_t
	{
		Point,
		Nothing,
		NotPorted,
	};
	/// `atom` is one this rule made. The draws, if any, are on the effect's stream
	[[nodiscard]] virtual Result RandomSurfacePoint(Effect& effect, const Atom& atom, glm::vec3& out) const = 0;
};

namespace object_surface
{
/// A 3D object's model as the game walks it for a random point of its surface: its parts, each part's items, each
/// item's triangles with their three corners, in the model's own space (the file's vertices)
class Model
{
public:
	Model() = default;
	Model(const Model&) = default;
	Model(Model&&) = default;
	Model& operator=(const Model&) = default;
	Model& operator=(Model&&) = default;
	virtual ~Model() = default;

	[[nodiscard]] virtual uint32_t PartCount() const = 0;
	[[nodiscard]] virtual uint32_t ItemCount(uint32_t part) const = 0;
	[[nodiscard]] virtual uint32_t TriangleCount(uint32_t part, uint32_t item) const = 0;
	[[nodiscard]] virtual std::array<glm::vec3, 3> Corners(uint32_t part, uint32_t item, uint32_t triangle) const = 0;
};

/// The 3D object's matrix: the model's x, y and z axes in the world (each scaled) and its place
struct Matrix
{
	glm::vec3 x {1.0f, 0.0f, 0.0f};
	glm::vec3 y {0.0f, 1.0f, 0.0f};
	glm::vec3 z {0.0f, 0.0f, 1.0f};
	glm::vec3 translation {0.0f};
};

/// A point of the model through its matrix: a part, one of its items and one of its triangles, each as likely as the
/// others whatever their size (one Rand of each count, in that order), then two random fractions a and b of 1 for the
/// sides from the first corner to the second and to the third, both turned to 1 minus themselves when they add up to
/// more than 1, so that the point falls inside the triangle. Nothing when a count is 0 (no draw after it)
[[nodiscard]] std::optional<glm::vec3> RandomPoint(Effect& effect, const Model& model, const Matrix& matrix);

/// The triangle's point (corners c0, c1, c2) at the fractions a and b, as the game sums it
[[nodiscard]] glm::vec3 TrianglePoint(const std::array<glm::vec3, 3>& corners, float a, float b);
/// A point of the model's space put in the world through the matrix, as the game sums it
[[nodiscard]] glm::vec3 ToWorld(const Matrix& matrix, const glm::vec3& point);

/// The world matrix of an object that turns about Y only (most objects): rows (c S, 0, s S), (0, S, 0), (-s S, 0, c S)
/// with c and s the angle's cosine and sine, each product rounded once; with a scale of 1, c and s rounded to floats;
/// with no angle, S down the diagonal (1 for a scale of 1). An angle or a scale that is not a number counts as 0 or 1
[[nodiscard]] Matrix ObjectWorldMatrix(const glm::vec3& place, float yAngle, float scale);
/// The world matrix of a mobile object or a mobile static: the rotation of its Y, X and Z angles with every cell times
/// the scale, unless the scale is 1 or not a number
[[nodiscard]] Matrix MobileWorldMatrix(const glm::vec3& place, const glm::mat3& rotation, float scale);

/// The size an emitted atom is drawn at for its age: a pulse of cos(age x speed x 2 pi) x magnitude / 2 + 1 (none below
/// 0), times a ramp from 0 at birth up to 1 at `ageMaxSize` and back down to 0 at `ageZeroSize`, kept within 0..1 (not a
/// number gives 0)
[[nodiscard]] float EmittedAtomScale(float age, float pulseSpeed, float pulseMagnitude, float ageMaxSize, float ageZeroSize);

/// The point the land is asked at for an emitted atom: each coordinate x 65536 then x 0.1, each product a float,
/// truncated. 65536 x 0.1f is 6553.6f exactly, so the cells are the map coordinates' own conversion's

[[nodiscard]] glm::ivec2 LandTestCell(const glm::vec3& point);

// The two below read the entities registry

/// A game object, the only kind of target the object rule keeps: buildings, fields, fish farms, features, trees,
/// people, animals, creatures, piles, rocks and the other things of the world. Not the hand, the towns, the mist, the
/// script markers, the spells and the other things that only have a place
[[nodiscard]] bool IsGameObject(entt::entity entity);
/// The object's world matrix as its class builds it: at its map coordinates, turned by its angles and scaled.
/// `object` has a Transform
[[nodiscard]] Matrix WorldMatrixOf(entt::entity object);
} // namespace object_surface

} // namespace openblack::psys
