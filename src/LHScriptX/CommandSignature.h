/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

namespace openblack::lhscriptx
{
enum class ParameterType
{
	None = 0,
	String, // L
	Float,  // F
	Number, // N
	Vector  // A
};

class ScriptCommandParameter
{
public:
	explicit ScriptCommandParameter(ParameterType type = ParameterType::None)
	    : _type(type)
	{
	}
	explicit ScriptCommandParameter(const std::string& value)
	    : _type(ParameterType::String)
	{
		_string = value;
	}
	explicit ScriptCommandParameter(float value)
	    : _type(ParameterType::Float)
	{
		SetFloat(value);
	}
	explicit ScriptCommandParameter(int32_t value)
	    : _type(ParameterType::Number)
	{
		SetNumber(value);
	}
	ScriptCommandParameter(float x, float y, float z)
	    : _type(ParameterType::Vector)
	{
		SetVector(x, y, z);
	}

	[[nodiscard]] ParameterType GetType() const { return _type; };

	[[nodiscard]] const std::string& GetString() const { return _string; }
	[[nodiscard]] float GetFloat() const { return _value._float; }
	[[nodiscard]] int32_t GetNumber() const { return _value._number; }
	void GetVector(float& x, float& y, float& z) const
	{
		x = _value._vector[0];
		y = _value._vector[1];
		z = _value._vector[2];
	}

	void SetString(const std::string& value) { _string = value; }
	void SetFloat(float value) { _value._float = value; };
	void SetNumber(int32_t value) { _value._number = value; }
	void SetVector(float x, float y, float z)
	{
		_value._vector[0] = x;
		_value._vector[1] = y;
		_value._vector[2] = z;
	}
	/// A position's map units as the script gave them, its height above the land included; the vector holds the place
	/// in metres on the land
	[[nodiscard]] const map_coords::MapCoords& GetUnits() const { return _units; }
	void SetUnits(const map_coords::MapCoords& units) { _units = units; }

private:
	ParameterType _type;

	union
	{
		// NOLINTNEXTLINE(readability-identifier-naming): float is a reserved name
		float _float;
		// NOLINTNEXTLINE(readability-identifier-naming)
		int32_t _number;
		// NOLINTNEXTLINE(readability-identifier-naming): std::array is trivial, so the union keeps it
		std::array<float, 3> _vector;
	} _value;
	std::string _string;
	map_coords::MapCoords _units {};
};

/// A position argument as a command can take it: its place on the land in metres, and the map units the script gave
struct ScriptPosition
{
	glm::vec3 metres {0.0f};
	map_coords::MapCoords units {};
};

using ScriptCommandParameters = std::vector<ScriptCommandParameter>;

using ScriptCommand = std::function<void(const ScriptCommandParameters&)>;

struct ScriptCommandSignature
{
	const std::array<char, 0x80> name;
	const ScriptCommand command;
	const std::array<ParameterType, 9> parameters;
};
} // namespace openblack::lhscriptx
