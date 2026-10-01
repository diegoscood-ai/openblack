/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Switches.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace openblack::mods::switches
{
namespace
{
std::vector<Switch>& Table()
{
	static std::vector<Switch> table;
	return table;
}

Switch* FindMutable(std::string_view name)
{
	auto& table = Table();
	const auto it = std::ranges::find(table, name, &Switch::name);
	return it != table.end() ? &*it : nullptr;
}

double Normalise(const Switch& value, double raw)
{
	switch (value.type)
	{
	case Type::Bool:
		return raw != 0.0 ? 1.0 : 0.0;
	case Type::Int:
		return std::clamp(std::round(raw), value.min, value.max);
	case Type::Float:
		return std::clamp(raw, value.min, value.max);
	}
	return raw;
}
} // namespace

void Register(Switch value)
{
	if (auto* existing = FindMutable(value.name); existing != nullptr)
	{
		*existing = std::move(value);
		return;
	}
	Table().push_back(std::move(value));
}

const Switch* Find(std::string_view name)
{
	return FindMutable(name);
}

const std::vector<Switch>& All()
{
	return Table();
}

bool Set(std::string_view name, double value)
{
	auto* target = FindMutable(name);
	if (target == nullptr || !target->set)
	{
		return false;
	}
	const double normalised = Normalise(*target, value);
	const double before = target->get ? target->get() : std::numeric_limits<double>::quiet_NaN();
	target->set(normalised);
	if (target->onChange && before != normalised)
	{
		target->onChange();
	}
	return true;
}

double Get(std::string_view name)
{
	const auto* target = Find(name);
	return target != nullptr && target->get ? target->get() : std::numeric_limits<double>::quiet_NaN();
}

void ResetAll()
{
	for (const auto& value : Table())
	{
		Set(value.name, value.defaultValue);
	}
}

void Clear()
{
	Table().clear();
}

std::string_view TypeName(Type type)
{
	switch (type)
	{
	case Type::Bool:
		return "bool";
	case Type::Int:
		return "int";
	case Type::Float:
		return "float";
	}
	return "?";
}

std::string_view WhenName(When when)
{
	switch (when)
	{
	case When::Live:
		return "live";
	case When::MapLoad:
		return "map";
	case When::Restart:
		return "restart";
	}
	return "?";
}

} // namespace openblack::mods::switches
