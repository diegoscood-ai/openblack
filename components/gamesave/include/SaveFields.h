/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

#include <array>
#include <tuple>
#include <type_traits>
#include <vector>

#include "SaveParts.h"

// The records whose values are listed in order: each record is a plain struct, and a static FieldsOf gives its values
// in the order the game writes them, so that one reader and one writer serve every such record. A value that is one
// write of the game is any plain value (a number, a map position, or an array written as one value); the wrappers
// here are the game's other ways of writing: a reference, a list of references, a counted list of values, values
// written one by one, a table.
namespace openblack::gamesave
{

/// A reference to another object of the stream, by its number
struct Ref
{
	ObjectNumber number {0};

	bool operator==(const Ref&) const = default;
};

/// A count, then a reference for each object (the game's lists and arrays of objects)
struct RefList
{
	std::vector<ObjectNumber> numbers;

	bool operator==(const RefList&) const = default;
};

/// A count, then each value
template <typename T>
struct Counted
{
	std::vector<T> values;

	bool operator==(const Counted&) const = default;
};

/// A count, then each element with its own values in order
template <typename T>
struct Listed
{
	std::vector<T> elements;

	bool operator==(const Listed&) const = default;
};

/// Values written one by one, with no count
template <typename T, size_t N>
struct Each
{
	std::array<T, N> values {};

	bool operator==(const Each&) const = default;
};

/// The rows and the columns, then each value, row by row
template <typename T>
struct Table
{
	uint32_t rows {0};
	uint32_t columns {0};
	std::vector<T> values;

	bool operator==(const Table&) const = default;
};

/// One piece of a building's destruction mesh: three words, then its triangles with their count, 88 bytes each
struct DestructionMeshPiece
{
	std::array<uint32_t, 3> words {};
	std::vector<std::array<uint8_t, 88>> triangles;

	bool operator==(const DestructionMeshPiece&) const = default;
};

/// Whether a building keeps the mesh of its destruction, and the mesh when it does: the count of its pieces, three
/// words, then each piece
struct DestructionMeshFlag
{
	uint32_t present {0};
	std::array<uint32_t, 3> words {};
	std::vector<DestructionMeshPiece> pieces;

	bool operator==(const DestructionMeshFlag&) const = default;
};

/// A struct whose values FieldsOf lists in order
template <typename T>
concept HasFields = requires(T& value) { T::FieldsOf(value); };

/// A part with a reader and a writer of its own, for values the game writes only in some cases
template <typename T>
concept HasPartReader = requires(StreamReader& reader, T& part, const ReadNewObject& readNew) {
	{
		ReadPart(reader, part, readNew)
	} -> std::same_as<bool>;
};

/// Reads values in the game's order
class FieldReader
{
public:
	FieldReader(StreamReader& reader, const ThingPart& thing, const ReadNewObject& readNew)
	    : _reader(reader)
	    , _thing(thing)
	    , _readNew(readNew)
	{
	}

	template <typename T>
	    requires(std::is_trivially_copyable_v<T> && !HasFields<T> && !HasPartReader<T>)
	bool operator()(T& value)
	{
		return _reader.Get(value);
	}

	template <HasPartReader T>
	bool operator()(T& part)
	{
		return ReadPart(_reader, part, _readNew);
	}

	bool operator()(Ref& ref) { return ReadReference(_reader, ref.number, _readNew); }

	bool operator()(RefList& list)
	{
		uint32_t count = 0;
		if (!_reader.Get(count) || count > k_MostElements)
		{
			_reader.Fail();
			return false;
		}
		list.numbers.assign(count, 0);
		for (auto& number : list.numbers)
		{
			if (!ReadReference(_reader, number, _readNew))
			{
				return false;
			}
		}
		return true;
	}

	template <typename T>
	bool operator()(Counted<T>& counted)
	{
		uint32_t count = 0;
		if (!_reader.Get(count) || count > k_MostElements)
		{
			_reader.Fail();
			return false;
		}
		counted.values.resize(count);
		for (auto& value : counted.values)
		{
			_reader.Get(value);
		}
		return !_reader.Failed();
	}

