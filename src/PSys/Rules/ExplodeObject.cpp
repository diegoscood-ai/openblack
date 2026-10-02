/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The meshes thrown to pieces: fn_00681260 / fn_006812B0 (the queues), UR_ExplodeObject::ModifyAtomCollection 0x6814E0
// with ExplodeMesh 0x6807B0, UR_ExplodeObject2 0x681560, and the always-on EXPLODE_OBJECT effect of PSysUtilityPSys
// (fn_006717F0 / fn_006718E0 / fn_006718C0). Disassembly: dev\_scratch\Milagros\polish\explodemesh.asm,
// gjmesh_drawat.asm, beam2\landlight.asm; the pieces' draw RenderParticleGJMesh::DrawAt 0x67C150 (gj_mesh, to
// Graphics/WorldTriangles.h); wiki: docs/bw1-notes/miracles.md, "Explosión de rayo".

#include "ExplodeObject.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <istream>
#include <string>
#include <unordered_map>
#include <utility>

#include <L3DFile.h>
#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/ModelLight.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "PSys/ParticleTypes.h"
#include "PSys/PSys.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;
using namespace openblack::psys::explode_object;

namespace
{
// ---- the CRT's qsort 0x7C7E64 / _shortsort 0x7C7FB8 and bsearch 0x7C8141 (the MSVC 6 ones) ----
// The edge comparator of ExplodeMesh never answers 0, so which of two equal edges comes first is the CRT's own
// business: their algorithms are kept to get the same order (and so the same pieces).

template <class T, class Compare>
void ShortSort(T* lo, T* hi, Compare comp)
{
	// _shortsort 0x7C7FB8: the largest of lo..hi goes to hi (a later one only if it compares > 0), hi steps down
	while (hi > lo)
	{
		T* max = lo;
		for (T* p = lo + 1; p <= hi; ++p)
		{
			if (comp(*p, *max) > 0)
			{
				max = p;
			}
		}
		std::swap(*max, *hi);
		--hi;
	}
}

template <class T, class Compare>
void CrtQsort(T* base, size_t num, Compare comp)
{
	// _qsort 0x7C7E64: insertion of size <= 8 (0x7C7EAF), the middle element as the pivot (0x7C7EE3), the smaller
	// part done first and the other one kept on a stack of 30
	if (num < 2)
	{
		return;
	}
	T* lostk[30];
	T* histk[30];
	int stkptr = 0;
	T* lo = base;
	T* hi = base + (num - 1);
	for (;;)
	{
		const size_t size = static_cast<size_t>(hi - lo) + 1;
		if (size <= 8)
		{
			ShortSort(lo, hi, comp);
		}
		else
		{
			T* mid = lo + size / 2;
			std::swap(*mid, *lo);
			T* loguy = lo;
			T* higuy = hi + 1;
			for (;;)
			{
				do
				{
					++loguy;
				} while (loguy <= hi && comp(*loguy, *lo) <= 0);
				do
				{
					--higuy;
				} while (higuy > lo && comp(*higuy, *lo) >= 0);
				if (higuy < loguy)
				{
					break;
				}
				std::swap(*loguy, *higuy);
			}
			std::swap(*lo, *higuy);
			if (higuy - 1 - lo >= hi - loguy)
			{
				if (lo + 1 < higuy)
				{
					lostk[stkptr] = lo;
					histk[stkptr] = higuy - 1;
					++stkptr;
				}
				if (loguy < hi)
				{
					lo = loguy;
					continue;
				}
			}
			else
			{
				if (loguy < hi)
				{
					lostk[stkptr] = loguy;
					histk[stkptr] = hi;
					++stkptr;
				}
				if (lo + 1 < higuy)
				{
					hi = higuy - 1;
					continue;
				}
			}
		}
		--stkptr;
		if (stkptr < 0)
		{
			return;
		}
		lo = lostk[stkptr];
		hi = histk[stkptr];
	}
}

/// bsearch 0x7C8141: halves of num, the middle one at half (num odd) or half - 1 (num even)
template <class T, class Key, class Compare>
const T* CrtBsearch(const Key& key, const T* base, size_t num, Compare comp)
{
	const T* lo = base;
	const T* hi = base + (static_cast<ptrdiff_t>(num) - 1);
	while (num != 0 && lo <= hi)
	{
		const size_t half = num / 2;
		if (half != 0)
		{
			const T* mid = lo + ((num & 1) != 0 ? half : half - 1);
			const int result = comp(key, *mid);
			if (result == 0)
			{
				return mid;
			}
			if (result < 0)
			{
				hi = mid - 1;
				num = (num & 1) != 0 ? half : half - 1;
			}
			else
			{
				lo = mid + 1;
				num = half;
			}
		}
		else
		{
			return comp(key, *lo) != 0 ? nullptr : lo;
		}
	}
	return nullptr;
}

// ---- ExplodeMesh's edges ----

/// fn_0067DFD0's 0x1C bytes: the two ends of an edge, the one with the smaller (z + y) + x first (on a tie the second
/// argument), and its squared length ((dz dz + dy dy) + dx dx, 0x67E060..0x67E084)
struct Edge
{
	glm::vec3 a;
	glm::vec3 b;
	float lengthSquared;
};

Edge MakeEdge(const glm::vec3& p, const glm::vec3& q)
{
	const float sp = (p.z + p.y) + p.x;
	const float sq = (q.z + q.y) + q.x;
	Edge edge;
	if (sq <= sp)
	{
		// 0x67E01F jne: sQ <= sP (C0 or C3) puts the second point first
		edge.a = q;
		edge.b = p;
	}
	else
	{
		edge.a = p;
		edge.b = q;
	}
	const glm::vec3 d = edge.b - edge.a;
	edge.lengthSquared = (d.z * d.z + d.y * d.y) + d.x * d.x;
	return edge;
}

/// fn_0067DF00(corner, vertices, triangle): corner 0 = (t[0], t[1]), 1 = (t[1], t[2]), 2 = (t[2], t[0])
Edge TriangleEdge(const SourcePrimitive& primitive, const std::array<uint16_t, 3>& triangle, int corner)
{
	const auto& v = primitive.positions;
	switch (corner)
	{
	case 0:
		return MakeEdge(v[triangle[0]], v[triangle[1]]);
	case 1:
		return MakeEdge(v[triangle[1]], v[triangle[2]]);
	default:
		return MakeEdge(v[triangle[2]], v[triangle[0]]);
	}
}

/// fn_00460640 twice (0x68093E, 0x68094E): the two ends equal, float by float
bool SameEdge(const Edge& x, const Edge& y)
{
	return x.a == y.a && x.b == y.b;
}

/// The qsort comparator 0x682550: 1 when a.len >= b.len (fcompp of b against a, C0 or C3), else -1; never 0
int CompareForSort(const Edge& x, const Edge& y)
{
	return !(y.lengthSquared > x.lengthSquared) ? 1 : -1;
}

/// The bsearch comparator 0x682770: 0 for the same six floats, else 1 when key.len >= elem.len, else -1
int CompareForSearch(const Edge& key, const Edge& elem)
{
	if (SameEdge(key, elem))
	{
		return 0;
	}
	return !(elem.lengthSquared > key.lengthSquared) ? 1 : -1;
}

/// fn_00682580: bsearch, and when that misses (the comparator is not an order on equal lengths) the first equal edge
/// of a walk through all of them (0x6825A9..0x682628). -1 when there is none
int FindEdge(const std::vector<Edge>& edges, const Edge& key)
{
	if (edges.empty())
	{
		return -1;
	}
	if (const auto* found = CrtBsearch(key, edges.data(), edges.size(), CompareForSearch); found != nullptr)
	{
		return static_cast<int>(found - edges.data());
	}
	for (size_t i = 0; i < edges.size(); ++i)
	{
		if (SameEdge(key, edges[i]))
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

// ---- the queues 0xD4E320 (UR_ExplodeObject) and 0xD4E308 (UR_ExplodeObject2) ----
std::vector<QueuedMesh> g_Queue;
std::vector<QueuedMesh> g_Queue2;

/// PSysUtilityPSys +0x10: the EXPLODE_OBJECT effect (fn_006718E0)
uint32_t g_Effect = 0;

/// The ctor 0x6BECA0: MaxTrigsPerFrag (+0x2C) 15, RandomFactor (+0x30) 0.3 (DefineProperties 0x6B0C70; SF_ExplodeObject
/// sets RandomFactor 0.889381). UR_ExplodeObject2's (0x6BED00): RandomFactor (+0x2C) 0.3, MaxDepth (+0x30) 4
constexpr int k_DefaultMaxTrigsPerFrag = 15;
constexpr float k_DefaultRandomFactor = 0.3f;
constexpr int k_DefaultMaxDepth = 4;
/// 0x681068: the random part's Y is halved ([0x8AA3B4] = 0.5)
constexpr float k_RandomYFactor = 0.5f;
/// ExplodeMesh 0x6807E2 / 0x6807F1: a sub-mesh of LOD 0 (flag 0x20000000) whose status bits (0x3F0) are clear
constexpr uint32_t k_SubMeshLod0 = 0x20000000u;
constexpr uint32_t k_SubMeshStatus = 0x3F0u;

/// The piece atoms' "creator": the original's atoms have none (AtomCore::Create 0x6737F0 + fn_00674DD0) and carry their
/// RenderParticleGJMesh at +0x128. Here a creator of Kind::GJMesh (drawn by gj_mesh, at once, out of every mesh atom
/// list and Z object) and the piece in the atom's modifier data under k_PieceKey. (old way) Kind::Mesh, the mesh atoms'
/// draw of PSys/Creators/Mesh.cpp
struct PieceCreator final: Creator
{
	PieceCreator() { kind = k_PiecesAsWorldTriangles ? Kind::GJMesh : Kind::Mesh; }
};
const PieceCreator g_PieceCreator;
/// The key of the piece in Atom::modifierData (no modifier owns it)
class PieceKey final: public Modifier
{
};
const PieceKey g_PieceKey;

uint32_t g_NextPieceMesh = 0;

/// The LH3DMesh of a game object: MeshPack index of its Mesh component (the resources' hashed MeshId), -1 if none
int PackIndexOf(entt::id_type meshId)
{
	static const std::unordered_map<entt::id_type, int> k_Index = [] {
		std::unordered_map<entt::id_type, int> map;
		for (uint32_t i = 0; i < static_cast<uint32_t>(MeshId::_COUNT); ++i)
		{
			map.emplace(resources::HashIdentifier(static_cast<MeshId>(i)), static_cast<int>(i));
		}
		return map;
	}();
	const auto it = k_Index.find(meshId);
	return it != k_Index.end() ? it->second : -1;
}

// ---- (old way, !k_PiecesAsWorldTriangles) a generated L3D mesh per piece; goes once sistemas takes the new draw ----

/// (openblack guard) the bgfx vertex / index buffer handles left to the rest of the game (building fragments, feature
/// meshes, loads) when pieces are made
constexpr uint32_t k_GpuBufferReserve = 256;

/// (openblack guard) whether one more piece mesh (one vertex and one index buffer) fits in bgfx's handles. The original
/// has no such limit: AtomCore::Create 0x6737F0 is a plain allocation and DrawAt 0x67C150 draws the GJ mesh's triangles
/// one by one (Draw3DWorldTriangle 0x81C090), with no GPU buffer per piece. Here every piece is a generated mesh, and
/// bgfx has BGFX_CONFIG_MAX_VERTEX_BUFFERS / _INDEX_BUFFERS (4096) handles in all: a few beam explosions in a forest
/// make thousands of pieces that live 6 s (SF_ExplodeObject's DieAge), and past the limit createVertexBuffer gives
/// kInvalidHandle and bgfx::setName (VertexBuffer.cpp) writes the name into m_vertexBuffers[0xFFFF]: a heap corruption
/// that crashed in RtlFreeHeap a few turns later. A piece past the budget keeps its atom (it moves and fades as the
/// others) but is not drawn
bool GpuBuffersLeft()
{
	const auto* caps = bgfx::getCaps();
	if (caps == nullptr || caps->limits.maxVertexBuffers == 0 || caps->limits.maxIndexBuffers == 0)
	{
		return true; // bgfx not initialised (the tests): no mesh is made anyway
	}
	// live counts (Context::getPerfStats): the handles destroyed this frame are only freed at its end, so this errs safe
	const auto* stats = bgfx::getStats();
	const bool left = stats->numVertexBuffers + k_GpuBufferReserve < caps->limits.maxVertexBuffers &&
	                  stats->numIndexBuffers + k_GpuBufferReserve < caps->limits.maxIndexBuffers;
	static bool warned = false;
	if (!left && !warned)
	{
		warned = true;
		SPDLOG_LOGGER_WARN(spdlog::get("game"),
		                   "ExplodeObject: {} / {} vertex and {} / {} index buffers in use, the new pieces are not drawn",
		                   stats->numVertexBuffers, caps->limits.maxVertexBuffers, stats->numIndexBuffers,
		                   caps->limits.maxIndexBuffers);
	}
	return left;
}

/// The mesh of a piece (GJMesh, ctor 0x67FF20, filled by fn_0057D630): three new vertices per triangle (+8 positions,
/// +0x44 uvs, +0x80 normals: the source's normals as they are, not turned by the matrix) and the triangles (+0x6C) b,
/// b + 1, b + 2; drawn with the source primitive's material (GJMesh +0 = the primitive, 0x680C49)
entt::id_type MakePieceMesh(const SourceMesh& mesh, size_t subMesh, size_t primitive, std::vector<glm::vec3> positions,
                            std::vector<glm::vec2> uvs, std::vector<glm::vec3> normals)
{
	if (!Locator::resources::has_value() || mesh.meshId == 0)
	{
		return 0;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh.meshId) || !GpuBuffersLeft())
	{
		return 0;
	}
	const auto source = meshes.Handle(mesh.meshId);
	const auto& subMeshes = source->GetSubMeshes();
	if (subMesh >= subMeshes.size() || primitive >= subMeshes[subMesh]->GetPrimitives().size())
	{
		return 0;
	}
	graphics::L3DSubMesh::GeneratedPrimitive generated;
	generated.material = subMeshes[subMesh]->GetPrimitives()[primitive];
	generated.indices.resize(positions.size());
	for (size_t i = 0; i < positions.size(); ++i)
	{
		generated.indices[i] = static_cast<uint16_t>(i);
	}
	generated.positions = std::move(positions);
	generated.uvs = std::move(uvs);
	generated.normals = std::move(normals);
	const std::string name = "psys/explode/" + std::to_string(g_NextPieceMesh++);
	const auto id = entt::hashed_string(name.c_str()).value();
	try
	{
		meshes.Load(id, resources::L3DLoader::FromGeneratedTag {}, name, std::vector {std::move(generated)});
		// the source's skins: a mesh with embedded textures (the rock 567, the trees) keeps them
		meshes.Handle(id)->SetSkinSource(source.handle());
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "ExplodeObject: {}", e.what());
		return 0;
	}
	return id;
}

/// UR_ExplodeObject::ExplodeMesh 0x6807B0 (collection, NextGroups (+0x20), mesh, matrix, MaxTrigsPerFrag (+0x2C),
/// origin, speed): every primitive of every LOD 0 sub-mesh in pieces, one atom each
void ExplodeMesh(Effect& effect, Collection& collection, const std::vector<int>& nextGroups, const QueuedMesh& entry,
                 int maxTrigsPerFrag, float randomFactor)
{
	if (!entry.mesh)
	{
		return;
	}
	const auto& mesh = *entry.mesh;
	uint32_t made = 0;
	for (size_t s = 0; s < mesh.subMeshes.size(); ++s)
	{
		const auto& subMesh = mesh.subMeshes[s];
		if ((subMesh.flags & k_SubMeshLod0) == 0 || (subMesh.flags & k_SubMeshStatus) != 0)
		{
			continue;
		}
		for (size_t p = 0; p < subMesh.primitives.size(); ++p)
		{
			const auto& primitive = subMesh.primitives[p];
			for (const auto& triangles : SplitPrimitive(primitive, maxTrigsPerFrag))
			{
				// AtomCore::Create 0x6737F0, RenderParticleGJMesh (ctor 0x6C8A90, +0x21 = 1: the land light) at +0x128,
				// fn_00674DD0 (CommonInitNewAtom 0x674C60, AddSubCollections with NextGroups)
				auto& atom = effect.NewAtom(collection, &g_PieceCreator, nextGroups);
				std::vector<glm::vec3> positions;
				std::vector<glm::vec2> uvs;
				std::vector<glm::vec3> normals;
				positions.reserve(triangles.size() * 3);
				for (const auto t : triangles)
				{
					for (const auto index : primitive.triangles[t])
					{
						positions.push_back(primitive.positions[index]);
						uvs.push_back(primitive.uvs[index]);
						normals.push_back(primitive.normals[index]);
					}
				}
				// 0x680D93..0x680E15: every vertex through the matrix (x m0 + y m3 + z m6 + m9, ...) into the world, summed;
				// the centroid is the sum x (1 / n) (0x680E26..0x680E54)
				glm::vec3 sum(0.0f);
				for (auto& v : positions)
				{
					v = entry.position + entry.axes * v;
					sum += v;
				}
				const float inverse = 1.0f / static_cast<float>(positions.size());
				const glm::vec3 centre = sum * inverse;
				// 0x680E5A..0x680E89: the vertices about the centroid
				for (auto& v : positions)
				{
					v -= centre;
				}
				// 0x680E8B..0x680F31: the atom's rotation the identity (SetRotationMatrix 0x674120), +0x80 the centroid
				atom.rotation = glm::mat3(1.0f);
				atom.position = centre;
				// 0x680F34..0x6810AC: the velocity (+0x34), PSysRandR3 0x6729F0
				atom.velocity = PieceVelocity(centre, entry.origin, entry.speed, randomFactor, effect.RandomInBall());
				auto piece = std::make_shared<Piece>();
				piece->triangles = static_cast<uint32_t>(triangles.size());
				// GJMesh +0: the source primitive, its material (0x680C49)
				piece->source = entry.mesh;
				piece->subMesh = static_cast<uint16_t>(s);
				piece->primitive = static_cast<uint16_t>(p);
				if constexpr (k_PiecesAsWorldTriangles)
				{
					piece->positions = std::move(positions);
					piece->uvs = std::move(uvs);
					piece->normals = std::move(normals);
				}
				else
				{
					piece->meshId = MakePieceMesh(mesh, s, p, std::move(positions), std::move(uvs), std::move(normals));
				}
				atom.modifierData[&g_PieceKey] = std::move(piece);
				++made;
			}
		}
	}
	if (magic::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "ExplodeObject: mesh {} at ({:.1f}, {:.1f}, {:.1f}) in {} pieces, from ({:.1f}, {:.1f}, {:.1f}) at {:.1f}",
		                   mesh.meshId, entry.position.x, entry.position.y, entry.position.z, made, entry.origin.x,
		                   entry.origin.y, entry.origin.z, entry.speed);
	}
}

