/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimationFill.h"

#include <cmath>

#include <algorithm>

#include "3D/AffineMatrix.h"
#include "3D/ObjectMatrix.h"
#include "Common/ModelInverseSquareRoot.h"

using namespace openblack;
using namespace openblack::anim_fill;

namespace
{
/// For each cell of a product a b, row i and column j, which of the three terms a[i][k] b[k][j] the engine adds last:
/// the other two are summed first. Each site of the engine has its own order
using LastTerm = std::array<std::array<uint8_t, 3>, 3>;

constexpr LastTerm k_AllFirst {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
/// A turned bone: its rest rotation times the blended rotation (k_AllFirst), then times its parent's inverse rest
constexpr LastTerm k_TurnedByParent {{{0, 1, 1}, {0, 1, 1}, {0, 0, 0}}};
/// A bone at the stand frame: its rest rotation times the stand rotation, then times its parent's inverse rest
constexpr LastTerm k_StandTurn {{{1, 0, 0}, {1, 0, 0}, {1, 0, 0}}};
/// A lean range: the filled rotation times the parent's rest, times the base frame's inverse and the blended rotation
/// (k_AllFirst), then times the parent's inverse rest (k_LeanByParentInverse)
constexpr LastTerm k_LeanByParent {{{0, 0, 1}, {0, 0, 1}, {0, 0, 1}}};
constexpr LastTerm k_LeanByParentInverse {{{0, 1, 1}, {0, 1, 1}, {0, 1, 1}}};

Rows Product(const Rows& a, const Rows& b, const LastTerm& last)
{
	Rows out {};
	for (size_t i = 0; i < 3; ++i)
	{
		for (size_t j = 0; j < 3; ++j)
		{
			const std::array<float, 3> terms {a[3 * i] * b[j], a[3 * i + 1] * b[3 + j], a[3 * i + 2] * b[6 + j]};
			const size_t l = last[i][j];
			const size_t first = l == 0 ? 1 : 0;
			const size_t second = l == 2 ? 1 : 2;
			out[3 * i + j] = (terms[first] + terms[second]) + terms[l];
		}
	}
	return out;
}

/// The engine's Y-X-Z rotation of a keyframe's (x, y, z) angles, by rows
Rows RotationYXZ(const glm::vec3& angles)
{
	const auto m = affine::RotationYXZ(angles.y, angles.x, angles.z);
	return {m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]};
}

/// Each row times the engine's inverse square root of its length squared, (x x + y y) + z z
Rows NormaliseRows(Rows m)
{
	for (size_t row = 0; row < 3; ++row)
	{
		const float x = m[3 * row];
		const float y = m[3 * row + 1];
		const float z = m[3 * row + 2];
		const float scale = gutils::ModelInverseSquareRoot((x * x + y * y) + z * z);
		m[3 * row] = scale * x;
		m[3 * row + 1] = scale * y;
		m[3 * row + 2] = scale * z;
	}
	return m;
}

/// Two keyframes' rotations blended on their elements, (b - a) blend + a, the rows made unit length again
Rows BlendedRotation(const glm::vec3& from, const glm::vec3& to, float blend)
{
	const auto a = RotationYXZ(from);
	const auto b = RotationYXZ(to);
	Rows m {};
	for (size_t k = 0; k < m.size(); ++k)
	{
		m[k] = (b[k] - a[k]) * blend + a[k];
	}
	return NormaliseRows(m);
}

glm::vec3 BlendedPlace(const glm::vec3& from, const glm::vec3& to, float blend)
{
	return {(to.x - from.x) * blend + from.x, (to.y - from.y) * blend + from.y, (to.z - from.z) * blend + from.z};
}

Rows RowsOf(const affine::AffineMatrix& m)
{
	return {m.m[0], m.m[1], m.m[2], m.m[3], m.m[4], m.m[5], m.m[6], m.m[7], m.m[8]};
}

/// A matrix in L3DMesh's column form (column r holds row r, the fourth the place) as the engine's twelve floats
affine::AffineMatrix FromColumns(const glm::mat4& m)
{
	affine::AffineMatrix a;
	for (glm::length_t r = 0; r < 4; ++r)
	{
		for (glm::length_t k = 0; k < 3; ++k)
		{
			a.m[static_cast<size_t>(3 * r + k)] = m[r][k];
		}
	}
	return a;
}

/// Each bone's matrix carried down the skeleton: its own times its parent's carried one, the root's times the identity
std::vector<affine::AffineMatrix> CarryDown(std::span<const affine::AffineMatrix> locals, std::span<const uint32_t> parents)
{
	const affine::AffineMatrix identity;
	std::vector<affine::AffineMatrix> out(locals.size());
	for (size_t i = 0; i < locals.size(); ++i)
	{
		const auto p = parents[i];
		out[i] = affine::Mul(locals[i], p != k_NoParent && p < i ? out[p] : identity);
	}
	return out;
}
} // namespace

