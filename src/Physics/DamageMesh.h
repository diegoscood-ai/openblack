/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/L3DSubMesh.h"
#include "Graphics/Haze.h"
#include "Graphics/WorldTriangles.h"

/// How a building breaks where a rock strikes it: its model is kept as loose triangles in the world, those near the
/// rock's line of flight are knocked out (big triangles halved until they fit), and the pieces that no longer hold on to
/// the rest fly off.
namespace openblack::physics::damage
{
/// A building copied into world-space triangles that a rock can knock pieces out of.
/// Every LOD 0 primitive keeps its material; the triangles know their edge neighbours and a size class (how many
/// times they may still be halved).
class Mesh
{
public:
	struct Corner
	{
		glm::vec3 position;
		glm::vec2 uv;
	};
	struct Triangle
	{
		int group {-1};
		std::array<Corner, 3> corners;
		/// edge k = (corners[k], corners[k+1]); -1 = open edge (a side wall is drawn)
		std::array<int, 3> neighbours {-1, -1, -1};
		int sizeClass {0};
		/// the size class it counts as part of the building at: a triangle counts while the two are equal
		int countedClass {0};
	};
	struct Primitive
	{
		graphics::L3DSubMesh::Primitive material;
		std::vector<Triangle> triangles;
		/// The primitive of the building's own mesh it was copied from: its material (texture, mode, flag bits,
		/// ALPHAREF) for the draw
		graphics::world_triangles::MaterialRef source;
	};
	/// A piece that broke off, centred on its centroid, and how it starts flying.
	struct Piece
	{
		std::shared_ptr<Mesh> mesh;
		glm::vec3 centre;
		glm::vec3 velocity;
		glm::vec3 angularVelocity;
		size_t lifeTriangles {0}; ///< the triangles when the piece was made (its lifetime: 100 turns each)
	};
	/// What starts a piece flying (its body and its dust), called the moment the piece is made
	using LaunchPiece = std::function<void(const Piece&)>;

	/// The LOD 0 sub-meshes of the object's mesh through its world matrix.
	static std::shared_ptr<Mesh> FromEntity(entt::entity entity);
	/// The same, baked onto the landscape when it follows it (a broken building has given up its MorphWithTerrain)
	static std::shared_ptr<Mesh> FromEntity(entt::entity entity, bool followsLand);
	/// A mesh of the primitives given, their triangles already in the world, each with its size class
	static std::shared_ptr<Mesh> FromPrimitives(std::vector<Primitive> primitives);

	/// An infinite cylinder along vel through pos of radius r knocks triangles out (one
	/// flying piece per primitive, split into connected pieces), then everything left without ground contact falls.
	/// Every piece is launched as it is made, in the original's order: for each primitive, first the parts that come
	/// loose of what broke off it, then what broke off, with three random draws dropped before its spin; the
	/// building's falling parts last.
	void Impact(glm::vec3 pos, glm::vec3 vel, float radius, const LaunchPiece& launch);
	/// Pieces connected by shared edges; with groundCheck the ones touching the
	/// landscape (y < ground + 0.1) stay, otherwise only group 0 stays. Lone triangles vanish.
	/// Each piece lets go of its own loose parts (launched first, in the same way) before it spins and is launched.
	void SplitUnconnectedGroups(bool groundCheck, glm::vec3 offset, const LaunchPiece& launch);
	/// The triangles still counting / the original count, 0..1.
	float GetRemaining();
	/// The size class of a triangle: how many times it may still be halved
	[[nodiscard]] static int SizeClassOf(const Triangle& triangle);
	[[nodiscard]] float Remaining() const { return _remaining; }
	[[nodiscard]] glm::vec3 Centroid() const;
	void Translate(glm::vec3 offset);
	[[nodiscard]] float Area() const;
	[[nodiscard]] size_t TriangleCount() const;
	/// The triangles' distinct vertices (exact match), each with the normal of the
	/// first triangle it was found in.
	void UniqueVertices(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals) const;
	/// Where a piece sheds dust as it starts to fly: each of its distinct vertices but the first one found
	[[nodiscard]] std::vector<glm::vec3> DustCorners() const;
	/// Too thin to fly on: a fifth of its hull's radius is more than its area over twice that radius. The hull is the
	/// distinct vertices and a copy of each 0.45 behind along its normal
	[[nodiscard]] bool IsSliver() const;
	/// A corner of a landed piece in the world, through its body's matrix: the rows r0, r1, r2 (glm's columns) and the
	/// body's centre c, in the original's order of float products and sums: x = ((v.x r0.x + v.z r2.x) + v.y r1.x) + c.x,
	/// y = ((v.z r2.y + v.y r1.y) + v.x r0.y) + c.y, z = ((v.x r0.z + v.z r2.z) + v.y r1.z) + c.z
	[[nodiscard]] static glm::vec3 StuckCorner(const glm::mat3& rows, glm::vec3 centre, glm::vec3 corner);
	/// Adds a landed piece back as rubble: its first primitive, each corner through StuckCorner, after the primitives
	/// already here
	void Merge(const Mesh& piece, const glm::mat3& rows, glm::vec3 centre);
	/// The same geometry as AppendDraw as an L3D mesh, in the space given by worldToLocal, plus `extra` (the partly
	/// built draw). Its own primitives are cpuDrawn: the main view draws AppendDraw's instead and this mesh serves
	/// the other views (reflections), the picking and the bounds. Registers it in the mesh cache; returns its id.
	[[nodiscard]] entt::id_type BuildMesh(const glm::mat4& worldToLocal, const std::string& name,
	                                      std::vector<graphics::L3DSubMesh::GeneratedPrimitive> extra = {}) const;

