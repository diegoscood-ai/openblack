/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mods/BuiltinMods.h"

#include <string>

#include <glm/vec2.hpp>

#include "EngineConfig.h"
#include "Graphics/RendererInterface.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"
#include "Windowing/WindowingInterface.h"

// The original draws with no anti-aliasing (docs/bw1-notes/rendering.md).

namespace openblack::mods
{
namespace
{
class MsaaMod final: public Mod
{
public:
	MsaaMod()
	    : Mod({"graphics.msaa", "Anti-aliasing (MSAA)",
	           "Multisampled backbuffer; leaves and fences get smooth edges with alpha to coverage", "Graphics"})
	{
		AddOption({"samples", "Samples", {"2x", "4x", "8x", "16x"}, 1});
	}

	void Apply() override
	{
		const auto& samples = GetChoice("samples");
		Locator::config::value().msaa = IsEnabled() ? static_cast<uint8_t>(std::stoi(samples)) : 0;
		if (Locator::rendererInterface::has_value() && Locator::windowing::has_value())
		{
			Locator::rendererInterface::value().Reset(glm::u16vec2(Locator::windowing::value().GetSize()));
		}
	}
};
} // namespace

void RegisterMsaaMod(ModRegistry& registry)
{
	registry.Register(std::make_unique<MsaaMod>());
}

} // namespace openblack::mods
