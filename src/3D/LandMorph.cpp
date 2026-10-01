/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandMorph.h"

#include <cmath>

#include <algorithm>
#include <array>

#include "3D/LandIslandInterface.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"

using namespace openblack;

namespace
{
constexpr float k_CellSize = 10.0f;          ///< [0x8AB414]
constexpr float k_MinusTenth = -0.1f;        ///< [0x8C7B10]
constexpr float k_ColourSteps = 255.0f;      ///< [0x8AB270]
constexpr float k_BoxStart = 10000000.0f;    ///< 0x4B189680 (fn_006C99E0: min starts at it, max at minus it)
constexpr float k_Half = 0.5f;               ///< [0x8AA3B4]
constexpr float k_TwoPi = 6.28318548f;       ///< [0x8AB210]
constexpr float k_SegmentsPerMetre = 0.05f;  ///< [0x8C7BD4]
constexpr int k_MinSegments = 8;             ///< 0x826659
constexpr int k_MaxSegments = 250;           ///< 0x826666 (0xFA)
constexpr float k_MinusOneOver111 = -0.00900900830f; ///< [0x9A3930]
constexpr float k_OneOver60 = 0.0166666675f; ///< [0x8C5818]
constexpr int k_MaxTurns = 6;                ///< 0x826818
constexpr float k_CurtainMiddle = 20.0f;     ///< [0x8C7658]
constexpr float k_CurtainTop = 40.0f;        ///< [0x8CF300]
constexpr float k_RowMiddle = 0.2f;          ///< [0x8AB244]
constexpr float k_RowTop = 0.4f;             ///< [0x8C7A44]

/// __ftol: towards zero
int Ftol(float value)
{
	return static_cast<int>(value);
}

/// A + t (O - A), each step stored to a float (fn_0044CF90, the LHPoint ctor 0x442700, fn_004605F0)
template <typename T>
T Lerp(const T& a, const T& o, float t)
{
	const T difference = o - a;
	const T scaled = difference * t;
	return a + scaled;
}

/// One colour channel of fn_00686D90 (0x687185..0x68724D): cA + ((cO - cA) tb >> 8), the bits of the byte only
uint32_t LerpChannel(uint32_t a, uint32_t o, int shift, int tb)
{
	const auto ca = static_cast<int>((a >> shift) & 0xFFu);
	const auto co = static_cast<int>((o >> shift) & 0xFFu);
	return (static_cast<uint32_t>(ca + (((co - ca) * tb) >> 8)) & 0xFFu) << shift;
}

uint32_t LerpColour(uint32_t a, uint32_t o, int tb)
{
	return LerpChannel(a, o, 24, tb) | LerpChannel(a, o, 16, tb) | LerpChannel(a, o, 8, tb) | LerpChannel(a, o, 0, tb);
}

/// fn_00453F50: (z z' + y y') + x x'
float Dot(const glm::vec3& n, const glm::vec3& p)
{
	float sum = p.z * n.z;
	sum = sum + p.y * n.y;
	return sum + p.x * n.x;
}

/// The plane fn_00686980 hands fn_00686D90: (n, -n.p) (fn_0045A7D0, fn_00453F50, fchs)
glm::vec4 PlaneThrough(const glm::vec3& normal, const glm::vec3& point)
{
	return {normal, -Dot(normal, point)};
}
} // namespace

land_morph::Ground land_morph::Altitude(const LandIslandInterface& land)
{
	return [&land](glm::vec2 xz) { return land.GetHeightAt(xz); };
}

land_morph::Ground land_morph::CurrentAltitude()
{
	return [](glm::vec2 xz) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
	};
}

float land_morph::Raised(const Ground& ground, const glm::vec3& point, float originHeight)
{
	const float delta = ground(glm::vec2(point.x, point.z)) - originHeight;
	return delta + point.y;
}

