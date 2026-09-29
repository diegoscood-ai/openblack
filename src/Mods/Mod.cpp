/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mod.h"

#include <utility>

namespace openblack::mods
{

Mod::Mod(Info info)
    : _info(std::move(info))
{
}

Mod::~Mod() = default;

void Mod::AddOption(ModOption option)
{
	_options.push_back(std::move(option));
}

const std::string& Mod::GetChoice(std::string_view optionId) const
{
	static const std::string k_None;
	for (const auto& option : _options)
	{
		if (option.id == optionId && option.value < option.choices.size())
		{
			return option.choices[option.value];
		}
	}
	return k_None;
}

} // namespace openblack::mods
