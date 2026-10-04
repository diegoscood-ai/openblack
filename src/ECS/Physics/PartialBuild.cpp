/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PartialBuild.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <span>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

namespace
{
using GeneratedPrimitive = graphics::L3DSubMesh::GeneratedPrimitive;

constexpr float k_MinimumCut = 0.2f;       ///< [0x8AB244], fn_0085C7F0 0x85C82F: nothing of the primitive under it
constexpr float k_ScaffoldRiseEnd = 0.2f;  ///< [0x8AB244], 0x816F77
constexpr float k_ScaffoldCutStart = 0.8f; ///< [0x8C4A04], 0x817030 / 0x817090
constexpr float k_ScaffoldRate = 5.0f;     ///< [0x8DAD58], 0x816F8B / 0x817096
constexpr float k_ScaffoldDrop = -2.0f;    ///< [0x8C7CE0], 0x816FAC: -2 x half height x (1 - 5 pct)
constexpr float k_BelowFloor = 0.0001f;    ///< [0x8BF518], fn_0081D790 0x81D8F4: the kept corner's distance at least this
constexpr float k_AboveCeiling = -0.0001f; ///< [0x8D8738], fn_0081D790 0x81D9B8: the cut corner's at most this
constexpr size_t k_MaxSegments = 1000;     ///< fn_00820EC0 0x820EC0: stored while count <= 0x3E8, else [0xEB99A0] = 1

struct Vertex
{
	glm::vec3 pos; ///< world
	glm::vec2 uv;
	glm::vec3 normal; ///< world
	float melt;       ///< the melting delta it carries (model units along the model's Y)
};

/// fn_0081D790 0x81D9CF..: t B + (1 - t) A for every attribute. (approximate) The original interpolates the lit colour;
/// here the normal, renormalised.
Vertex Lerp(const Vertex& a, const Vertex& b, float t)
{
	const float s = 1.0f - t;
	const auto n = b.normal * t + a.normal * s;
	return {b.pos * t + a.pos * s, b.uv * t + a.uv * s, glm::dot(n, n) > 0.0f ? glm::normalize(n) : a.normal,
	        b.melt * t + a.melt * s};
}

/// fn_0081D790(ecx = the corner below, edx = the corner above): the point on the world plane (0, -1, 0, h)
/// ([0xF03128..0xF03134]) of the edge. The distances are clamped to the side the 0x800 flags gave each corner, so a
/// corner the (local y) flag test put on the wrong side of the world plane gives t = 0 or 1, not a point off the edge.
Vertex CutPoint(const Vertex& below, const Vertex& above, float h)
{
	float dA = h - below.pos.y;
	if (dA < 0.0f) // 0x81D8E5..0x81D8F4
	{
		dA = k_BelowFloor;
	}
	float dB = h - above.pos.y;
	if (dB > 0.0f) // 0x81D9A9..0x81D9B8
	{
		dB = k_AboveCeiling;
	}
	return Lerp(below, above, dA / (dA - dB)); // 0x81D9BE..0x81D9C6
}

/// One clipped draw (fn_0085C7F0's or fn_0085C0E0's list) and the cut segments of the primitive (0xE637E0, count
/// [0xEB9994], overflow [0xEB99A0]; both passes of a primitive share them)
struct Pass
{
	std::vector<std::array<Vertex, 3>> triangles;
};
struct Segments
{
	std::vector<std::array<Vertex, 2>> list;
	bool overflow {false};

