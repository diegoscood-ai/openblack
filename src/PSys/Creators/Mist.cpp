/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mist.h"

#include <cmath>

#include <algorithm>
#include <memory>

#include "3D/LandLightTable.h"
#include "Graphics/Mists.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The atlas counter of the PSys mists (LH3DMist +0x84; fn_007FA300 adds ftol(g_game_time_inc x 0.255), modulo 900).
/// (aproximado) one counter for every PSys mist: the original keeps one per LH3DMist object (started at
/// Random(0, 16) & 15 by the ctor 0x7F9560), and the port's draw atoms carry no per-atom slot to keep it in
float g_counter = 0.0f;

std::unique_ptr<Creator> MakeMistCreator(const Object& object)
{
	auto creator = std::make_unique<MistCreator>();
	ReadCreatorProperties(object, *creator);
	creator->kind = Creator::Kind::Other; // drawn by mist_atoms::SubmitFrame, not by the sprite or mesh passes
	// ctor 0x6AA380: RandomiseScale 0, IsShadowMap 1, LoadLightMap 1, TakeRatioFromMatrix 0, Pitch 12, 1 frame in the
	// file and in use, InitialScaleMin 1.0, Ratio 0, TextureFileName ""
	creator->takeRatioFromMatrix = object.Bool("TakeRatioFromMatrix", false);
	creator->isShadowMap = object.Bool("IsShadowMap", true);
	creator->loadLightMap = object.Bool("LoadLightMap", true);
	creator->lightMap = object.String("TextureFileName");
	if (creator->lightMap == "NULL_STRING")
	{
		creator->lightMap.clear();
	}
	creator->pitch = std::clamp(object.Int("Pitch", 12), 1, 12);                    // range [1, 12]
	creator->numFramesInFile = std::clamp(object.Int("NumFramesInFile", 1), 1, 32); // range [1, 32]
	creator->numFramesInUse = std::clamp(object.Int("NumFramesInUse", 1), 1, 32);
	creator->initialScaleMin = object.Float("InitialScaleMin", 1.0f);
	creator->ratio = object.Float("Ratio", 0.0f);
	return creator;
}
} // namespace

void MistCreator::InitAtom(Effect& effect, Atom& atom) const
{
	// CreateParticleMist 0x6AA610: RandomiseScale ? PSysFloatRand(InitialScaleMin (+0x70), InitialScale (+0x30)) :
	// InitialScale -> atom +0x74 (it replaces fn_006A85E0's)
	atom.baseScale = randomiseScale ? initialScaleMin + effect.Random(initialScale - initialScaleMin) : initialScale;
	// atom +0x110 = 1.0, +0x114 = 1, +0x118 = LoopAnim (+0x0C): one frame at 1 fps
	atom.frame = 0.0f;
	atom.frameRate = 0.0f;
}

uint32_t mist_atoms::MistColour(uint32_t atomArgb, uint32_t baseArgb)
{
	// 0x67A6C1..0x67A741: the base's alpha byte is set to 0xFF ([esp+0x17] = 0xFF), then each channel is (c x g) >> 8
	const auto channel = [&](int shift, uint32_t g) { return ((((atomArgb >> shift) & 0xFFu) * g) >> 8) << shift; };
	return channel(24, 0xFFu) | channel(16, (baseArgb >> 16) & 0xFFu) | channel(8, (baseArgb >> 8) & 0xFFu) |
	       channel(0, baseArgb & 0xFFu);
}

void mist_atoms::SubmitFrame(float milliseconds)
{
	// fn_007FA300: counter += ftol(g_game_time_inc x 0.255), and modulo 900 once past it (the fraction kept, as the map
	// mists do in RendererMists.cpp, so that high frame rates do not stop the animation)
	g_counter += milliseconds * 0.255f;
	if (g_counter > 900.0f)
	{
		g_counter = std::fmod(g_counter, 900.0f);
	}
	for (const auto& drawable : manager::Collect(Creator::Kind::Other))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const MistCreator*>(atom.creator);
			if (creator == nullptr)
			{
				continue;
			}
			mists::MistDesc mist {};
			mist.position = atom.position; // vt 0x24 SetPos(PSR +0x24)
			mist.size = atom.scale;        // mist +0x88 = PSR +0x30
			// CreateLH3DMist 0x6AA5A0: +0x80 |= 2 (the effect branch) and k = Ratio, or 2.5 + LocalFloatRand(2.5) when 0.
			// (aproximado) with Ratio 0 the per-mist random k has no per-atom slot here: its mean, 3.75
			mist.edgeShrink = true;
			mist.k = creator->ratio != 0.0f ? creator->ratio : 3.75f;
			// DrawAt: TakeRatioFromMatrix -> k = M[1][1] / M[0][0] when |M[0][0]| > 0.0001 (0x8BF518)
			// (the PSR matrix's Y axis carries the stretch, fn_00673DB0: the storm clouds' cloud ratio, UR_CloudGather)
			const glm::mat3 matrix = atom.rotation * glm::mat3(atom.scale, 0.0f, 0.0f, 0.0f, atom.scale * atom.stretch, 0.0f,
			                                                   0.0f, 0.0f, atom.scale);
			if (creator->takeRatioFromMatrix && std::abs(matrix[0][0]) > 0.0001f)
			{
				mist.k = matrix[1][1] / matrix[0][0];
			}
			// DrawData colour (the atom's, its alpha with the collection's) x [0xFA26A4], the land light table's base
			// colour (the renderer's last Build, LandLightTable::Current().GetRawBase(): (aproximado) the previous frame's)
			const auto alpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
			const uint32_t argb = (alpha << 24) | (static_cast<uint32_t>(atom.colour[0]) << 16) |
			                      (static_cast<uint32_t>(atom.colour[1]) << 8) | static_cast<uint32_t>(atom.colour[2]);
			mist.colour = MistColour(argb, LandLightTable::Current().GetRawBase());
			mist.counter = static_cast<int>(g_counter);
			// vt 0x100 (Z-sorted, [0xC0215D] set) / vt 0x104: the sorting is mists::Submit's
			// TODO(storm): the land light / shadow map of a creator with a TextureFileName (+0x40, the storm's
			// S_SMClouds16), a record in list 0xD4EDB8 like ParticleLightMap's: not ported
			mists::Submit(mist);
		}
	}
}

void openblack::psys::RegisterMistCreator()
{
	RegisterCreator("ParticleMistCreator", MakeMistCreator);
}