void land_morph::SplitByPlane(Primitive& primitive, const glm::vec4& plane)
{
	const size_t vertexCount = primitive.positions.size();
	const bool hasNormals = primitive.normals.size() == vertexCount; // +0x88 == +0x10
	const bool hasDiffuse = primitive.diffuse.size() == vertexCount;  // +0x24 == +0x10
	const bool hasSpecular = primitive.specular.size() == vertexCount; // +0x38 == +0x10
	// (port guard) the original reads the UVs (+0x44) as if there were always one per position
	const bool hasUvs = primitive.uvs.size() == vertexCount;
	// 0x686E3B..0x686E63, into the static buffer 0xD4E730: ((y ny + z nz) + x nx) + w
	std::vector<float> distance(vertexCount);
	for (size_t k = 0; k < vertexCount; ++k)
	{
		const auto& p = primitive.positions[k];
		float d = p.y * plane.y;
		d = d + p.z * plane.z;
		d = d + p.x * plane.x;
		distance[k] = d + plane.w;
	}
	// the triangles there are now ([esp+0x38]); the ones added below are not tested again
	const size_t triangleCount = primitive.indices.size() / 3;
	for (size_t triangle = 0; triangle < triangleCount; ++triangle)
	{
		const size_t first = 3 * triangle;
		const std::array<uint32_t, 3> tri = {primitive.indices[first], primitive.indices[first + 1],
		                                     primitive.indices[first + 2]};
		// > 0 is positive, 0 counts as negative ([0x8AA398], test ah,0x41)
		std::array<int, 3> sign {};
		for (size_t k = 0; k < 3; ++k)
		{
			sign[k] = distance[tri[k]] > 0.0f ? 1 : -1;
		}
		if (sign[0] == sign[1] && sign[1] == sign[2])
		{
			continue;
		}
		const int product = sign[0] * sign[1] * sign[2];
		size_t lone = 0; // 0x686F83..0x686F9A: the first k whose sign is the product
		while (lone < 3 && sign[lone] != product)
		{
			++lone;
		}
		const uint32_t a = tri[lone];
		const uint32_t b = tri[(lone + 1) % 3];
		const uint32_t c = tri[(lone + 2) % 3];
		const auto n1 = static_cast<uint32_t>(primitive.positions.size()); // [ebp+0x10]: the edge A-B
		const uint32_t n2 = n1 + 1;                                         // the edge A-C
		for (const uint32_t o : {b, c})
		{
			// 0x686FEB..0x687006: t = -dA / (dO - dA)
			const float t = -distance[a] / (distance[o] - distance[a]);
			primitive.positions.push_back(Lerp(primitive.positions[a], primitive.positions[o], t));
			if (hasUvs)
			{
				primitive.uvs.push_back(Lerp(primitive.uvs[a], primitive.uvs[o], t)); // fn_0067FEC0, fn_0067FEE0
			}
			if (hasNormals)
			{
				primitive.normals.push_back(Lerp(primitive.normals[a], primitive.normals[o], t)); // fn_0044E9F0
			}
			const int tb = Ftol(t * k_ColourSteps) & 0xFF; // stored as a byte (0x687194)
			if (hasDiffuse)
			{
				primitive.diffuse.push_back(LerpColour(primitive.diffuse[a], primitive.diffuse[o], tb));
			}
			if (hasSpecular)
			{
				primitive.specular.push_back(LerpColour(primitive.specular[a], primitive.specular[o], tb)); // 0x68726A
			}
		}
		// 0x687386..0x687410
		primitive.indices[first] = a;
		primitive.indices[first + 1] = n1;
		primitive.indices[first + 2] = n2;
		for (const uint32_t index : {b, c, n2, b, n2, n1})
		{
			primitive.indices.push_back(index);
		}
	}
}

