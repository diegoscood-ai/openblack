/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectStream.h"

using namespace openblack::gamesave;

void StreamWriter::PutUncounted(uint32_t value)
{
	const auto at = _bytes.size();
	_bytes.resize(at + sizeof(value));
	std::memcpy(_bytes.data() + at, &value, sizeof(value));
}

bool StreamWriter::PutReference(ObjectKey key)
{
	if (key == k_NoObject)
	{
		Put(uint32_t {0});
		return false;
	}
	if (const auto known = NumberOfKey(key); known.has_value())
	{
		Put(*known);
		return false;
	}
	++_objects;
	_numbers.emplace(key, _objects);
	Put(_objects);
	return true;
}

std::optional<uint32_t> StreamWriter::NumberOfKey(ObjectKey key) const
{
	if (const auto found = _numbers.find(key); found != _numbers.end())
	{
		return found->second;
	}
	// The objects before the start of the stream keep the numbers that the file gave them
	if (const auto number = NumberOf(key); number.has_value() && *number <= _start.objects)
	{
		return number;
	}
	return std::nullopt;
}

bool StreamReader::GetUncounted(uint32_t& value)
{
	if (_failed || _bytes.size() - _at < sizeof(value))
	{
		_failed = true;
		return false;
	}
	std::memcpy(&value, _bytes.data() + _at, sizeof(value));
	_at += sizeof(value);
	return true;
}

bool StreamReader::GetChecksum(uint32_t& stored)
{
	const auto running = _checksum;
	const auto at = _at;
	if (!Get(stored))
	{
		return false;
	}
	++_sumsRead;
	if (stored != running && !_firstWrongSum.has_value())
	{
		_firstWrongSum = at;
	}
	return true;
}

ReferenceKind StreamReader::GetReference(ObjectKey& key)
{
	uint32_t number = 0;
	key = k_NoObject;
	if (!Get(number))
	{
		return ReferenceKind::Bad;
	}
	if (number == 0)
	{
		return ReferenceKind::None;
	}
	if (number <= _objects)
	{
		key = NumberKey(number);
		return ReferenceKind::Earlier;
	}
	if (number != _objects + 1)
	{
		_failed = true;
		return ReferenceKind::Bad;
	}
	++_objects;
	key = NumberKey(number);
	return ReferenceKind::New;
}
