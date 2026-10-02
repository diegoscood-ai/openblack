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

Mod::Mod(Info info, std::filesystem::path root)
    : _info(std::move(info))
    , _root(std::move(root))
    , _enabled(_info.enabledByDefault)
{
}

Mod::~Mod() = default;

void Mod::CollectSwitches(std::map<std::string, double, std::less<>>& switches) const
{
	for (const auto& [name, value] : _runtimeSwitches)
	{
		switches[name] = value;
	}
}

void Mod::Apply() {}

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