/// UR_ExplodeObject (vtable 0x938A98, an AtomCreateRule: ctor 0x6BECA0 sets +0x10 |= 6, so it counts as a creator)
class ExplodeObject final: public Modifier
{
public:
	explicit ExplodeObject(const Object& object)
	    : nextGroups(object.Array("NextGroups"))
	    , maxTrigsPerFrag(object.Int("MaxTrigsPerFrag", k_DefaultMaxTrigsPerFrag))
	    , randomFactor(object.Float("RandomFactor", k_DefaultRandomFactor))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	/// ModifyAtomCollection 0x6814E0: the queue emptied from its last entry (0x6814F5..0x68153F), each one exploded
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		while (!g_Queue.empty())
		{
			const QueuedMesh entry = std::move(g_Queue.back());
			g_Queue.pop_back();
			ExplodeMesh(effect, collection, nextGroups, entry, maxTrigsPerFrag, randomFactor);
		}
		return true;
	}
	std::vector<int> nextGroups; ///< +0x20 (AtomCreateRule's; empty in SF_ExplodeObject)
	int maxTrigsPerFrag;         ///< +0x2C
	float randomFactor;          ///< +0x30
};

/// UR_ExplodeObject2 (vtable 0x938ABC): ModifyAtomCollection 0x681560 empties 0xD4E308 the same way through fn_0067FFB0
/// (RandomFactor +0x2C, MaxDepth +0x30). Nobody fills that queue: fn_006812B0's last argument is 0 at all its callers
/// (fn_00681260 from 0x67EC86, 0x681253 and 0x6F9460; the rocks 0x67E87E), so fn_0067FFB0 never runs and is not ported
class ExplodeObject2 final: public Modifier
{
public:
	explicit ExplodeObject2(const Object& object)
	    : randomFactor(object.Float("RandomFactor", k_DefaultRandomFactor))
	    , maxDepth(object.Int("MaxDepth", k_DefaultMaxDepth))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& /*effect*/, Collection& /*collection*/, Collection::Slot& /*slot*/) const override
	{
		g_Queue2.clear(); // (no portado) fn_0067FFB0: never reached, see above
		return true;
	}
	float randomFactor;
	int maxDepth;
};