	template <typename T>
	bool operator()(Listed<T>& listed)
	{
		uint32_t count = 0;
		if (!_reader.Get(count) || count > k_MostElements)
		{
			_reader.Fail();
			return false;
		}
		listed.elements.resize(count);
		for (auto& element : listed.elements)
		{
			if (!(*this)(element))
			{
				return false;
			}
		}
		return true;
	}

	template <typename T, size_t N>
	bool operator()(Each<T, N>& each)
	{
		for (auto& value : each.values)
		{
			_reader.Get(value);
		}
		return !_reader.Failed();
	}

	template <typename T>
	bool operator()(Table<T>& table)
	{
		_reader.Get(table.rows);
		_reader.Get(table.columns);
		const auto count = static_cast<uint64_t>(table.rows) * table.columns;
		if (_reader.Failed() || count > k_MostElements)
		{
			_reader.Fail();
			return false;
		}
		table.values.resize(static_cast<size_t>(count));
		for (auto& value : table.values)
		{
			_reader.Get(value);
		}
		return !_reader.Failed();
	}

	bool operator()(DestructionMeshFlag& flag)
	{
		if (!_reader.Get(flag.present) || flag.present == 0)
		{
			return !_reader.Failed();
		}
		uint32_t pieces = 0;
		_reader.Get(pieces);
		for (auto& word : flag.words)
		{
			_reader.Get(word);
		}
		if (_reader.Failed() || pieces > k_MostElements)
		{
			_reader.Fail();
			return false;
		}
		flag.pieces.resize(pieces);
		for (auto& piece : flag.pieces)
		{
			for (auto& word : piece.words)
			{
				_reader.Get(word);
			}
			uint32_t triangles = 0;
			if (!_reader.Get(triangles) || triangles > k_MostElements)
			{
				_reader.Fail();
				return false;
			}
			piece.triangles.resize(triangles);
			for (auto& triangle : piece.triangles)
			{
				_reader.Get(triangle);
			}
		}
		return !_reader.Failed();
	}

	bool operator()(PositionPart& position)
	{
		_position = &position;
		return Read(_reader, position);
	}

	bool operator()(ObjectPart& object) { return Read(_reader, _thing, *_position, object, _readNew); }
	bool operator()(FixedPart& part) { return Read(_reader, part, _readNew); }
	bool operator()(MultiMapFixedPart& part) { return Read(_reader, part, _readNew); }
	bool operator()(MobilePart& part) { return Read(_reader, part); }
	bool operator()(MobileObjectPart& part) { return Read(_reader, part, _readNew); }
	bool operator()(MobileStaticPart& part) { return Read(_reader, part, _readNew); }

	template <HasFields T>
	bool operator()(T& part)
	{
		return std::apply([this](auto&... fields) { return ((*this)(fields) && ...); }, T::FieldsOf(part));
	}

private:
	/// The most elements that a read of a counted list accepts, so that bad bytes cannot ask for a huge list
	static constexpr uint32_t k_MostElements = 0x100000;

	StreamReader& _reader;
	const ThingPart& _thing;
	const ReadNewObject& _readNew;
	const PositionPart* _position {nullptr};
};

/// Writes values in the game's order
class FieldWriter
{
public:
	FieldWriter(StreamWriter& writer, const ThingPart& thing, const WriteNewObject& writeNew)
	    : _writer(writer)
	    , _thing(thing)
	    , _writeNew(writeNew)
	{
	}

	template <typename T>
	    requires(std::is_trivially_copyable_v<T> && !HasFields<T> && !HasPartReader<T>)
	void operator()(const T& value)
	{
		_writer.Put(value);
	}

	template <HasPartReader T>
	void operator()(const T& part)
	{
		WritePart(_writer, part, _writeNew);
	}

	void operator()(const Ref& ref) { WriteReference(_writer, ref.number, _writeNew); }

	void operator()(const RefList& list)
	{
		_writer.Put(static_cast<uint32_t>(list.numbers.size()));
		for (const auto number : list.numbers)
		{
			WriteReference(_writer, number, _writeNew);
		}
	}

	template <typename T>
	void operator()(const Counted<T>& counted)
	{
		_writer.Put(static_cast<uint32_t>(counted.values.size()));
		for (const auto& value : counted.values)
		{
			_writer.Put(value);
		}
	}

