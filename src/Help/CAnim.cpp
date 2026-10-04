/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CAnim.h"

#include <cstring>

#include <array>
#include <limits>

#include <glm/mat3x3.hpp>
#include <glm/mat4x3.hpp>

#include "3D/ObjectMatrix.h"

namespace openblack::help
{

namespace
{
constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();

bool ReadU32(const uint8_t* data, size_t size, size_t& offset, uint32_t& out)
{
	if (size < 4 || offset > size - 4)
	{
		return false;
	}
	std::memcpy(&out, data + offset, 4);
	offset += 4;
	return true;
}

bool ReadVec3(const uint8_t* data, size_t size, size_t& offset, glm::vec3& out)
{
	if (size < 12 || offset > size - 12)
	{
		return false;
	}
	std::memcpy(&out.x, data + offset, 4);
	std::memcpy(&out.y, data + offset + 4, 4);
	std::memcpy(&out.z, data + offset + 8, 4);
	offset += 12;
	return true;
}

float AsFloat(uint32_t bits)
{
	float value = 0.0f;
	std::memcpy(&value, &bits, 4);
	return value;
}

/// A 3x3 in the original's layout: rows[i] = row i of the LHMatrix (= column i of the glm matrix), row vectors
struct Rows
{
	std::array<glm::vec3, 3> r {};
};

Rows RowsOf(const glm::mat4& m)
{
	return {{glm::vec3(m[0]), glm::vec3(m[1]), glm::vec3(m[2])}};
}

void SetRows(glm::mat4& m, const Rows& rows)
{
	for (int i = 0; i < 3; ++i)
	{
		m[i] = glm::vec4(rows.r.at(static_cast<size_t>(i)), 0.0f);
	}
}

/// X * Y with row vectors, the sum order of the inlined products (x * Y0 + y * Y1 + z * Y2)
Rows Mul(const Rows& x, const Rows& y)
{
	Rows out;
	for (size_t i = 0; i < 3; ++i)
	{
		out.r.at(i) = x.r.at(i).x * y.r[0] + x.r.at(i).y * y.r[1] + x.r.at(i).z * y.r[2];
	}
	return out;
}

Rows Transposed(const Rows& m)
{
	Rows out;
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			out.r.at(static_cast<size_t>(i))[j] = m.r.at(static_cast<size_t>(j))[i];
		}
	}
	return out;
}

glm::mat3 ToMat3(const Rows& m)
{
	return {m.r[0], m.r[1], m.r[2]};
}

Rows FromMat3(const glm::mat3& m)
{
	return {{m[0], m[1], m[2]}};
}

/// LHMatrix::SetYXZMatrixOnly 0x7FAC10 (lh_matrix::YXZ, its cells and precisions); fn_00860E00 and fn_00861EE0 pass
/// (rot.y, rot.x, rot.z)
Rows KeyRotation(const glm::vec3& angles)
{
	return FromMat3(lh_matrix::YXZ(angles.y, angles.x, angles.z));
}

/// The lerp of the two keys' matrices, element by element ((m1 - m0) f + m0), rows normalised (0x8620AC..0x8621B8,
/// 0x860FA9..0x8610B2)
Rows LerpedRotation(const CAnim& clip, const KeySample& keys, size_t channel)
{
	const Rows m0 = KeyRotation(clip.frames[keys.key0].rotations[channel]);
	const Rows m1 = KeyRotation(clip.frames[keys.key1].rotations[channel]);
	Rows out;
	for (size_t i = 0; i < 3; ++i)
	{
		out.r.at(i) = (m1.r.at(i) - m0.r.at(i)) * keys.fraction + m0.r.at(i);
	}
	// fn_007FB5C0 (lh_matrix::NormaliseRows: InverseSquareRoot 0x841170 with its 128-byte table)
	glm::mat3 rows = ToMat3(out);
	lh_matrix::NormaliseRows(rows);
	return FromMat3(rows);
}

