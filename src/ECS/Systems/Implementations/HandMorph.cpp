/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's good / evil morph: CHand::PrepareForDrawing 0x46C550 (SetTextureSet 0x46BF60, the alignment into Morphable
// +0x9C, Morphable::UpdateMorphing 0x618C40), Morphable::MorphTexture 0x619500 with Blend4444 fn_00870640 and
// Morphable::MorphVertices 0x618D10. Research: dev\documentacion\hand\morph\README.md and coordinador's
// dev\_scratch\coordinador\spec_alignment_history.md §3. Wiki: docs/bw1-notes/hand-and-interface.md, "Good and evil".

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <L3DFile.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "ECS/Components/Hand.h"
#include "ECS/Effects/Alignment.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// fn_00870640 on one texel: each nibble floor(((A & m) (255 - t) + (B & m) t) / 255) & m (0x80808081 x, shr 7)
uint16_t Blend4444(uint16_t a, uint16_t b, uint32_t t)
{
	uint32_t out = 0;
	for (const uint32_t mask : {0xFu, 0xF0u, 0xF00u, 0xF000u})
	{
		out |= (((a & mask) * (255u - t) + (b & mask) * t) / 255u) & mask;
	}
	return static_cast<uint16_t>(out);
}
} // namespace

void HandSystem::LoadMorphMeshes() noexcept
{
	// Morphable::LoadBase 0x618360 / ReadBinary 0x617AE0: the base twice (the drawn mesh [0] and the pristine [1]), then
	// hh.HBN's variants: [2] hand_boned_evil2, [3] hand_boned_good2 ([4..7] empty)
	auto& fileSystem = Locator::filesystem::value();
	const auto dir = fileSystem.GetPath<filesystem::Path::CreatureMesh>();
	const auto load = [&](const char* name, std::unique_ptr<l3d::L3DFile>& file) {
		file = std::make_unique<l3d::L3DFile>();
		try
		{
			if (file->Open(fileSystem.FindPath(dir / name)) != l3d::L3DResult::Success)
			{
				file.reset();
			}
		}
		catch (const std::exception&)
		{
			file.reset();
		}
		if (!file)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Hand morph: cannot read {}", name);
		}
	};
	load("Hand_Boned_Base2.l3d", _morphBase);
	load("Hand_Boned_Evil2.l3d", _morphEvil);
	load("Hand_Boned_Good2.l3d", _morphGood);
	if (_morphBase && !_morphBase->GetSkins().empty())
	{
		_morphTexels.resize(_morphBase->GetSkins()[0].texels.size());
	}
}

void HandSystem::UpdateMorphing() noexcept
{
	// CHand::PrepareForDrawing 0x46C550, once a drawn frame
	if (!_morphBase)
	{
		return;
	}
	// SetTextureSet(GInterface +0x48) 0x46C60C: a change blends the texture again (the set-0 branch fn_0046BFA0 is an
	// empty ret, so the result is the same blend)
	bool textureDirty = false;
	const int32_t textureSet = InInfluence() ? 1 : 0;
	if (textureSet != _morphTextureSet)
	{
		_morphTextureSet = textureSet;
		textureDirty = true;
	}
	// 0x46C63A..0x46C665: the local player's alignment, clamped to -1..1, into Morphable +0x9C; UpdateMorphing 0x618C40
	// re-morphs when it moved 0.03 ([0x900AD4]) from the one applied (+0xA0)
	float alignment = std::clamp(ecs::effects::alignment::Get(PlayerNames::PLAYER_ONE), -1.0f, 1.0f);
	// test hook OPENBLACK_TEST_HAND_ALIGNMENT=<-1..1>: the hand morphs as if the player had that alignment
	if (static const char* test = std::getenv("OPENBLACK_TEST_HAND_ALIGNMENT"); test != nullptr)
	{
		alignment = std::clamp(static_cast<float>(std::atof(test)), -1.0f, 1.0f);
	}
	bool verticesDirty = false;
	if (std::abs(alignment - _morphApplied) >= 0.03f)
	{
		_morphApplied = alignment;
		textureDirty = true;
		verticesDirty = true;
	}
	if (textureDirty)
	{
		MorphTexture();
	}
	if (verticesDirty)
	{
		MorphVertices();
	}
	if (textureDirty || verticesDirty)
	{
		UploadMorph(textureDirty, verticesDirty);
	}
}