// ---- AllMeshes.g3d read for the CPU (the pack's MESHES block, the same layout components/pack reads) ----
struct PackIndex
{
	bool read {false};
	std::streamoff block {0};
	uint32_t size {0};
	std::vector<uint32_t> offsets;
};

PackIndex& Pack()
{
	static PackIndex pack;
	return pack;
}

std::unique_ptr<std::istream> OpenPack()
{
	if (!Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		return fileSystem.GetData(fileSystem.GetPath<filesystem::Path::Data>() / "AllMeshes.g3d");
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "ExplodeObject: no AllMeshes.g3d: {}", e.what());
		return nullptr;
	}
}

/// The pack: "LiOnHeAd", then blocks of a 32-character name, a size and the body; MESHES = "MKJC", the count and the
/// offsets of the meshes in the block (components/pack/src/PackFile.cpp)
bool ReadPackIndex(std::istream& stream, PackIndex& pack)
{
	stream.seekg(8);
	while (stream)
	{
		std::array<char, 32> name {};
		uint32_t size = 0;
		stream.read(name.data(), name.size());
		stream.read(reinterpret_cast<char*>(&size), sizeof(size));
		if (!stream)
		{
			return false;
		}
		const auto body = stream.tellg();
		if (std::strncmp(name.data(), "MESHES", name.size()) == 0)
		{
			std::array<char, 4> magic {};
			uint32_t count = 0;
			stream.read(magic.data(), magic.size());
			stream.read(reinterpret_cast<char*>(&count), sizeof(count));
			pack.offsets.resize(count);
			stream.read(reinterpret_cast<char*>(pack.offsets.data()), static_cast<std::streamsize>(count * sizeof(uint32_t)));
			pack.block = body;
			pack.size = size;
			return static_cast<bool>(stream);
		}
		stream.seekg(body + static_cast<std::streamoff>(size));
	}
	return false;
}
} // namespace