glm::vec3 LerpedPosition(const CAnim& clip, const KeySample& keys, size_t channel)
{
	const glm::vec3& p0 = clip.frames[keys.key0].positions[channel];
	const glm::vec3& p1 = clip.frames[keys.key1].positions[channel];
	return (p1 - p0) * keys.fraction + p0;
}

bool ParentOf(const RestSkeleton& rest, uint32_t bone, uint32_t& parent)
{
	parent = rest.parents[bone];
	return parent != k_NoParent && parent < rest.parents.size();
}

} // namespace

bool ReadCAnim(const uint8_t* data, size_t size, size_t& offset, CAnim& out, std::string* error)
{
	const auto fail = [&](const char* what) {
		if (error != nullptr)
		{
			*error = what;
		}
		return false;
	};
	// 0x860878..0x860965: eleven u32
	std::array<uint32_t, 11> header {};
	for (auto& word : header)
	{
		if (!ReadU32(data, size, offset, word))
		{
			return fail("CAnim: header past the end of the data");
		}
	}
	out = {};
	out.durationMs = static_cast<int32_t>(header[0]);
	out.looping = (header[1] & 1u) != 0;
	out.speed = AsFloat(header[2]);
	out.distance = AsFloat(header[3]);
	out.displacement = {AsFloat(header[4]), AsFloat(header[5]), AsFloat(header[6])};
	out.frameCount = header[7];
	out.boneCount = header[8];
	const uint32_t rotationCount = header[9];
	const uint32_t positionCount = header[10];
	// a sanity bound before the allocations (the original allocates whatever the file says, 0x86098B)
	const uint64_t needed = 4ull * rotationCount + 4ull * positionCount +
	                        12ull * (static_cast<uint64_t>(rotationCount) + positionCount) * out.frameCount;
	if (needed > size - offset || out.frameCount > size)
	{
		return fail("CAnim: channels past the end of the data");
	}
	// 0x8609B8 / 0x8609E3: the channel -> bone tables
	out.rotationBones.resize(rotationCount);
	for (auto& bone : out.rotationBones)
	{
		ReadU32(data, size, offset, bone);
	}
	out.positionBones.resize(positionCount);
	for (auto& bone : out.positionBones)
	{
		ReadU32(data, size, offset, bone);
	}
	// 0x860A1D..0x860A61: CFrame::ReadBinary 0x860C30 per key, rotations then positions
	out.frames.resize(out.frameCount);
	for (auto& frame : out.frames)
	{
		frame.rotations.resize(rotationCount);
		for (auto& rotation : frame.rotations)
		{
			ReadVec3(data, size, offset, rotation);
		}
		frame.positions.resize(positionCount);
		for (auto& position : frame.positions)
		{
			ReadVec3(data, size, offset, position);
		}
	}
	return true;
}

size_t CAnimBinarySize(const CAnim& clip)
{
	const size_t channels = clip.rotationBones.size() + clip.positionBones.size();
	return 0x2C + 4 * channels + 12 * channels * clip.frames.size();
}

RestSkeleton MakeRestSkeleton(const std::vector<uint32_t>& parents, const std::vector<glm::mat4>& local)
{
	RestSkeleton rest;
	rest.parents = parents;
	rest.local = local;
	rest.world.resize(local.size());
	rest.worldInverse.resize(local.size());
	float highest = 0.0f;
	float lowest = 0.0f;
	for (size_t i = 0; i < local.size(); ++i)
	{
		const uint32_t parent = i < parents.size() ? parents[i] : k_NoParent;
		// fn_007FAFF0 (0x83A266): the bone's matrix times its parent's result (or the identity root)
		rest.world[i] = parent != k_NoParent && parent < i ? rest.world[parent] * local[i] : local[i];
		const float y = rest.world[i][3].y; // +0x28 (0x83A26B)
		highest = y > highest ? y : highest;
		lowest = y < lowest ? y : lowest;
		// LHMatrix::SetInverse 0x7FB290 (lh_matrix::Inverse) of the 3 rows and the translation
		rest.worldInverse[i] = glm::mat4(lh_matrix::Inverse(glm::mat4x3(rest.world[i])));
	}
	rest.height = highest - lowest;
	return rest;
}