	/// fn_0081D1B0 -> fn_00820EC0
	void Record(const Vertex& a, const Vertex& b)
	{
		if (list.size() > k_MaxSegments)
		{
			overflow = true;
			return;
		}
		list.push_back({a, b});
	}
};

/// fn_0081D4F0(a, b, c): a triangle with a corner flagged 0x800 (above), in the original's corner order. Every cut
/// records its segment (n0, n1) before the triangles are emitted.
void ClipTriangle(const std::array<Vertex, 3>& v, const std::array<bool, 3>& up, float h, Pass& pass, Segments& segments)
{
	const auto& a = v[0];
	const auto& b = v[1];
	const auto& c = v[2];
	const auto emit = [&pass](const Vertex& x, const Vertex& y, const Vertex& z) { pass.triangles.push_back({x, y, z}); };
	if (up[0])
	{
		if (up[1])
		{
			if (up[2])
			{
				return; // 0x81D536: all above
			}
			// 0x81D53C: c below
			const auto n0 = CutPoint(c, a, h);
			const auto n1 = CutPoint(c, b, h);
			segments.Record(n0, n1);
			emit(c, n0, n1);
		}
		else if (up[2])
		{
			// 0x81D5B8: b below
			const auto n0 = CutPoint(b, a, h);
			const auto n1 = CutPoint(b, c, h);
			segments.Record(n0, n1);
			emit(n0, b, n1);
		}
		else
		{
			// 0x81D5F5: b and c below
			const auto n0 = CutPoint(b, a, h);
			const auto n1 = CutPoint(c, a, h);
			segments.Record(n0, n1);
			emit(n0, b, c);
			emit(n0, c, n1);
		}
	}
	else if (up[1])
	{
		if (up[2])
		{
			// 0x81D67E: a below
			const auto n0 = CutPoint(a, b, h);
			const auto n1 = CutPoint(a, c, h);
			segments.Record(n0, n1);
			emit(a, n0, n1);
		}
		else
		{
			// 0x81D6BB: a and c below
			const auto n0 = CutPoint(a, b, h);
			const auto n1 = CutPoint(c, b, h);
			segments.Record(n0, n1);
			emit(a, n0, c);
			emit(n0, n1, c);
		}
	}
	else if (up[2])
	{
		// 0x81D71B: a and b below
		const auto n0 = CutPoint(a, c, h);
		const auto n1 = CutPoint(b, c, h);
		segments.Record(n0, n1);
		emit(n0, a, b);
		emit(n0, b, n1);
	}
	else
	{
		emit(a, b, c); // 0x81D778 (only the frustum bits were set)
	}
}

/// The triangles of one drawn primitive, split when the 16-bit indices are full (port guard)
class Emitter
{
public:
	Emitter(std::vector<GeneratedPrimitive>& result, const graphics::L3DSubMesh::Primitive& material, glm::vec3 up, bool bake)
	    : _result(result)
	    , _up(up)
	    , _bake(bake)
	{
		_current.material = material;
	}
	Emitter(const Emitter&) = delete;
	Emitter& operator=(const Emitter&) = delete;
	~Emitter() { Flush(); }

	void Triangle(const Vertex& a, const Vertex& b, const Vertex& c)
	{
		if (_current.positions.size() + 3 > 0xFFFF)
		{
			Flush();
		}
		for (const auto* v : {&a, &b, &c})
		{
			_current.indices.push_back(static_cast<uint16_t>(_current.positions.size()));
			// not baked: the vertex shader adds the delta along the model's Y when drawing (vs_object_hm_instanced)
			_current.positions.push_back(_bake ? v->pos : v->pos - _up * v->melt);
			_current.uvs.push_back(v->uv);
			_current.normals.push_back(v->normal);
		}
	}

private:
	void Flush()
	{
		if (!_current.indices.empty())
		{
			auto material = _current.material;
			_result.push_back(std::move(_current));
			_current = {};
			_current.material = material;
		}
	}

	std::vector<GeneratedPrimitive>& _result;
	GeneratedPrimitive _current;
	glm::vec3 _up;
	bool _bake;
};

/// The object being drawn: [0xEA1AE8] and its world matrix 0xEA9E10
struct DrawnObject
{
	glm::mat4 model;
	glm::mat3 rotation;
	float y;     ///< obj +0x3C
	float scale; ///< obj +0x44 (openblack's Transform scale is uniform: its y)
	float half;  ///< mesh +0x28, half the height of the bounding box
	bool melting;
	bool bake;
	std::vector<float> stream; ///< obj +0x80: one melting delta per vertex of every sub-mesh, in order
	size_t cursor {0};         ///< [0xF05180]

