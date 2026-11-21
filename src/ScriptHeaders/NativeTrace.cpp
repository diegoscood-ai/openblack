/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NativeTrace.h"

#include <algorithm>

#include <fmt/format.h>

namespace openblack::script
{

std::string FormatNativeValues(std::span<const lhvm::VMValue> values, std::span<const lhvm::DataType> types)
{
	std::string text;
	const auto count = std::min(values.size(), types.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (i > 0)
		{
			text += ", ";
		}
		const auto& value = values[i];
		switch (types[i])
		{
		case lhvm::DataType::Int:
			text += fmt::format("{}", value.intVal);
			break;
		case lhvm::DataType::Float:
		case lhvm::DataType::Vector:
			text += fmt::format("{}", value.floatVal);
			break;
		case lhvm::DataType::Object:
			text += fmt::format("obj:{}", value.uintVal);
			break;
		case lhvm::DataType::Boolean:
			text += value.intVal != 0 ? "true" : "false";
			break;
		default:
		{
			const auto index = static_cast<size_t>(types[i]);
			const auto& name = index < lhvm::k_DataTypeNames.size() ? lhvm::k_DataTypeNames.at(index) : std::string("?");
			text += fmt::format("{}:{:#x}", name, value.uintVal);
			break;
		}
		}
	}
	return text;
}

std::string FormatNativeCall(std::string_view name, std::string_view arguments, std::string_view results, uint32_t turn,
                             std::string_view task)
{
	return fmt::format("native {}({}) -> {}, turn {}, task {}", name, arguments, results, turn, task);
}

} // namespace openblack::script