void HandSystem::MorphTexture() noexcept
{
	// Morphable::MorphTexture 0x619500: a < 0 -> Evil [2] with f = -a, else Good [3] with f = a (0 counts as good), the
	// base [1] when the variant is missing; t = ftol(f x 256), at most 255
	const float a = _morphApplied;
	const auto* target = a < 0.0f ? _morphEvil.get() : _morphGood.get();
	if (target == nullptr)
	{
		target = _morphBase.get();
	}
	const auto t = std::min<uint32_t>(static_cast<uint32_t>(std::abs(a) * 256.0f), 255u);
	const auto& base = _morphBase->GetSkins()[0].texels;
	const auto& variant = target->GetSkins()[0].texels;
	for (size_t i = 0; i < _morphTexels.size(); ++i)
	{
		uint16_t baseTexel = 0;
		uint16_t variantTexel = 0;
		std::memcpy(&baseTexel, &base[i], sizeof(uint16_t));
		std::memcpy(&variantTexel, &variant[i], sizeof(uint16_t));
		_morphTexels[i] = Blend4444(baseTexel, variantTexel, t);
	}
}

void HandSystem::MorphVertices() noexcept
{
	// Morphable::MorphVertices 0x618D10: out = base + w (m - base) on the position (+0) and the normal (+0x14), w = |a|,
	// m the variant of the same rule as the texture; the hand has no second or third axis (hh.HBN slots [4..7] are empty).
	// Morphable::MorphAnims 0x619100 changes nothing for the hand (the three files have the same bones and hh.HBN's evil
	// and good animation tables are empty)
	const float a = _morphApplied;
	const auto* target = a < 0.0f ? _morphEvil.get() : _morphGood.get();
	if (target == nullptr)
	{
		target = _morphBase.get();
	}
	const float w = std::abs(a);
	const auto& base = _morphBase->GetVertices();
	const auto& variant = target->GetVertices();
	_morphVertices = base;
	for (size_t i = 0; i < _morphVertices.size() && i < variant.size(); ++i)
	{
		auto& out = _morphVertices[i];
		out.position.x = base[i].position.x + w * (variant[i].position.x - base[i].position.x);
		out.position.y = base[i].position.y + w * (variant[i].position.y - base[i].position.y);
		out.position.z = base[i].position.z + w * (variant[i].position.z - base[i].position.z);
		out.normal.x = base[i].normal.x + w * (variant[i].normal.x - base[i].normal.x);
		out.normal.y = base[i].normal.y + w * (variant[i].normal.y - base[i].normal.y);
		out.normal.z = base[i].normal.z + w * (variant[i].normal.z - base[i].normal.z);
	}
}

void HandSystem::UploadMorph(bool texture, bool vertices) noexcept
{
	// the one place the morph reaches the GPU (for the draw thread: session Motor). The texture: the base skin of the
	// drawn hand mesh (the material's dirty flag +0x138); the vertices: the hand's sub-mesh built again from the morphed
	// array (with the hd-tweaks smoothing when on)
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(Hand::k_MeshId))
	{
		return;
	}
	auto& mesh = *meshes.Handle(Hand::k_MeshId);
	if (texture && !_morphTexels.empty())
	{
		mesh.UpdateSkin(_morphBase->GetSkins()[0].id, _morphTexels);
	}
	if (vertices && !_morphVertices.empty())
	{
		mesh.RebuildSubMeshes(*_morphBase, _morphVertices);
	}
}