	template <typename T>
	void operator()(const Listed<T>& listed)
	{
		_writer.Put(static_cast<uint32_t>(listed.elements.size()));
		for (const auto& element : listed.elements)
		{
			(*this)(element);
		}
	}

	template <typename T, size_t N>
	void operator()(const Each<T, N>& each)
	{
		for (const auto& value : each.values)
		{
			_writer.Put(value);
		}
	}

	template <typename T>
	void operator()(const Table<T>& table)
	{
		_writer.Put(table.rows);
		_writer.Put(table.columns);
		for (const auto& value : table.values)
		{
			_writer.Put(value);
		}
	}

	void operator()(const DestructionMeshFlag& flag)
	{
		_writer.Put(flag.present);
		if (flag.present == 0)
		{
			return;
		}
		_writer.Put(static_cast<uint32_t>(flag.pieces.size()));
		for (const auto word : flag.words)
		{
			_writer.Put(word);
		}
		for (const auto& piece : flag.pieces)
		{
			for (const auto word : piece.words)
			{
				_writer.Put(word);
			}
			_writer.Put(static_cast<uint32_t>(piece.triangles.size()));
			for (const auto& triangle : piece.triangles)
			{
				_writer.Put(triangle);
			}
		}
	}

	void operator()(const PositionPart& position)
	{
		_position = &position;
		Write(_writer, _thing, position);
	}

	void operator()(const ObjectPart& object) { WriteObject(_writer, _thing, *_position, object, _writeNew); }

	void operator()(const FixedPart& part) { Write(_writer, part, _writeNew); }
	void operator()(const MultiMapFixedPart& part) { Write(_writer, part, _writeNew); }
	void operator()(const MobilePart& part) { Write(_writer, part); }
	void operator()(const MobileObjectPart& part) { Write(_writer, part, _writeNew); }
	void operator()(const MobileStaticPart& part) { Write(_writer, part, _writeNew); }

	template <HasFields T>
	void operator()(const T& part)
	{
		std::apply([this](const auto&... fields) { ((*this)(fields), ...); }, T::FieldsOf(part));
	}

private:
	StreamWriter& _writer;
	const ThingPart& _thing;
	const WriteNewObject& _writeNew;
	const PositionPart* _position {nullptr};
};

// The parts of the base classes that many records share

/// What a container writes after the position: its kind and its owner
struct ContainerFields
{
	PositionPart position;
	uint32_t info {0};
	Ref owner;

	static auto FieldsOf(auto& part) { return std::tie(part.position, part.info, part.owner); }
	bool operator==(const ContainerFields&) const = default;
};

/// The parts every object of several map cells writes, from the position on
struct MultiMapFixedFields
{
	PositionPart position;
	ObjectPart object;
	FixedPart fixed;
	MultiMapFixedPart multi;

	static auto FieldsOf(auto& part) { return std::tie(part.position, part.object, part.fixed, part.multi); }
	bool operator==(const MultiMapFixedFields&) const = default;
};

/// The parts every moving object writes, from the position on
struct MobileObjectFields
{
	PositionPart position;
	ObjectPart object;
	MobilePart mobile;
	MobileObjectPart mobileObject;

	static auto FieldsOf(auto& part) { return std::tie(part.position, part.object, part.mobile, part.mobileObject); }
	bool operator==(const MobileObjectFields&) const = default;
};

/// The parts every fixed object writes, from the position on
struct FixedFields
{
	PositionPart position;
	ObjectPart object;
	FixedPart fixed;

	static auto FieldsOf(auto& part) { return std::tie(part.position, part.object, part.fixed); }
	bool operator==(const FixedFields&) const = default;
};

/// Reads a record's values after the part every object writes first, which this reads too
template <HasFields Record>
bool ReadFields(StreamReader& reader, Record& record, const ReadNewObject& readNew)
{
	if (!ReadThing(reader, record.thing))
	{
		return false;
	}
	FieldReader fields(reader, record.thing, readNew);
	return fields(record);
}

/// Writes a record from its class's number
template <HasFields Record>
void WriteFields(StreamWriter& writer, const Record& record, const WriteNewObject& writeNew)
{
	WriteThing(writer, Record::k_Type, record.thing);
	FieldWriter fields(writer, record.thing, writeNew);
	fields(record);
}

} // namespace openblack::gamesave