FrameSpan anim_fill::FindFrame(int32_t frameCount, int32_t durationMs, bool cyclic, int32_t timeMs)
{
	const int32_t count = std::max(frameCount, 1);
	int32_t duration = cyclic || count < 2 ? durationMs : (durationMs * count) / (count - 1);
	duration = std::max(duration, 1);
	const int32_t frame = std::clamp((count * timeMs) / duration, 0, count - 1);
	const int32_t next = frame + 1 == count ? 0 : frame + 1;
	const float blend =
	    (static_cast<float>(count) / static_cast<float>(duration)) * static_cast<float>(timeMs) - static_cast<float>(frame);
	return {.frame = frame, .next = next, .blend = blend};
}

Rows anim_fill::RotationZXY(float z, float x, float y)
{
	// The sine and cosine of y are kept unrounded, those of x and z stored as floats; each product of one of them is
	// rounded once, and the engine's additions of zero are kept (they turn a negative zero positive)
	const double cy = std::cos(static_cast<double>(y));
	const double sy = std::sin(static_cast<double>(y));
	const auto cx = static_cast<float>(std::cos(static_cast<double>(x)));
	const auto sx = static_cast<float>(std::sin(static_cast<double>(x)));
	const auto cz = static_cast<float>(std::cos(static_cast<double>(z)));
	const auto sz = static_cast<float>(std::sin(static_cast<double>(z)));
	const float negativeSy = -static_cast<float>(sy);
	const float sySx = static_cast<float>(sy * static_cast<double>(sx)) + 0.0f;
	const float cySx = static_cast<float>(cy * static_cast<double>(sx)) + 0.0f;
	return {
	    static_cast<float>(cy * static_cast<double>(cz)) + sySx * sz,
	    sySx * cz - static_cast<float>(cy * static_cast<double>(sz)),
	    static_cast<float>(sy * static_cast<double>(cx)),
	    cx * sz + 0.0f,
	    cx * cz,
	    0.0f - sx,
	    negativeSy * cz + cySx * sz,
	    cySx * cz - negativeSy * sz,
	    static_cast<float>(cy * static_cast<double>(cx)),
	};
}

Rest anim_fill::RestFromLocals(std::span<const glm::mat4> locals, std::span<const uint32_t> parents)
{
	std::vector<affine::AffineMatrix> own(locals.size());
	std::transform(locals.begin(), locals.end(), own.begin(), FromColumns);
	auto carried = CarryDown(own, parents);
	// The good and evil look's blend of the same matrices adds only zeros, which makes a negative zero positive
	for (auto& matrix : carried)
	{
		for (auto& value : matrix.m)
		{
			value = value + 0.0f;
		}
	}
	// Set back relative to the parents, the last bone first, each by the inverse of its parent's carried matrix
	const affine::AffineMatrix identity;
	for (size_t i = carried.size(); i-- > 0;)
	{
		const auto p = parents[i];
		carried[i] = affine::Mul(carried[i], affine::Inverse(p != k_NoParent && p < i ? carried[p] : identity));
	}
	const auto base = CarryDown(carried, parents);
	Rest rest;
	rest.parents.assign(parents.begin(), parents.end());
	rest.base.reserve(base.size());
	rest.inverseBase.reserve(base.size());
	for (const auto& matrix : base)
	{
		rest.base.push_back(RowsOf(matrix));
		rest.inverseBase.push_back(RowsOf(affine::Inverse(matrix)));
	}
	return rest;
}

void anim_fill::Fill(std::span<BoneMatrix> out, const Clip& clip, const Rest& rest, int32_t timeMs, const Frame& stand)
{
	if (clip.frames.empty())
	{
		return;
	}
	const auto span = FindFrame(static_cast<int32_t>(clip.frames.size()), clip.durationMs, clip.cyclic, timeMs);
	const auto& from = clip.frames[static_cast<size_t>(span.frame)];
	const auto& to = clip.frames[static_cast<size_t>(span.next)];
	size_t angle = 0;
	size_t position = 0;
	const auto bones = std::min<size_t>({clip.boneCount, out.size(), rest.base.size()});
	for (size_t bone = 0; bone < bones; ++bone)
	{
		auto& matrix = out[bone];
		const auto parent = rest.parents[bone];
		const bool child = bone != 0 && parent != k_NoParent;
		if (angle < clip.angleBones.size() && clip.angleBones[angle] == bone)
		{
			const auto turn = BlendedRotation(from.angles[angle], to.angles[angle], span.blend);
			matrix.rows = Product(rest.base[bone], turn, k_AllFirst);
			if (child)
			{
				matrix.rows = Product(matrix.rows, rest.inverseBase[parent], k_TurnedByParent);
			}
			++angle;
		}
		else
		{
			const auto turn = bone < stand.angles.size() ? RotationYXZ(stand.angles[bone]) : RotationYXZ(glm::vec3(0.0f));
			matrix.rows = Product(rest.base[bone], turn, k_StandTurn);
			if (child)
			{
				matrix.rows = Product(matrix.rows, rest.inverseBase[parent], k_StandTurn);
			}
		}
		if (position < clip.positionBones.size() && clip.positionBones[position] == bone)
		{
			matrix.position = BlendedPlace(from.positions[position], to.positions[position], span.blend);
			++position;
		}
		else
		{
			matrix.position = bone < stand.positions.size() ? stand.positions[bone] : glm::vec3(0.0f);
		}
	}
}