	[[nodiscard]] float Delta(size_t k) const
	{
		// (port guard) past the end the original reads whatever follows the buffer (see the scaffold below)
		return melting && k < stream.size() ? stream[k] : 0.0f;
	}

	/// A model vertex in the world: (x - k nx, y + delta, z - k nz) through the matrix (fn_0085C7F0 0x85C86F..0x85C8DD,
	/// fn_0085C0E0 0x85C156..0x85C1C9)
	[[nodiscard]] Vertex World(const graphics::L3DSubMesh& subMesh, uint32_t index, float delta, float inset,
	                           glm::vec3 offset) const
	{
		const auto& p = subMesh.GetCollisionPositions().at(index);
		const auto& normals = subMesh.GetCollisionNormals();
		const auto n = index < normals.size() ? normals[index] : glm::vec3(0.0f, 1.0f, 0.0f);
		const auto& uvs = subMesh.GetCollisionUVs();
		const glm::vec3 local(p.x - inset * n.x, p.y + delta, p.z - inset * n.z);
		const auto w = rotation * n;
		return {glm::vec3(model * glm::vec4(local, 1.0f)) + offset, index < uvs.size() ? uvs[index] : glm::vec2(0.0f),
		        glm::dot(w, w) > 0.0f ? glm::normalize(w) : glm::vec3(0.0f, 1.0f, 0.0f), delta};
	}
};

/// fn_0085C7F0: one primitive cut at the plane `planeD`, then (when `innerWalls`, [0xC392B4]) fn_0085C0E0's inner pass
/// and fn_00820B20's cap
void ClippedPrimitive(DrawnObject& object, const graphics::L3DSubMesh& subMesh, size_t primitiveIndex, float planeD,
                      bool innerWalls, const PartialBuildOptions& options, std::vector<GeneratedPrimitive>& result)
{
	// 0x85C7F6..0x85C83A: rel = planeD - obj.y; under 0.2 nothing, and the stream does not move (literal: the next
	// primitives read this one's deltas)
	const float rel = planeD - object.y;
	if (rel < k_MinimumCut)
	{
		return;
	}
	const auto& primitive = subMesh.GetPrimitives().at(primitiveIndex);
	const auto [firstVertex, vertexCount] = subMesh.GetCollisionVertexRanges().at(primitiveIndex);
	const auto [firstIndex, indexCount] = subMesh.GetCollisionRanges().at(primitiveIndex);
	const auto& indices = subMesh.GetCollisionIndices();
	const auto& positions = subMesh.GetCollisionPositions();
	const size_t stream = object.cursor;
	object.cursor += vertexCount; // 0x85C87A..0x85C884
	// 0x85C938..0x85C9AD / 0x85CBA8..0x85CC1A: flagged 0x800 when rel <= the model y (+ the melting delta). Literal: the
	// unscaled model y against a height scaled by obj +0x44 (only differs for scale != 1)
	const auto above = [&](uint32_t index) {
		return rel <= positions.at(index).y + object.Delta(stream + (index - firstVertex));
	};
	// the world plane the cut points are put on (fn_0081D790)
	const float h = planeD;
	const auto run = [&](float inset, Pass& pass, Segments& segments) {
		for (uint32_t i = firstIndex; i + 2 < firstIndex + indexCount; i += 3)
		{
			std::array<Vertex, 3> t;
			std::array<bool, 3> up {};
			for (uint32_t k = 0; k < 3; ++k)
			{
				const uint32_t index = indices.at(i + k);
				t.at(k) = object.World(subMesh, index, object.Delta(stream + (index - firstVertex)), inset, glm::vec3(0.0f));
				up.at(k) = above(index);
			}
			if (!up[0] && !up[1] && !up[2])
			{
				pass.triangles.push_back(t); // 0x85CDC5: no flag, into the list as it is
			}
			else
			{
				ClipTriangle(t, up, h, pass, segments); // 0x85CDB9
			}
		}
	};
	Segments segments; // 0x85C809 / 0x85C810: count and overflow reset per primitive
	Pass outer;
	run(0.0f, outer, segments);
	if (outer.triangles.empty())
	{
		return; // 0x85CE06: an empty list draws nothing, no inner pass and no cap
	}
	// 0x85CD61 g_NoBackfaceCull = 1 and 0x85CE91..0x85CEB1 D3DRENDERSTATE_CULLMODE = NONE: every pass is two-sided
	auto material = primitive;
	material.twoSided = true;
	const glm::vec3 up(object.model[1]);
	Emitter emitter(result, material, up, object.bake);
	for (const auto& t : outer.triangles)
	{
		emitter.Triangle(t[0], t[1], t[2]);
	}
	if (!innerWalls)
	{
		return; // [0xC392B4] = 0 while the scaffold draws (0x816F60)
	}
	// fn_0085C0E0: k = [0xC392AC] when material +5 bit 0 (0x85C0EB..0x85C111), else [0xC392B0]; the stream is
	// rewound first (0x85CEFE..0x85CF0C) so the same deltas are used
	const float k = options.innerOffset.value_or(primitive.twoSided ? PartialBuild::k_InnerOffsetTwoSided
	                                                                 : PartialBuild::k_InnerOffsetCulled);
	const size_t outerCount = segments.list.size(); // [0xEB9998] (0x85CEEC)
	Pass inner;
	run(k, inner, segments);
	for (const auto& t : inner.triangles)
	{
		emitter.Triangle(t[0], t[1], t[2]);
	}
	// 0x85CF18..0x85CF43: the cap only if the outer pass cut something, the inner one cut as many times and obj+8 & 0x200
	// is clear
	const size_t innerCount = segments.list.size() - outerCount; // [0xEB999C]
	if (outerCount == 0 || innerCount != outerCount || options.noCap)
	{
		return;
	}
	// fn_00820B20: nothing after an overflow (0x820B20..0x820B45)
	if (segments.overflow)
	{
		return;
	}
	// 0x820D54..0x820E61: per segment i, (o0, o1, i0) and (i1, o1, i0); the uvs of the cut points, no culling.
	// (approximate) the original's colour is 0.75 x the object diffuse (+0x4C), unlit (0x820B46..0x820B93); here the
	// normal is the world up and the lighting is the renderer's
	const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
	for (size_t s = 0; s < outerCount; ++s)
	{
		auto o0 = segments.list[s][0];
		auto o1 = segments.list[s][1];
		auto i0 = segments.list[outerCount + s][0];
		auto i1 = segments.list[outerCount + s][1];
		o0.normal = o1.normal = i0.normal = i1.normal = worldUp;
		emitter.Triangle(o0, o1, i0);
		emitter.Triangle(i1, o1, i0);
	}
}

/// fn_0084BA90 (one primitive) / fn_0087FB60 -> fn_00859530 (every primitive of the sub-mesh, with the melting stream):
/// the normal, uncut draw with the material's own culling
void WholePrimitive(const DrawnObject& object, const graphics::L3DSubMesh& subMesh, size_t primitiveIndex, size_t stream,
                    glm::vec3 offset, std::vector<GeneratedPrimitive>& result)
{
	const auto firstVertex = subMesh.GetCollisionVertexRanges().at(primitiveIndex).first;
	const auto [firstIndex, indexCount] = subMesh.GetCollisionRanges().at(primitiveIndex);
	const auto& indices = subMesh.GetCollisionIndices();
	Emitter emitter(result, subMesh.GetPrimitives().at(primitiveIndex), glm::vec3(object.model[1]), object.bake);
	for (uint32_t i = firstIndex; i + 2 < firstIndex + indexCount; i += 3)
	{
		std::array<Vertex, 3> t;
		for (uint32_t k = 0; k < 3; ++k)
		{
			const uint32_t index = indices.at(i + k);
			t.at(k) = object.World(subMesh, index, object.Delta(stream + (index - firstVertex)), 0.0f, offset);
		}
		emitter.Triangle(t[0], t[1], t[2]);
	}
}

/// fn_00816AD0
std::vector<GeneratedPrimitive> Draw(entt::entity building, entt::id_type meshId, float percent,
                                     const PartialBuildOptions& options, bool bake)
{
	std::vector<GeneratedPrimitive> result;
	auto& meshes = Locator::resources::value().GetMeshes();
	if ((options.skipZero && percent == 0.0f) || !meshes.Contains(meshId))
	{
		return result; // DrawBuilding 0x517FE0: nothing at 0
	}
	// 0x816AD6..0x816B02: pct clamped to [0, 1]
	const float pct = percent < 0.0f ? 0.0f : (percent > 1.0f ? 1.0f : percent);
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(building);
	const auto mesh = meshes.Handle(meshId);
	DrawnObject object {
	    lh_matrix::Model(transform),
	    transform.rotation,
	    transform.position.y,
	    transform.scale.y,
	    // (approximate) the bounding box openblack keeps; the original's is the L3D mesh's own (+0x14)
	    mesh->GetBoundingBox().Size().y * 0.5f,
	    options.melting.value_or(registry.AllOf<MorphWithTerrain>(building)),
	    bake,
	    {},
	};
	if (object.melting)
	{
		// (approximate) UpdateMelting 0x8168F0 takes the deltas once, at creation; here at every build (like the
		// vertex shader of openblack)
		const auto ground = land_morph::CurrentAltitude();
		for (const auto& subMesh : mesh->GetSubMeshes())
		{
			const auto& positions = subMesh->GetCollisionPositions();
			const size_t start = object.stream.size();
			object.stream.resize(start + positions.size());
			land_morph::MeltingDeltas(ground, object.model, object.scale, positions, std::span(object.stream).subspan(start));
		}
	}
	// 0x816D6F..0x816D9B: d = ((half x scale) x pct) x 2 + obj.y
	const float planeD = object.half * object.scale * pct * 2.0f + object.y;
	// 0x816EA8..0x816EB5: S = max(fn_0080AD60 = the highest status, 1)
	uint32_t scaffold = 1;
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		scaffold = std::max<uint32_t>(scaffold, subMesh->GetFlags().status);
	}
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		const auto flags = subMesh->GetFlags();
		const auto& primitives = subMesh->GetPrimitives();
		const size_t subMeshVertices = subMesh->GetCollisionPositions().size();
		// 0x816F0B..0x816F13: the LOD bit (flags >> 29) & [0xC37D94] ((approximate) always LOD 0 here, no distance bands;
		// the physics sub-meshes have none, skipped as a port guard); any other status than 0 or S is not drawn
		// (0x816F54). Both advance the stream past the sub-mesh (0x817278 / 0x81725F)
		if (subMesh->IsPhysics() || (flags.lodMask & 1) == 0 || (flags.status != 0 && flags.status != scaffold))
		{
			object.cursor += subMeshVertices;
			continue;
		}
		if (flags.status == 0)
		{
			// 0x816F19..0x816F44: every primitive through fn_0085C7F0, with the inner walls ([0xC392B4] = 1)
			for (size_t p = 0; p < primitives.size(); ++p)
			{
				ClippedPrimitive(object, *subMesh, p, planeD, true, options, result);
			}
			continue;
		}
		// the scaffold, [0xC392B4] = 0 (0x816F60) until 0x817253; the phase is chosen per primitive
		for (size_t p = 0; p < primitives.size(); ++p)
		{
			if (pct <= k_ScaffoldCutStart) // 0x817030..0x81703B: pct > 0.8 cut, else whole (risen or rising)
			{
				glm::vec3 offset(0.0f);
				if (pct < k_ScaffoldRiseEnd)
				{
					// 0x816F8B..0x816FEA: the matrix translation += its row 1 (the up axis with the scale) x
					// (1 - 5 pct) x half x -2
					offset = glm::vec3(object.model[1]) * ((1.0f - pct * k_ScaffoldRate) * object.half * k_ScaffoldDrop);
				}
				if (object.melting)
				{
					// 0x817001 / 0x81722F fn_0087FB60: literal, every primitive of the sub-mesh once per primitive, each
					// call moving the stream by the whole sub-mesh (with n primitives the copies after the first and the
					// sub-meshes after it read the wrong deltas)
					for (size_t q = 0; q < primitives.size(); ++q)
					{
						WholePrimitive(object, *subMesh, q,
						               object.cursor + subMesh->GetCollisionVertexRanges().at(q).first, offset, result);
					}
					object.cursor += subMeshVertices;
				}
				else
				{
					WholePrimitive(object, *subMesh, p, 0, offset, result); // 0x817014 / 0x81723C fn_0084BA90
				}
			}
			else
			{
				// 0x81708D..0x8170CB: d = ((1 - (pct - 0.8) x 5) x half x scale) x 2 + obj.y, through fn_0085C7F0
				// (so the 0.2 rule holds here too), then the main plane is restored (0x8171DD..0x817217)
				const float scaffoldD =
				    (1.0f - (pct - k_ScaffoldCutStart) * k_ScaffoldRate) * object.half * object.scale * 2.0f + object.y;
				ClippedPrimitive(object, *subMesh, p, scaffoldD, false, options, result);
			}
		}
	}
	return result;
}
} // namespace