// ---- the split ----

std::vector<std::vector<uint32_t>> explode_object::SplitPrimitive(const SourcePrimitive& primitive, int maxTrigsPerFrag)
{
	std::vector<std::vector<uint32_t>> pieces;
	const auto count = static_cast<uint32_t>(primitive.triangles.size());
	if (count == 0)
	{
		return pieces;
	}
	// 0x68088B..0x6808E2: the three edges of every triangle (fn_0067DF00 -> fn_0067DFD0) into 0xD4E338
	std::vector<Edge> edges;
	edges.reserve(count * 3);
	for (const auto& triangle : primitive.triangles)
	{
		for (int corner = 0; corner < 3; ++corner)
		{
			edges.push_back(TriangleEdge(primitive, triangle, corner));
		}
	}
	// 0x6808F8: qsort by length (0x682550); 0x68091A..0x68099D: of a run of equal neighbours the last one kept
	CrtQsort(edges.data(), edges.size(), CompareForSort);
	std::vector<Edge> unique;
	unique.reserve(edges.size());
	for (size_t i = 0; i < edges.size(); ++i)
	{
		while (i + 1 < edges.size() && SameEdge(edges[i + 1], edges[i]))
		{
			++i;
		}
		unique.push_back(edges[i]);
	}
	// 0x6809E0..0x680AF0: per unique edge (0xD4E350, 0x30 bytes: the edge and a GJArray) the triangles that have it
	std::vector<std::vector<uint32_t>> users(unique.size());
	for (uint32_t t = 0; t < count; ++t)
	{
		for (int corner = 0; corner < 3; ++corner)
		{
			const int found = FindEdge(unique, TriangleEdge(primitive, primitive.triangles[t], corner));
			if (found >= 0)
			{
				users[static_cast<size_t>(found)].push_back(t);
			}
		}
	}
	// 0x680AF6..0x680D6C: the pieces. From the first triangle not used yet, a walk: the triangle goes in, then the first
	// unused triangle sharing one of its edges (corners 0, 1, 2, the users in their order) is the next one, while one is
	// found and the piece has no more than MaxTrigsPerFrag (so up to MaxTrigsPerFrag + 1 triangles)
	std::vector<uint8_t> used(count, 0);
	uint32_t start = 0;
	while (start < count)
	{
		auto& piece = pieces.emplace_back();
		uint32_t current = start;
		int made = 0;
		bool found = false;
		do
		{
			piece.push_back(current); // fn_0057D630
			++made;
			used[current] = 1;
			found = false;
			for (int corner = 0; corner < 3 && !found; ++corner)
			{
				const int edge = FindEdge(unique, TriangleEdge(primitive, primitive.triangles[current], corner));
				if (edge < 0)
				{
					continue;
				}
				for (const auto other : users[static_cast<size_t>(edge)])
				{
					if (used[other] == 0)
					{
						current = other;
						found = true;
						break;
					}
				}
			}
		} while (found && made <= maxTrigsPerFrag);
		// 0x680D47..0x680D68: the next start, the first unused triangle from this start on (count when there is none)
		uint32_t next = start;
		while (next < count && used[next] != 0)
		{
			++next;
		}
		start = next;
	}
	return pieces;
}