void anim_fill::AddDifferenceFromFrame(std::span<BoneMatrix> out, const Clip& clip, const Frame& baseFrame, const Rest& rest,
                                       int32_t timeMs)
{
	if (clip.frames.empty())
	{
		return;
	}
	const auto span = FindFrame(static_cast<int32_t>(clip.frames.size()), clip.durationMs, clip.cyclic, timeMs);
	const auto& from = clip.frames[static_cast<size_t>(span.frame)];
	const auto& to = clip.frames[static_cast<size_t>(span.next)];
	size_t angle = 0;
	size_t position = 0;
	const auto bones = std::min<size_t>({clip.boneCount, out.size(), rest.base.size()});
	for (size_t bone = 0; bone < bones; ++bone)
	{
		auto& matrix = out[bone];
		const auto parent = rest.parents[bone];
		const bool child = bone != 0 && parent != k_NoParent;
		if (angle < clip.angleBones.size() && clip.angleBones[angle] == bone)
		{
			const auto turn = BlendedRotation(from.angles[angle], to.angles[angle], span.blend);
			const auto& b = baseFrame.angles[angle];
			const auto inverseBaseTurn = RotationZXY(-b.z, -b.x, -b.y);
			if (child)
			{
				matrix.rows = Product(matrix.rows, rest.base[parent], k_LeanByParent);
			}
			matrix.rows = Product(matrix.rows, inverseBaseTurn, k_AllFirst);
			matrix.rows = Product(matrix.rows, turn, k_AllFirst);
			if (child)
			{
				matrix.rows = Product(matrix.rows, rest.inverseBase[parent], k_LeanByParentInverse);
			}
			++angle;
		}
		if (position < clip.positionBones.size() && clip.positionBones[position] == bone)
		{
			const auto& p0 = from.positions[position];
			const auto& p1 = to.positions[position];
			const auto& b = baseFrame.positions[position];
			const glm::vec3 move {((p1.x - p0.x) * span.blend + p0.x) - b.x, ((p1.y - p0.y) * span.blend + p0.y) - b.y,
			                      ((p1.z - p0.z) * span.blend + p0.z) - b.z};
			matrix.position = matrix.position + move;
			++position;
		}
	}
}

int32_t anim_fill::RangeTimeMs(float value, float range, int32_t durationMs)
{
	return static_cast<int32_t>(((range + value) / (range + range)) * static_cast<float>(durationMs));
}

std::vector<glm::mat4> anim_fill::ToWorld(std::span<const BoneMatrix> bones, std::span<const uint32_t> parents,
                                          const glm::mat4& object)
{
	const auto place = FromColumns(object);
	std::vector<affine::AffineMatrix> world(bones.size());
	std::vector<glm::mat4> out(bones.size());
	for (size_t i = 0; i < bones.size(); ++i)
	{
		affine::AffineMatrix own;
		std::copy(bones[i].rows.begin(), bones[i].rows.end(), own.m.begin());
		own.m[9] = bones[i].position.x;
		own.m[10] = bones[i].position.y;
		own.m[11] = bones[i].position.z;
		const auto p = parents[i];
		world[i] = affine::Mul(own, p != k_NoParent && p < i ? world[p] : place);
		const auto& m = world[i].m;
		out[i] = glm::mat4(m[0], m[1], m[2], 0.0f, m[3], m[4], m[5], 0.0f, m[6], m[7], m[8], 0.0f, m[9], m[10], m[11], 1.0f);
	}
	return out;
}

std::vector<glm::mat4> anim_fill::ToMesh(std::span<const BoneMatrix> bones, std::span<const uint32_t> parents)
{
	std::vector<affine::AffineMatrix> own(bones.size());
	for (size_t i = 0; i < bones.size(); ++i)
	{
		std::copy(bones[i].rows.begin(), bones[i].rows.end(), own[i].m.begin());
		own[i].m[9] = bones[i].position.x;
		own[i].m[10] = bones[i].position.y;
		own[i].m[11] = bones[i].position.z;
	}
	std::vector<glm::mat4> out(bones.size());
	std::vector<affine::AffineMatrix> carried(bones.size());
	for (size_t i = 0; i < bones.size(); ++i)
	{
		const auto p = parents[i];
		carried[i] = p != k_NoParent && p < i ? affine::Mul(own[i], carried[p]) : own[i];
		const auto& m = carried[i].m;
		out[i] = glm::mat4(m[0], m[1], m[2], 0.0f, m[3], m[4], m[5], 0.0f, m[6], m[7], m[8], 0.0f, m[9], m[10], m[11], 1.0f);
	}
	return out;
}