	/// What every damage mesh draw of a frame shares: the light position, the ambient, this frame's haze and
	/// the view it is measured in, and whether the land light is loaded (without it, white and unlit, as vs_object)
	struct FrameLight
	{
		bool lit {false};
		glm::vec3 light {0.0f};
		int ambient {90};
		graphics::haze::Params haze {};
		glm::mat4 view {1.0f};
	};
	/// The lighting of one draw, the same for every primitive
	struct DrawLight
	{
		bool lit {false};
		glm::vec3 direction {0.0f};    ///< normalize(light - position), in the world
		uint32_t colour {0xFFFFFFFFu}; ///< the land light under the position, tinted and hazed (ARGB)
		uint32_t specular {0};         ///< its specular, + the tint's and the haze's
		int ambient {90};
	};
	/// L = (light - position) per axis, times ModelInverseSquareRoot((y y + z z) + x x)
	[[nodiscard]] static glm::vec3 LightDirection(glm::vec3 light, glm::vec3 position);
	/// The land light at the position (land_light::At); unless the tint pair is 0xFFFFFFFF / 0 the colour times the
	/// tint (c t) >> 8 in the 4 channels and the specular + the tint's, saturated in the 4; then the haze at the
	/// position
	[[nodiscard]] static DrawLight ObjectLight(glm::vec3 position, uint32_t tint, uint32_t tintSpecular,
	                                           const FrameLight& frame);
	/// One triangle of the draw, pass 0. With a matrix its corners go through it first,
	/// ((z r2 + y r1) + r0 x) + t and the like; the face normal is (v1 - v0) x (v2 - v0)
	/// normalised with ModelInverseSquareRoot ((x x + y y) + z z), lit once
	/// (model_light::TwoSided of (l.z n.z + l.y n.y) + l.x n.x). Out come the front (v0, v1, v2) in the
	/// front colour, the back copy 0.45 behind along -n in the back colour and drawn
	/// (b2, b1, b0), and for each edge k with no neighbour
	/// the wall (fk, bk, fk+1), (fk+1, bk, bk+1) on those same vertices. Both
	/// copies keep the triangle's uv and take the specular
	static void AppendTriangle(std::vector<graphics::world_triangles::Vertex>& out, const Triangle& triangle,
	                           const glm::mat4* matrix, const DrawLight& light);
	/// The triangles of each primitive in its source material, lit and in the world. The original flushes them
	/// every 256 vertices or triangles: one batch per primitive here, the same triangles in order.
	/// Not ported: its second pass, the snow over the mesh (from the snow map)
	void AppendDraw(graphics::world_triangles::Frame& out, const glm::mat4* matrix, const DrawLight& light) const;

private:
	/// Finds each triangle's neighbour across each of its edges: a triangle of the same primitive with both ends of the
	/// edge among its corners
	void LinkNeighbours(Primitive& primitive) const;

	std::vector<Primitive> _primitives;
	int _trianglesAtCreation {0};
	float _remaining {1.0f};
};

/// A built building without a broken model is drawn this much of its life repaired
inline constexpr float k_RepairedWithoutRepair = 0.98f;
/// How far a built building is drawn repaired since it was last broken: with a broken model and a repair site, from the
/// life its repair started at (none regained, or a repair that started whole, counts as none); else a little short of
/// its life
[[nodiscard]] float RepairedShare(float life, std::optional<float> repairStart);
/// How much of a building is drawn standing: the smaller of how far it is built and how far it is repaired
[[nodiscard]] float DrawShare(float built, float repaired);
/// A building drawn at least this much repaired, but not wholly, has its broken model made afresh from its intact one
/// when a blow breaks it again
inline constexpr float k_RebuildDrawShare = 0.2f;
/// Whether a breaking blow on a building that already has a broken model makes that model afresh from the intact one
[[nodiscard]] bool RebuildsOnBlow(float drawShare);
/// The broken model a breaking blow on a built building strikes, and how it was got
struct BlowModel
{
	/// what the blow strikes: null when no model could be made
	std::shared_ptr<Mesh> mesh;
	/// made afresh in place of an old broken model (the building had been repaired enough), whose last hitter goes with it
	bool rebuilt {false};
};
/// Before a breaking blow strikes a built building: with no broken model (`current` null) one is made from the intact
/// model; with one and a draw share that RebuildsOnBlow, a new one is made from the intact model in the old one's place;
/// otherwise the blow strikes a copy of the old one, so that a draw taken earlier keeps it. `fromIntact` makes a broken
/// model from the intact one, and is called only when one is needed
[[nodiscard]] BlowModel ModelForBlow(const std::shared_ptr<const Mesh>& current, float drawShare,
                                     const std::function<std::shared_ptr<Mesh>()>& fromIntact);

/// How a building drawn part built shows: its whole model kept below a cut as far up as the share; its scaffold whole
/// and sunk into the land below a fifth built, whole up to four fifths, then cut down from its top
struct PartialBuild
{
	/// The height the model is cut at: twice the half height times the scale and the share, above the foot
	float modelCut {0.0f};
	/// While the scaffold rises, its move along its up axis (with the scale), down into the land
	std::optional<float> scaffoldDrop;
	/// Past four fifths, the height the scaffold is cut at
	std::optional<float> scaffoldCut;
};
/// A cut less than this above the foot draws nothing of a primitive
inline constexpr float k_LeastPartialCut = 0.2f;
/// The scaffold rises until this share, and is cut from its top past the other
inline constexpr float k_ScaffoldRisen = 0.2f;
inline constexpr float k_ScaffoldCutFrom = 0.8f;
/// The share drawn (held between 0 and 1), the height of the building's foot, its model's half height and its scale
[[nodiscard]] PartialBuild PartialBuildOf(float share, float footHeight, float halfHeight, float scale);
} // namespace openblack::physics::damage