glm::vec3 explode_object::PieceVelocity(const glm::vec3& centre, const glm::vec3& origin, float speed, float randomFactor,
                                        glm::vec3 random)
{
	glm::vec3 d = centre - origin;
	if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
	{
		// 0x680F8A..0x680FCD: speed / sqrt((x x + z z) + y y)
		const float k = speed / std::sqrt((d.x * d.x + d.z * d.z) + d.y * d.y);
		d *= k;
	}
	// 0x680FD9..0x681001: |d| x RandomFactor ([edx + 0x30])
	const float length = std::sqrt((d.x * d.x + d.z * d.z) + d.y * d.y) * randomFactor;
	if (random.x != 0.0f || random.y != 0.0f || random.z != 0.0f)
	{
		const float k = length / std::sqrt((random.x * random.x + random.y * random.y) + random.z * random.z);
		random *= k;
	}
	// 0x681068: only the random part's Y is halved, then the three added to d (0x681073..0x681093)
	return {d.x + random.x, d.y + random.y * k_RandomYFactor, d.z + random.z};
}

// ---- the queues ----

void explode_object::QueueMesh(std::shared_ptr<const SourceMesh> mesh, const glm::mat3& axes, const glm::vec3& position,
                               const glm::vec3& origin, float speed, float spread, bool second)
{
	// fn_006812B0: vt 0xF8 (the 3D object's mesh) NULL -> nothing (0x6812C5); the entry at the end of its GJArray
	if (!mesh)
	{
		return;
	}
	(second ? g_Queue2 : g_Queue).push_back({std::move(mesh), axes, position, origin, speed, spread});
}