std::vector<glm::vec4> land_morph::CellPlanes(const glm::vec3& minimum, const glm::vec3& maximum)
{
	// fn_006C99E0 0x6C9A3C..0x6C9AF9: centre = (max + min) 0.5, half = (max - min) 0.5; fn_00686980 0x6869BE..0x6869EB:
	// min = centre - half, max = half + centre
	const glm::vec3 centre = (maximum + minimum) * k_Half;
	const glm::vec3 half = (maximum - minimum) * k_Half;
	const float minX = centre.x - half.x;
	const float minZ = centre.z - half.z;
	const float maxX = half.x + centre.x;
	const float maxZ = half.z + centre.z;
	// 0x6869EF..0x686A41: -1 - ftol(-0.1 min) and 1 - ftol(-0.1 max), floor(min / 10) - 1 .. floor(max / 10) + 1 for x >= 0
	const int x0 = -1 - Ftol(minX * k_MinusTenth);
	const int x1 = 1 - Ftol(maxX * k_MinusTenth);
	const int z0 = -1 - Ftol(minZ * k_MinusTenth);
	const int z1 = 1 - Ftol(maxZ * k_MinusTenth);
	std::vector<glm::vec4> planes;
	for (int i = x0; i <= x1; ++i) // 0x686A53: x = 10 i
	{
		planes.push_back(PlaneThrough(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(static_cast<float>(i) * k_CellSize, 0.0f, 0.0f)));
	}
	for (int j = z0; j <= z1; ++j) // 0x686AF9: z = 10 j
	{
		planes.push_back(PlaneThrough(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, static_cast<float>(j) * k_CellSize)));
	}
	// 0x686B93..0x686BC3: s = 1 / sqrt(2.0) ([0x8AC410], [0x8AA390]); both diagonals through (10 i, 0, 10 z1)
	const auto s = static_cast<float>(1.0 / std::sqrt(2.0));
	const int dz = z1 - z0;
	const float z = static_cast<float>(z1) * k_CellSize;
	for (int i = x0 - dz; i <= x1; ++i) // 0x686BDB: x + z = 10 (i + z1)
	{
		planes.push_back(PlaneThrough(glm::vec3(s, 0.0f, s), glm::vec3(static_cast<float>(i) * k_CellSize, 0.0f, z)));
	}
	const float minusS = s * -1.0f; // [0x8AB678]
	for (int i = x0; i <= x1 + dz; ++i) // 0x686C8B: x - z = 10 (i - z1)
	{
		planes.push_back(PlaneThrough(glm::vec3(s, 0.0f, minusS), glm::vec3(static_cast<float>(i) * k_CellSize, 0.0f, z)));
	}
	return planes;
}

void land_morph::RaiseAboveLandscape(const Ground& ground, std::span<Primitive> worldPrimitives, glm::vec2 originXZ)
{
	if (worldPrimitives.empty())
	{
		return; // no group (0x68699F) or no primitive in the first one (0x6869B8)
	}
	// fn_006C99E0 -> fn_006C98D0: the box of every primitive's positions
	glm::vec3 minimum(k_BoxStart);
	glm::vec3 maximum(-k_BoxStart);
	for (const auto& primitive : worldPrimitives)
	{
		for (const auto& p : primitive.positions)
		{
			minimum = glm::vec3(minimum.x < p.x ? minimum.x : p.x, minimum.y < p.y ? minimum.y : p.y,
			                    minimum.z < p.z ? minimum.z : p.z);
			maximum = glm::vec3(maximum.x > p.x ? maximum.x : p.x, maximum.y > p.y ? maximum.y : p.y,
			                    maximum.z > p.z ? maximum.z : p.z);
		}
	}
	// only the first primitive of the first group ([[mesh]], 0x6869C2)
	auto& first = worldPrimitives.front();
	for (const auto& plane : CellPlanes(minimum, maximum))
	{
		SplitByPlane(first, plane);
	}
	// 0x686D01..0x686D55: H0 at the matrix's position (M+0x24, M+0x2C), then every position
	const float originHeight = ground(originXZ);
	for (auto& p : first.positions)
	{
		p.y = Raised(ground, p, originHeight);
	}
}

void land_morph::MeltingDeltas(const Ground& ground, const glm::mat4& object, float scale,
                               std::span<const glm::vec3> modelVertices, std::span<float> outDeltas)
{
	// 0x816915..0x816977: a0 = GetAltitude(+0x38, +0x40), inv = 1 / [+0x44]
	const float originHeight = ground(glm::vec2(object[3].x, object[3].z));
	const float inverseScale = 1.0f / scale;
	const size_t count = std::min(modelVertices.size(), outDeltas.size());
	for (size_t k = 0; k < count; ++k)
	{
		const auto& v = modelVertices[k];
		// 0x8169E1..0x816A18: x = ((z m2c + y m20) + x m14) + px, z = ((x m1c + z m34) + y m28) + pz
		float x = v.z * object[2].x;
		x = x + v.y * object[1].x;
		x = x + v.x * object[0].x;
		x = x + object[3].x;
		float z = v.x * object[0].z;
		z = z + v.z * object[2].z;
		z = z + v.y * object[1].z;
		z = z + object[3].z;
		// 0x816A5E..0x816A77: -((a0 - H) inv)
		outDeltas[k] = -((originHeight - ground(glm::vec2(x, z))) * inverseScale);
	}
}

void land_morph::Bake(const Ground& ground, std::span<glm::vec3> worldVertices, float originHeight)
{
	for (auto& v : worldVertices)
	{
		v.y = Raised(ground, v, originHeight);
	}
}

