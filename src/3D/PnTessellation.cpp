/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PnTessellation.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <tuple>
#include <unordered_map>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>

namespace openblack::graphics
{
namespace
{
struct Corner
{
	glm::vec3 p; // welded rest position
	glm::vec3 n; // welded rest normal
};

// Inner control point of the edge from a to b, next to a (b210 for a = 1, b = 2)
glm::vec3 EdgeControl(const Corner& a, const Corner& b)
{
	return (2.0f * a.p + b.p - glm::dot(b.p - a.p, a.n) * a.n) / 3.0f;
}

// Quadratic normal control of the edge a-b (n110)
glm::vec3 EdgeNormal(const Corner& a, const Corner& b)
{
	const auto d = b.p - a.p;
	const float lengthSquared = glm::dot(d, d);
	const float v = lengthSquared > 1e-12f ? 2.0f * glm::dot(d, a.n + b.n) / lengthSquared : 0.0f;
	const auto n = a.n + b.n - v * d;
	const float length = glm::length(n);
	return length > 1e-6f ? n / length : a.n;
}

// Point and normal of the PN triangle (a, b, c) at barycentric (u, v, w)
std::pair<glm::vec3, glm::vec3> Evaluate(const Corner& a, const Corner& b, const Corner& c, float u, float v, float w)
{
	const auto b210 = EdgeControl(a, b);
	const auto b120 = EdgeControl(b, a);
	const auto b021 = EdgeControl(b, c);
	const auto b012 = EdgeControl(c, b);
	const auto b102 = EdgeControl(c, a);
	const auto b201 = EdgeControl(a, c);
	const auto e = (b210 + b120 + b021 + b012 + b102 + b201) / 6.0f;
	const auto centre = (a.p + b.p + c.p) / 3.0f;
	const auto b111 = e + (e - centre) / 2.0f;

	const auto position = a.p * (u * u * u) + b.p * (v * v * v) + c.p * (w * w * w) + b210 * (3.0f * u * u * v) +
	                      b120 * (3.0f * u * v * v) + b201 * (3.0f * u * u * w) + b021 * (3.0f * v * v * w) +
	                      b102 * (3.0f * u * w * w) + b012 * (3.0f * v * w * w) + b111 * (6.0f * u * v * w);
	auto normal = a.n * (u * u) + b.n * (v * v) + c.n * (w * w) + EdgeNormal(a, b) * (u * v) + EdgeNormal(b, c) * (v * w) +
	              EdgeNormal(c, a) * (w * u);
	const float length = glm::length(normal);
	normal = length > 1e-6f ? normal / length : a.n;
	return {position, normal};
}
} // namespace

bool TessellatePn(std::vector<PnVertex>& vertices, std::vector<uint16_t>& indices, std::vector<PnRange>& ranges,
                  const std::vector<glm::mat4>& restBones, int level)
{
	if (level < 2 || vertices.empty())
	{
		return false;
	}
	const auto boneMatrix = [&restBones](int16_t bone) {
		return bone >= 0 && static_cast<size_t>(bone) < restBones.size() ? restBones[bone] : glm::mat4(1.0f);
	};

	// Rest pose, welded by position
	std::vector<uint32_t> weldOf(vertices.size());
	std::vector<Corner> welded;
	{
		std::map<std::tuple<int64_t, int64_t, int64_t>, uint32_t> byPosition;
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			const auto matrix = boneMatrix(vertices[i].bone);
			const auto p = glm::vec3(matrix * glm::vec4(vertices[i].position, 1.0f));
			const auto n = glm::mat3(matrix) * vertices[i].normal;
			const auto key = std::make_tuple(std::llround(p.x * 1e4), std::llround(p.y * 1e4), std::llround(p.z * 1e4));
			const auto [entry, added] = byPosition.try_emplace(key, static_cast<uint32_t>(welded.size()));
			if (added)
			{
				welded.push_back({p, glm::vec3(0.0f)});
			}
			weldOf[i] = entry->second;
			welded[entry->second].n += glm::length(n) > 1e-6f ? glm::normalize(n) : glm::vec3(0.0f);
		}
		for (size_t i = 0; i < welded.size(); ++i)
		{
			const float length = glm::length(welded[i].n);
			welded[i].n = length > 1e-6f ? welded[i].n / length : glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}

	std::vector<PnVertex> out = vertices;
	// the corners keep their vertex, with the shared normal
	for (size_t i = 0; i < out.size(); ++i)
	{
		const auto normal = glm::inverse(glm::mat3(boneMatrix(out[i].bone))) * welded[weldOf[i]].n;
		out[i].normal = glm::normalize(normal);
	}
	const auto addVertex = [&](const glm::vec3& p, const glm::vec3& n, const glm::vec2& uv, int16_t bone) {
		const auto matrix = boneMatrix(bone);
		const auto inverse = glm::inverse(matrix);
		PnVertex vertex;
		vertex.position = glm::vec3(inverse * glm::vec4(p, 1.0f));
		vertex.normal = glm::normalize(glm::inverse(glm::mat3(matrix)) * n);
		vertex.uv = uv;
		vertex.bone = bone;
		out.push_back(vertex);
		return static_cast<uint32_t>(out.size() - 1);
	};

	// the points inside an edge, shared by the triangles on the same side of a UV seam and computed from the welded
	// corners in one order, so both sides of any edge get the same positions
	std::unordered_map<uint64_t, uint32_t> edgePoints;
	const auto edgePoint = [&](uint32_t p, uint32_t q, int steps) { // steps of `level` from p towards q
		if (p > q)
		{
			std::swap(p, q);
			steps = level - steps;
		}
		const uint64_t key = (static_cast<uint64_t>(p) << 40) | (static_cast<uint64_t>(q) << 16) | static_cast<uint64_t>(steps);
		if (const auto found = edgePoints.find(key); found != edgePoints.end())
		{
			return found->second;
		}
		const float t = static_cast<float>(steps) / static_cast<float>(level);
		auto a = welded[weldOf[p]];
		auto b = welded[weldOf[q]];
		float u = 1.0f - t;
		if (weldOf[p] > weldOf[q])
		{
			std::swap(a, b);
			u = t;
		}
		const auto [position, normal] = Evaluate(a, b, b, u, 1.0f - u, 0.0f);
		const auto uv = glm::mix(vertices[p].uv, vertices[q].uv, t);
		const auto bone = t <= 0.5f ? vertices[p].bone : vertices[q].bone;
		const auto index = addVertex(position, normal, uv, bone);
		edgePoints.emplace(key, index);
		return index;
	};

	std::vector<uint32_t> newIndices;
	std::vector<PnRange> newRanges;
	for (const auto& range : ranges)
	{
		PnRange newRange {static_cast<uint32_t>(newIndices.size()), 0};
		for (uint32_t t = range.indicesOffset; t + 2 < range.indicesOffset + range.indicesCount; t += 3)
		{
			const uint32_t a = indices[t];
			const uint32_t b = indices[t + 1];
			const uint32_t c = indices[t + 2];
			// A joint triangle (corners on different bones) stretches when the limb bends: a new vertex inside it, stuck
			// to one bone, folded the surface. Only an edge with both corners on one bone is curved and split (its
			// points move rigidly with that bone); a joint triangle is a fan from its other corner onto that edge, or
			// stays as it is when no edge has a single bone.
			const auto sameBone = [&vertices](uint32_t p, uint32_t q) { return vertices[p].bone == vertices[q].bone; };
			if (!(sameBone(a, b) && sameBone(b, c)))
			{
				const std::array<uint32_t, 3> corners = {a, b, c};
				int edge = -1; // the corner the single-bone edge starts at, in the triangle's order
				for (int e = 0; e < 3; ++e)
				{
					if (sameBone(corners[e], corners[(e + 1) % 3]))
					{
						edge = e;
					}
				}
				if (edge < 0)
				{
					newIndices.insert(newIndices.end(), {a, b, c});
					continue;
				}
				const uint32_t x = corners[edge];
				const uint32_t y = corners[(edge + 1) % 3];
				const uint32_t z = corners[(edge + 2) % 3];
				for (int j = 0; j < level; ++j)
				{
					const uint32_t p = j == 0 ? x : edgePoint(x, y, j);
					const uint32_t q = j + 1 == level ? y : edgePoint(x, y, j + 1);
					newIndices.insert(newIndices.end(), {p, q, z});
				}
				continue;
			}
			// grid point (i, j): i steps towards a, j towards b, the rest towards c
			std::vector<uint32_t> grid(static_cast<size_t>((level + 1) * (level + 1)), 0);
			const auto at = [&](int i, int j) -> uint32_t& { return grid[static_cast<size_t>(i * (level + 1) + j)]; };
			for (int i = 0; i <= level; ++i)
			{
				for (int j = 0; i + j <= level; ++j)
				{
					const int k = level - i - j;
					if (i == level)
					{
						at(i, j) = a;
					}
					else if (j == level)
					{
						at(i, j) = b;
					}
					else if (k == level)
					{
						at(i, j) = c;
					}
					else if (k == 0)
					{
						at(i, j) = edgePoint(a, b, j);
					}
					else if (j == 0)
					{
						at(i, j) = edgePoint(a, c, k);
					}
					else if (i == 0)
					{
						at(i, j) = edgePoint(b, c, k);
					}
					else
					{
						const float u = static_cast<float>(i) / static_cast<float>(level);
						const float v = static_cast<float>(j) / static_cast<float>(level);
						const float w = static_cast<float>(k) / static_cast<float>(level);
						const auto [position, normal] =
						    Evaluate(welded[weldOf[a]], welded[weldOf[b]], welded[weldOf[c]], u, v, w);
						const auto uv = vertices[a].uv * u + vertices[b].uv * v + vertices[c].uv * w;
						const auto bone = u >= v && u >= w ? vertices[a].bone : v >= w ? vertices[b].bone : vertices[c].bone;
						at(i, j) = addVertex(position, normal, uv, bone);
					}
				}
			}
			for (int i = 0; i < level; ++i)
			{
				for (int j = 0; i + j < level; ++j)
				{
					newIndices.insert(newIndices.end(), {at(i + 1, j), at(i, j + 1), at(i, j)});
					if (i + j + 2 <= level)
					{
						newIndices.insert(newIndices.end(), {at(i + 1, j + 1), at(i, j + 1), at(i + 1, j)});
					}
				}
			}
		}
		newRange.indicesCount = static_cast<uint32_t>(newIndices.size()) - newRange.indicesOffset;
		newRanges.push_back(newRange);
	}

	if (out.size() > std::numeric_limits<uint16_t>::max())
	{
		return false;
	}
	vertices = std::move(out);
	indices.assign(newIndices.begin(), newIndices.end());
	ranges = std::move(newRanges);
	return true;
}

} // namespace openblack::graphics