bool explode_object::QueueObject(entt::entity object, const glm::vec3& origin, float speed, float spread, bool second)
{
	// fn_00681260: an object with a 3D object (+0x40), its GetWorldMatrix (vt 0x63C) and that 3D object's mesh
	if (!Locator::entitiesRegistry::has_value() || object == entt::null)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
	const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
	if (mesh == nullptr || transform == nullptr)
	{
		return false;
	}
	// the drawn matrix of the object (RenderingSystem: T(position) R S)
	const int index = PackIndexOf(mesh->id);
	if (index < 0)
	{
		if (magic::TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "ExplodeObject: object {} has a mesh that is not the pack's, not exploded",
			                   static_cast<uint32_t>(object));
		}
		return false;
	}
	auto source = PackMesh(static_cast<uint32_t>(index));
	if (!source)
	{
		return false;
	}
	glm::mat3 axes = transform->rotation;
	axes[0] *= transform->scale.x;
	axes[1] *= transform->scale.y;
	axes[2] *= transform->scale.z;
	QueueMesh(std::move(source), axes, transform->position, origin, speed, spread, second);
	return true;
}

size_t explode_object::QueuedCount(bool second)
{
	return (second ? g_Queue2 : g_Queue).size();
}

void explode_object::GameLoopEnd()
{
	// fn_006717F0: fn_006718E0 makes the effect if PSysUtilityPSys +0x10 is NULL: PSysInterface::Create(NULL, 0x17,
	// (0, 0, 0), (0, 0, 0), 1.0, 0) (0x6718ED..0x671932)
	if (g_Effect != 0 && manager::Find(g_Effect) == nullptr)
	{
		g_Effect = 0; // gone with the PSys (a new map)
	}
	if (g_Effect == 0)
	{
		const auto file = ParticleTypeFile(ParticleType::ExplodeObject);
		if (file.empty())
		{
			return;
		}
		g_Effect = manager::StartForSpell(std::string(file), glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, nullptr);
		if (g_Effect == 0)
		{
			return;
		}
	}
	// vt 0x100 with a PSysProcessInfo of zeros (power 1.0, 0x671879); a 5 (finished: SF_ExplodeObject's MaxSpellAge 25)
	// deletes it (fn_006718C0, 0x67188E) and the next turn makes a new one
	const ProcessInfo info {};
	if (!manager::ProcessForSpell(g_Effect, info, game_clock::k_TurnSeconds))
	{
		g_Effect = 0;
	}
}

void explode_object::Clear()
{
	// PSysGlobal::OnClearMap 0x68F820 -> fn_006811D0 (0xD4E310 = 0) and fn_00681200 (0xD4E328 = 0)
	g_Queue2.clear();
	g_Queue.clear();
	g_Effect = 0;
}

// ---- the pack meshes ----

std::shared_ptr<const SourceMesh> explode_object::PackMesh(uint32_t index)
{
	static std::unordered_map<uint32_t, std::shared_ptr<const SourceMesh>> cache;
	if (const auto it = cache.find(index); it != cache.end())
	{
		return it->second;
	}
	auto& pack = Pack();
	auto stream = OpenPack();
	if (!stream)
	{
		return nullptr;
	}
	if (!pack.read)
	{
		pack.read = true;
		if (!ReadPackIndex(*stream, pack))
		{
			pack.offsets.clear();
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "ExplodeObject: no MESHES block in AllMeshes.g3d");
		}
	}
	std::shared_ptr<const SourceMesh> result;
	if (index < pack.offsets.size())
	{
		const uint32_t begin = pack.offsets[index];
		const uint32_t end = index + 1 < pack.offsets.size() ? pack.offsets[index + 1] : pack.size;
		std::vector<uint8_t> bytes(end > begin ? end - begin : 0);
		stream->clear();
		stream->seekg(pack.block + static_cast<std::streamoff>(begin));
		stream->read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		l3d::L3DFile file;
		if (!bytes.empty() && *stream && file.Open(bytes) == l3d::L3DResult::Success)
		{
			auto mesh = std::make_shared<SourceMesh>();
			mesh->meshId = resources::HashIdentifier(static_cast<MeshId>(index));
			const auto& headers = file.GetSubmeshHeaders();
			for (uint32_t s = 0; s < headers.size(); ++s)
			{
				auto& subMesh = mesh->subMeshes.emplace_back();
				std::memcpy(&subMesh.flags, &headers[s].flags, sizeof(subMesh.flags));
				const auto& vertices = file.GetVertexSpan(s);
				const auto& indices = file.GetIndexSpan(s);
				size_t firstVertex = 0;
				size_t firstIndex = 0;
				for (const auto& header : file.GetPrimitiveSpan(s))
				{
					auto& primitive = subMesh.primitives.emplace_back();
					for (uint32_t v = 0; v < header.numVertices && firstVertex + v < vertices.size(); ++v)
					{
						const auto& vertex = vertices[firstVertex + v];
						primitive.positions.emplace_back(vertex.position.x, vertex.position.y, vertex.position.z);
						primitive.uvs.emplace_back(vertex.texCoord.x, vertex.texCoord.y);
						primitive.normals.emplace_back(vertex.normal.x, vertex.normal.y, vertex.normal.z);
					}
					for (uint32_t t = 0; t < header.numTriangles && firstIndex + t * 3 + 2 < indices.size(); ++t)
					{
						const std::array<uint16_t, 3> triangle {indices[firstIndex + t * 3], indices[firstIndex + t * 3 + 1],
						                                        indices[firstIndex + t * 3 + 2]};
						if (triangle[0] < primitive.positions.size() && triangle[1] < primitive.positions.size() &&
						    triangle[2] < primitive.positions.size())
						{
							primitive.triangles.push_back(triangle);
						}
					}
					firstVertex += header.numVertices;
					firstIndex += header.numTriangles * 3;
				}
			}
			result = std::move(mesh);
		}
	}
	cache.emplace(index, result);
	return result;
}

