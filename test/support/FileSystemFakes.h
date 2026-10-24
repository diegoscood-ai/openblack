/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// File systems in memory for the tests: one that keeps every file in memory, and one that cannot write

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "FileSystem/FileSystemInterface.h"
#include "FileSystem/Stream.h"

namespace openblack::test
{
using Files = std::map<std::filesystem::path, std::vector<uint8_t>>;

/// A stream that writes into its file's bytes in memory, from the start (the file is made or replaced when opened)
class SavingStream final: public filesystem::Stream
{
public:
	explicit SavingStream(std::vector<uint8_t>& bytes)
	    : _bytes(bytes)
	{
		_bytes.clear();
	}

	[[nodiscard]] std::size_t Position() const override { return _position; }
	[[nodiscard]] std::size_t Size() const override { return _bytes.size(); }
	void Seek(std::size_t position, SeekMode seek) override
	{
		_position = seek == SeekMode::Begin ? position : (seek == SeekMode::Current ? _position + position : _bytes.size());
	}
	Stream& Read(uint8_t* /*buffer*/, std::size_t /*length*/) override { throw std::runtime_error("write only"); }
	Stream& Write(const uint8_t* buffer, std::size_t length) override
	{
		_bytes.resize(std::max(_bytes.size(), _position + length));
		std::copy_n(buffer, length, _bytes.begin() + static_cast<std::ptrdiff_t>(_position));
		_position += length;
		return *this;
	}
	std::string GetLine() override { return {}; }
	[[nodiscard]] bool IsEndOfFile() const override { return _position >= _bytes.size(); }

private:
	std::vector<uint8_t>& _bytes;
	std::size_t _position {0};
};

/// The file system with every file in memory: only what the writer and the mind loader use does anything
class MemoryFileSystem final: public filesystem::FileSystemInterface
{
public:
	Files files;
	/// How many files were opened for writing
	int writes {0};
	/// The paths that cannot be opened for writing
	std::set<std::filesystem::path> unwritable;
	/// The folders made, and those that cannot be
	std::set<std::filesystem::path> folders;
	std::set<std::filesystem::path> unmakable;

	[[nodiscard]] std::filesystem::path FindPath(const std::filesystem::path& path) const override
	{
		if (!files.contains(path))
		{
			throw std::runtime_error("not found");
		}
		return path;
	}
	[[nodiscard]] std::unique_ptr<std::istream> GetData(const std::filesystem::path& path) override
	{
		const auto& bytes = files.at(path);
		return std::make_unique<std::istringstream>(std::string(bytes.begin(), bytes.end()));
	}
	[[nodiscard]] bool IsPathValid(const std::filesystem::path& path) override { return files.contains(path); }
	[[nodiscard]] std::unique_ptr<filesystem::Stream> Open(const std::filesystem::path& path,
	                                                       filesystem::Stream::Mode mode) override
	{
		if (mode == filesystem::Stream::Mode::Read)
		{
			throw std::runtime_error("the tests read through ReadAll");
		}
		if (unwritable.contains(path))
		{
			throw std::runtime_error("cannot write");
		}
		++writes;
		return std::make_unique<SavingStream>(files[path]);
	}
	[[nodiscard]] bool Exists(const std::filesystem::path& path) const override { return files.contains(path); }
	void SetGamePath(const std::filesystem::path& path) override { _gamePath = path; }
	[[nodiscard]] const std::filesystem::path& GetGamePath() const override { return _gamePath; }
	void AddAdditionalPath(const std::filesystem::path& /*path*/) override {}
	[[nodiscard]] std::vector<uint8_t> ReadAll(const std::filesystem::path& path) override { return files.at(path); }
	void Iterate(const std::filesystem::path& /*path*/, bool /*recursive*/,
	             const std::function<void(const std::filesystem::path&)>& /*function*/) const override
	{
	}
	bool MakeFolder(const std::filesystem::path& path) override
	{
		if (unmakable.contains(path))
		{
			return false;
		}
		folders.insert(path);
		return true;
	}
	/// The paths whose whole-file writes stop half way
	std::set<std::filesystem::path> incomplete;
	WriteResult WriteFile(const std::filesystem::path& path, std::span<const uint8_t> bytes) override
	{
		if (unwritable.contains(path))
		{
			return WriteResult::NotOpened;
		}
		++writes;
		if (incomplete.contains(path))
		{
			files[path] = std::vector<uint8_t>(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(bytes.size() / 2));
			return WriteResult::Incomplete;
		}
		files[path] = std::vector<uint8_t>(bytes.begin(), bytes.end());
		return WriteResult::Written;
	}
	bool Rename(const std::filesystem::path& from, const std::filesystem::path& to) override
	{
		const auto found = files.find(from);
		if (found == files.end())
		{
			return false;
		}
		auto bytes = std::move(found->second);
		files.erase(found);
		files[to] = std::move(bytes);
		return true;
	}
	bool Remove(const std::filesystem::path& path) override { return files.erase(path) > 0; }

private:
	std::filesystem::path _gamePath {"game"};
};

/// A file system that cannot write
class ReadOnlyFileSystem final: public filesystem::FileSystemInterface
{
public:
	[[nodiscard]] std::filesystem::path FindPath(const std::filesystem::path& path) const override { return path; }
	[[nodiscard]] std::unique_ptr<std::istream> GetData(const std::filesystem::path& /*path*/) override { return {}; }
	[[nodiscard]] bool IsPathValid(const std::filesystem::path& /*path*/) override { return false; }
	[[nodiscard]] std::unique_ptr<filesystem::Stream> Open(const std::filesystem::path& /*path*/,
	                                                       filesystem::Stream::Mode /*mode*/) override
	{
		throw std::runtime_error("read only");
	}
	[[nodiscard]] bool Exists(const std::filesystem::path& /*path*/) const override { return false; }
	void SetGamePath(const std::filesystem::path& /*path*/) override {}
	[[nodiscard]] const std::filesystem::path& GetGamePath() const override { return _gamePath; }
	void AddAdditionalPath(const std::filesystem::path& /*path*/) override {}
	[[nodiscard]] std::vector<uint8_t> ReadAll(const std::filesystem::path& /*path*/) override { return {}; }
	void Iterate(const std::filesystem::path& /*path*/, bool /*recursive*/,
	             const std::function<void(const std::filesystem::path&)>& /*function*/) const override
	{
	}

private:
	std::filesystem::path _gamePath;
};

} // namespace openblack::test