KeySample SampleKeys(const CAnim& clip, int32_t milliseconds, bool clampBelow)
{
	KeySample keys;
	const auto frames = static_cast<int32_t>(clip.frames.size());
	if (frames == 0)
	{
		return keys;
	}
	// (inferred) guards: a one-shot clip of one key or a zero period divide by zero in the original
	int32_t period = clip.looping || frames < 2 ? clip.durationMs : clip.durationMs * frames / (frames - 1);
	if (period <= 0)
	{
		return keys;
	}
	if (!clampBelow && milliseconds < 0)
	{
		milliseconds = 0;
	}
	int32_t key0 = frames * milliseconds / period;
	if (key0 < 0)
	{
		key0 = 0; // 0x860E42 (fn_00860E00 only)
	}
	if (key0 >= frames)
	{
		key0 = frames - 1;
	}
	int32_t key1 = key0 + 1;
	if (key1 == frames)
	{
		key1 = 0;
	}
	keys.key0 = static_cast<uint32_t>(key0);
	keys.key1 = static_cast<uint32_t>(key1);
	// fild frames / fidiv period * fimul t - fisub key0, stored as a float (key0 after its clamp): 0x860E68..0x860E8E
	// (fn_00861EE0 0x861F42..0x861F68). The FPU is at 24 bits (fn_007DEE00): each step rounds to a float, the integer
	// operands used whole
	float fraction = static_cast<float>(frames) / static_cast<float>(period);                          // fidiv
	fraction = static_cast<float>(static_cast<double>(fraction) * static_cast<double>(milliseconds)); // fimul
	keys.fraction = static_cast<float>(static_cast<double>(fraction) - static_cast<double>(key0));    // fisub
	return keys;
}

void SetPose(const CAnim& clip, int32_t milliseconds, const RestSkeleton& rest, const CFrame* fill,
             std::vector<glm::mat4>& local)
{
	if (clip.frames.empty() || local.size() < rest.parents.size())
	{
		return;
	}
	const KeySample keys = SampleKeys(clip, milliseconds, true);
	size_t rotationChannel = 0;
	size_t positionChannel = 0;
	for (uint32_t bone = 0; bone < clip.boneCount && bone < rest.parents.size(); ++bone)
	{
		uint32_t parent = 0;
		const bool hasParent = bone != 0 && ParentOf(rest, bone, parent);
		auto& matrix = local[bone];
		const bool rotated = rotationChannel < clip.rotationBones.size() && clip.rotationBones[rotationChannel] == bone;
		if (rotated || (fill != nullptr && bone < fill->rotations.size()))
		{
			// channel: the lerped key (0x8610B7..0x8612B9); no channel and the fill flag: the fill key of this bone,
			// indexed by bone (0x86148C..0x8616B9). Rest world (bone) x key, then x inverse rest world (parent) if the
			// bone is not bone 0 (0x8612BC / 0x8616BC).
			const Rows key = rotated ? LerpedRotation(clip, keys, rotationChannel) : KeyRotation(fill->rotations[bone]);
			Rows result = Mul(RowsOf(rest.world[bone]), key);
			if (hasParent)
			{
				result = Mul(result, RowsOf(rest.worldInverse[parent]));
			}
			SetRows(matrix, result);
		}
		if (rotated)
		{
			++rotationChannel;
		}
		const bool moved = positionChannel < clip.positionBones.size() && clip.positionBones[positionChannel] == bone;
		if (moved)
		{
			matrix[3] = glm::vec4(LerpedPosition(clip, keys, positionChannel), 1.0f); // 0x861964..0x86198E
			++positionChannel;
		}
		else if (fill != nullptr && bone < fill->positions.size())
		{
			matrix[3] = glm::vec4(fill->positions[bone], 1.0f); // 0x8619AB..0x8619D5
		}
	}
}