// ---- the pieces ----

explode_object::Piece::~Piece()
{
	if (meshId != 0 && Locator::resources::has_value())
	{
		Locator::resources::value().GetMeshes().Erase(meshId);
	}
}

const Piece* explode_object::PieceOf(const Atom& atom)
{
	if (atom.creator != &g_PieceCreator)
	{
		return nullptr;
	}
	const auto it = atom.modifierData.find(&g_PieceKey);
	return it != atom.modifierData.end() ? static_cast<const Piece*>(it->second.get()) : nullptr;
}

uint32_t explode_object::LandLight(const glm::vec3& position)
{
	const auto& table = LandLightTable::Current();
	// 0x8020F8: off the map or with no block, table[255] ([0xEDDD08])
	const uint32_t none = table.GetRaw(255);
	if (!Locator::terrainSystem::has_value())
	{
		return none;
	}
	const auto& island = Locator::terrainSystem::value();
	// 0x801CB8..0x801CDC: x and z x 0.1 ([0x8AC404]), ftol. The port's cells count from the island's extent (as
	// vs_object's u_cellMap and RendererMists do); 0 for the original's maps
	const auto extent = island.GetExtent();
	const float fx = (position.x - extent.minimum.x) * 0.1f;
	const float fz = (position.z - extent.minimum.y) * 0.1f;
	const auto cx = static_cast<int32_t>(fx);
	const auto cz = static_cast<int32_t>(fz);
	const int32_t last = static_cast<int32_t>(island.GetCellsPerSide()) - 1; // 0x1FF
	if (cx < 0 || cx > last || cz < 0 || cz > last || !island.HasBlockAt(glm::u16vec2(cx, cz)))
	{
		return none;
	}
	// the cell and its neighbours of the 17 x 17 block (+8: z + 1, +0x88: x + 1, +0x90: both); table[luminosity] of each
	// (byte +3, 0xEDD90C). fn_00801C90 has two outputs: its second argument gets these lights interpolated (0x8020F0),
	// its third one the cells' own colours (dword +0 | 0xFF000000) interpolated the same way (0x801F81); DrawAt 0x67C184
	// keeps only the first (the third goes to [ebp - 0x40], unread), so only the lights are ported here.
	// (aproximado) on the map's last row / column the original reads the block's 17th cell; the port clamps to the last
	const auto lightOf = [&](int32_t x, int32_t z) {
		const auto cell = glm::u16vec2(std::min(x, last), std::min(z, last));
		return table.GetRaw(island.GetCell(cell).luminosity);
	};
	const uint32_t l00 = lightOf(cx, cz);
	const uint32_t l01 = lightOf(cx, cz + 1);
	const uint32_t l10 = lightOf(cx + 1, cz);
	const uint32_t l11 = lightOf(cx + 1, cz + 1);
	// 0x801DCB..0x801E06: the weights ftol(frac x 256) ([0x8D45CC])
	const auto wx = static_cast<int32_t>((fx - static_cast<float>(cx)) * 256.0f);
	const auto wz = static_cast<int32_t>((fz - static_cast<float>(cz)) * 256.0f);
	// per channel a + ((b - a) w >> 8), imul then shr and the channel's mask (0x801F83..0x8020E2): the red, green and
	// blue; the alpha comes out 0xFF (0x8020E9)
	const auto lerp = [](uint32_t a, uint32_t b, int32_t w) {
		uint32_t out = 0;
		for (const int shift : {16, 8, 0})
		{
			const auto ca = static_cast<int32_t>((a >> shift) & 0xFFu);
			const auto cb = static_cast<int32_t>((b >> shift) & 0xFFu);
			out |= (static_cast<uint32_t>(ca + ((cb - ca) * w >> 8)) & 0xFFu) << shift;
		}
		return out;
	};
	const uint32_t z0 = lerp(l00, l01, wz);
	const uint32_t z1 = lerp(l10, l11, wz);
	return lerp(z0, z1, wx) | 0xFF000000u;
}

uint32_t explode_object::LitColour(uint32_t argb, const glm::vec3& position)
{
	const uint32_t light = LandLight(position);
	uint32_t out = 0;
	for (const int shift : {24, 16, 8, 0})
	{
		out |= ((((argb >> shift) & 0xFFu) * ((light >> shift) & 0xFFu)) >> 8) << shift;
	}
	return out;
}

// ---- the pieces' draw, RenderParticleGJMesh::DrawAt 0x67C150 ----

uint32_t gj_mesh::DrawDataColour(const Effect::DrawAtom& atom)
{
	// fn_00679920: DrawData +8, the atom's colour and its alpha byte
	const auto alphaByte = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
	return (alphaByte << 24) | (static_cast<uint32_t>(atom.colour[0]) << 16) | (static_cast<uint32_t>(atom.colour[1]) << 8) |
	       atom.colour[2];
}