void land_morph::BakeAgainstY(const Ground& ground, const glm::mat4& object, std::span<glm::vec3> modelVertices)
{
	for (auto& v : modelVertices)
	{
		// (inferido) the order of the sums of the world x, z: UpdateMelting's
		const glm::vec3 world = glm::vec3(object * glm::vec4(v, 1.0f));
		// 0x882DBE..0x882DD7: v.y - (pos.y - H)
		v.y = v.y - (object[3].y - ground(glm::vec2(world.x, world.z)));
	}
}

float land_morph::OnGround(const Ground& ground, glm::vec2 xz, float lift)
{
	return ground(xz) + lift;
}

land_morph::Curtain land_morph::InfluenceCurtain(const Ground& ground, const glm::vec3& centre, float radius, uint32_t colour)
{
	Curtain curtain;
	// 0x8265F7..0x826675: N = clamp(ftol(2 pi r 0.05), 8, 250)
	const float circumference = radius * k_TwoPi;
	int segments = Ftol(circumference * k_SegmentsPerMetre);
	segments = segments < k_MinSegments ? k_MinSegments : segments;
	segments = segments >= k_MaxSegments ? k_MaxSegments : segments;
	// 0x826760..0x8267A8: the u step, (1 - ftol(2 pi r (-1 / 111))) / N
	const float uStep = static_cast<float>(1 - Ftol(circumference * k_MinusOneOver111)) / static_cast<float>(segments);
	// 0x8267AC..0x826805: H0 at the centre, H0 + 20, H0 + 40
	const float h0 = ground(glm::vec2(centre.x, centre.z));
	const float h20 = h0 + k_CurtainMiddle;
	const float h40 = h0 + k_CurtainTop;
	// 0x826809..0x826842: the v step, min(ftol(r / 60), 6) / N
	int turns = Ftol(radius * k_OneOver60);
	turns = turns > k_MaxTurns ? k_MaxTurns : turns;
	const float vStep = static_cast<float>(turns) / static_cast<float>(segments);
	float u = 0.0f;
	float v = 0.0f;
	const auto addColumn = [&](float x, float z) {
		// (H - H0) + H0 and so on (0x8268BF..0x82691A)
		const float h = ground(glm::vec2(x, z)) - h0;
		for (const float base : {h0, h20, h40})
		{
			curtain.positions.emplace_back(x, h + base, z);
			curtain.colours.push_back(colour);
		}
		curtain.uvs.emplace_back(u, v);
		curtain.uvs.emplace_back(u, v + k_RowMiddle);
		curtain.uvs.emplace_back(u, v + k_RowTop);
	};
	for (int i = 0; i < segments; ++i)
	{
		// 0x826851..0x826878: angle = 2 pi i / N, (cos r + cx, sin r + cz)
		const float angle = static_cast<float>(i) * k_TwoPi / static_cast<float>(segments);
		addColumn(std::cos(angle) * radius + centre.x, std::sin(angle) * radius + centre.z);
		const auto b = static_cast<uint32_t>(3 * i); // 0x8269DA..0x826A34
		for (const uint32_t index : {b, b + 3, b + 4, b, b + 4, b + 1, b + 1, b + 4, b + 5, b + 1, b + 5, b + 2})
		{
			curtain.indices.push_back(index);
		}
		u = uStep + u;
		v = vStep + v;
	}
	// 0x826A57..0x826B93: the ring closed at angle 0, (r + cx, cz)
	addColumn(radius + centre.x, centre.z);
	return curtain;
}

const std::string& land_morph::ObjectProgramName(bool morph, ObjectPass pass)
{
	static const std::string k_Object = "ObjectInstanced";
	static const std::string k_ObjectMorph = "ObjectHeightMapInstanced";
	static const std::string k_Shadow = "ObjectShadowInstanced";
	static const std::string k_ShadowMorph = "ObjectHeightMapShadowInstanced";
	if (pass == ObjectPass::Shadow)
	{
		return morph ? k_ShadowMorph : k_Shadow;
	}
	return morph ? k_ObjectMorph : k_Object;
}

const graphics::ShaderProgram* land_morph::ObjectProgram(const graphics::ShaderManager& shaders, bool morph, ObjectPass pass)
{
	return shaders.GetShader(ObjectProgramName(morph, pass));
}