std::vector<GeneratedPrimitive> PartialBuild::Build(entt::entity building, entt::id_type meshId, float percent,
                                                    const PartialBuildOptions& options)
{
	return Draw(building, meshId, percent, options, true);
}

entt::id_type PartialBuild::BuildMesh(entt::entity building, entt::id_type intactMesh, float percent, std::string_view tag,
                                      const PartialBuildOptions& options)
{
	auto& registry = Locator::entitiesRegistry::value();
	// drawn as the entity's mesh: with MorphWithTerrain the vertex shader adds the deltas, so they are not baked
	auto primitives = Draw(building, intactMesh, percent, options, !registry.AllOf<MorphWithTerrain>(building));
	if (primitives.empty())
	{
		return 0;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	// Build works in world space: back into the building's own
	const glm::mat4 toLocal = glm::inverse(lh_matrix::Model(registry.Get<const Transform>(building)));
	const glm::mat3 normals(glm::transpose(glm::inverse(glm::mat3(toLocal))));
	for (auto& p : primitives)
	{
		for (auto& v : p.positions)
		{
			v = glm::vec3(toLocal * glm::vec4(v, 1.0f));
		}
		for (auto& n : p.normals)
		{
			const auto m = normals * n;
			n = glm::dot(m, m) > 0.0f ? glm::normalize(m) : glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}
	static uint32_t s_Next = 0;
	const std::string name(tag);
	const auto id = entt::hashed_string((name + "/" + std::to_string(s_Next++)).c_str()).value();
	try
	{
		meshes.Load(id, resources::L3DLoader::FromGeneratedTag {}, name, primitives);
		if (meshes.Contains(intactMesh))
		{
			// the building's mark on the landscape stays
			meshes.Handle(id)->SetFootprintSource(meshes.Handle(intactMesh).handle());
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Partly built mesh ({}): {}", name, e.what());
		return 0;
	}
	return id;
}

void PartialBuild::EraseMesh(entt::id_type id)
{
	auto& meshes = Locator::resources::value().GetMeshes();
	if (id != 0 && meshes.Contains(id))
	{
		meshes.Erase(id);
	}
}