glm::mat4 gj_mesh::DrawMatrix(const Effect::DrawAtom& atom)
{
	// fn_00679920: the PSR matrix (rotation x scale, the Y axis x the stretch): the LHMatrix rows are the columns here
	glm::mat3 axes = atom.rotation * atom.scale;
	axes[1] *= atom.stretch;
	glm::mat4 model(axes);
	model[3] = glm::vec4(atom.position, 1.0f);
	return model;
}

void gj_mesh::AppendPiece(graphics::world_triangles::Frame& out, const Piece& piece, const glm::mat4& model, uint32_t argb,
                          const void* tag)
{
	namespace wt = graphics::world_triangles;
	const size_t count = piece.positions.size();
	if (count == 0 || !piece.source)
	{
		return;
	}
	// +0x21 (set by ExplodeMesh 0x680B9B): the DrawData colour x the land light under the matrix's translation
	// (fn_00801C90 on DrawData+4 +0x24, 0x67C175..0x67C184), byte by byte (0x67C189..0x67C1F6)
	const uint32_t base = LitColour(argb, glm::vec3(model[3]));
	// 0x67C4CE..0x67C4E4: the model light needs [0xC029C0] (1 in .data) and a normal per vertex (+0x88 == +0x10)
	const bool lit = piece.normals.size() == count;
	// 0x67C508..0x67C5E4: the light [0xEA9E90] as a point through SetInverse 0x7FB290 of the drawn matrix, normalised
	// unless null. (aproximado) model_light::LightInMeshSpace sums the squares x, y, z where 0x67C5AE..0x67C5BF does
	// (x x + z z) + y y: the last bit of the float may differ
	const glm::vec3 light = lit ? model_light::LightInMeshSpace(model) : glm::vec3(0.0f);
	const int ambient = model_light::Ambient(); // [0xC39264] (0x67C635 / 0x67C63D)
	const glm::vec3 r0(model[0]);
	const glm::vec3 r1(model[1]);
	const glm::vec3 r2(model[2]);
	const glm::vec3 t(model[3]);
	std::vector<wt::Vertex> vertices(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto& v = piece.positions[i];
		// 0x67C29B..0x67C303: ((z r2 + y r1) + x r0) + t per axis, into 0xD4E2A0
		const glm::vec3 world((v.z * r2.x + v.y * r1.x) + v.x * r0.x + t.x, (v.z * r2.y + v.y * r1.y) + v.x * r0.y + t.y,
		                      (v.z * r2.z + v.y * r1.z) + v.x * r0.z + t.z);
		uint32_t colour = base; // 0x67C47F..0x67C4C4: no colours of its own, all of the base colour
		if (lit)
		{
			// 0x67C602..0x67C62C: I = fistp(255 ((l.z n.z + l.y n.y) + l.x n.x)); 0x67C631..0x67C6A3: f and RGB x f >> 8,
			// the alpha kept
			const auto& n = piece.normals[i];
			const float dot = (light.z * n.z + light.y * n.y) + light.x * n.x;
			colour = model_light::Apply(base, model_light::Intensity(dot), ambient);
		}
		const glm::vec2 uv = i < piece.uvs.size() ? piece.uvs[i] : glm::vec2(0.0f);
		vertices[i] = {world, uv, wt::ToAbgr(colour)};
	}
	// 0x67C9B7..0x67C9C0: the DrawData alpha byte (+0xB) not 0xFF -> g_set_render_mode_data = 0xC387C8 for this draw
	const auto alphaByte = static_cast<uint8_t>(argb >> 24);
	const auto table =
	    alphaByte != 0xFFu ? graphics::render_modes::Table::GlobalAlpha : graphics::render_modes::Table::Normal;
	// Draw3DWorldTriangle 0x81C090 with the GJMesh's material (+0, the source primitive) at 0x67C9DD..0x67C9F2.
	// (inferido) the pieces have no second colour set (+0x30 / +0x38, fn_0057D630), so never the 0x67CAEE branch
	out.Append({piece.source->meshId, piece.subMesh, piece.primitive}, table, alphaByte, vertices, tag);
}

void gj_mesh::Build(graphics::world_triangles::Frame& out, DrawPath path)
{
	if constexpr (!k_PiecesAsWorldTriangles)
	{
		return;
	}
	for (const auto& drawable : manager::Collect(Creator::Kind::GJMesh))
	{
		if (drawable.path != path)
		{
			continue;
		}
		for (const auto& atom : drawable.atoms)
		{
			if (path == DrawPath::Sorted)
			{
				if (const auto* piece = atom.atom != nullptr ? PieceOf(*atom.atom) : nullptr; piece != nullptr)
				{
					AppendPiece(out, *piece, DrawMatrix(atom), DrawDataColour(atom));
				}
			}
			else
			{
				BuildAtom(out, atom);
			}
		}
	}
}

void gj_mesh::BuildAtom(graphics::world_triangles::Frame& out, const Effect::DrawAtom& atom)
{
	if (const auto* piece = atom.atom != nullptr ? PieceOf(*atom.atom) : nullptr; piece != nullptr)
	{
		AppendPiece(out, *piece, DrawMatrix(atom), DrawDataColour(atom), atom.atom);
	}
}

void explode_object::RegisterRules()
{
	RegisterModifier("UR_ExplodeObject", MakeModifierOf<ExplodeObject>);
	RegisterModifier("UR_ExplodeObject2", MakeModifierOf<ExplodeObject2>);
}