void ApplyAdditive(const CAnim& clip, int32_t milliseconds, const CFrame& reference, const RestSkeleton& rest,
                   std::vector<glm::mat4>& local)
{
	if (clip.frames.empty() || local.size() < rest.parents.size() ||
	    reference.rotations.size() < clip.rotationBones.size() || reference.positions.size() < clip.positionBones.size())
	{
		return;
	}
	const KeySample keys = SampleKeys(clip, milliseconds, false);
	size_t rotationChannel = 0;
	size_t positionChannel = 0;
	for (uint32_t bone = 0; bone < clip.boneCount && bone < rest.parents.size(); ++bone)
	{
		uint32_t parent = 0;
		const bool hasParent = bone != 0 && ParentOf(rest, bone, parent);
		auto& matrix = local[bone];
		if (rotationChannel < clip.rotationBones.size() && clip.rotationBones[rotationChannel] == bone)
		{
			const Rows key = LerpedRotation(clip, keys, rotationChannel);
			// fn_007FACC0 (-z, -x, -y) = the transpose of the reference key's YXZ matrix (0x862086..0x8620A7)
			const Rows inverseReference = Transposed(KeyRotation(reference.rotations[rotationChannel]));
			Rows result = RowsOf(matrix);
			if (hasParent)
			{
				result = Mul(result, RowsOf(rest.world[parent])); // 0x8621C9..0x86233A
			}
			result = Mul(result, inverseReference); // 0x86233D..0x8625A2
			result = Mul(result, key);              // 0x8625A5..0x862718
			if (hasParent)
			{
				result = Mul(result, RowsOf(rest.worldInverse[parent])); // 0x862721..0x86289D
			}
			SetRows(matrix, result);
			++rotationChannel;
		}
		if (positionChannel < clip.positionBones.size() && clip.positionBones[positionChannel] == bone)
		{
			// 0x8628DB..0x8629AF: the lerped key minus the reference key, added to the translation
			const glm::vec3 delta = LerpedPosition(clip, keys, positionChannel) - reference.positions[positionChannel];
			matrix[3] += glm::vec4(delta, 0.0f);
			++positionChannel;
		}
	}
}

void SampleLocal(const CAnim& clip, float milliseconds, const RestSkeleton& rest, std::vector<glm::mat4>& local)
{
	local = rest.local;
	SetPose(clip, static_cast<int32_t>(milliseconds), rest, nullptr, local);
}

void ComposeWorld(const RestSkeleton& rest, const std::vector<glm::mat4>& local, const glm::mat4& root,
                  std::vector<glm::mat4>& world)
{
	world.resize(local.size());
	for (size_t i = 0; i < local.size(); ++i)
	{
		const uint32_t parent = i < rest.parents.size() ? rest.parents[i] : k_NoParent;
		world[i] = (parent != k_NoParent && parent < i ? world[parent] : root) * local[i];
	}
}

ApplyAnimArgs ApplyAnimArguments(const CAnim& clip, float phase, float referencePhase, bool wrap)
{
	ApplyAnimArgs args;
	if (wrap)
	{
		phase -= static_cast<float>(static_cast<int32_t>(phase)); // 0x5BB9A8..0x5BB9B9
	}
	else if (phase > 1.0f)
	{
		phase = 1.0f; // 0x5BBB26..0x5BBB37
	}
	if (phase < 0.0f)
	{
		phase = 0.0f; // 0x5BB9BD..0x5BB9CE
	}
	args.phase = phase;
	const int32_t duration = clip.durationMs;
	int32_t milliseconds = static_cast<int32_t>(static_cast<float>(duration) * phase);
	auto key = static_cast<int32_t>(static_cast<float>(static_cast<int32_t>(clip.frameCount) - 1) * referencePhase);
	if (milliseconds >= duration)
	{
		milliseconds = duration - 1;
	}
	if (key >= duration)
	{
		key = duration - 1;
	}
	args.milliseconds = milliseconds;
	args.referenceKey = key > 0 ? static_cast<uint32_t>(key) : 0;
	args.rootMove = phase * clip.displacement; // 0x5BBA19..0x5BBA3C
	return args;
}

} // namespace openblack::help
