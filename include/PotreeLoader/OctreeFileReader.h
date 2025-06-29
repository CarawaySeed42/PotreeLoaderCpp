#pragma once
#ifndef OCTREEFILEREADER_H
#define OCTREEFILEREADER_H
#include <vector>
#include <string>
#include <filesystem>

struct OctreeFileReader
{
	std::string file_path;
	size_t file_size;

	OctreeFileReader(std::string path) : file_path(path), file_size(std::filesystem::file_size(path)) {};

	inline size_t readBinaryData(FILE* file, uint64_t byte_start, uint64_t byte_size, uint8_t* target) const
	{
		if (byte_start >= file_size) {
			return 0;
		}

		// Determine bytes to read
		auto bytes_to_read = (std::min)(byte_start + byte_size, file_size) - byte_start;

		// Read the data
		_fseeki64(file, byte_start, SEEK_SET);
		const auto bytes_read = fread(target, sizeof(uint8_t), bytes_to_read, file);

		return bytes_read;
	}

	inline size_t readBinaryData(uint64_t byte_start, uint64_t byte_size, uint8_t* target) const
	{
		FILE* file;
		errno_t err = fopen_s(&file, file_path.c_str(), "rb");
		if (err != 0) {
			return 0;
		}

		auto bytes_read = readBinaryData(file, byte_start, byte_size, target);

		// Determine bytes to read
		//auto bytes_to_read = (std::min)(byte_start + byte_size, file_size) - byte_start;

		//// Read the data
		//_fseeki64(file, byte_start, SEEK_SET);
		//const auto bytes_read = fread(target, sizeof(uint8_t), bytes_to_read, file);
		fclose(file);

		return bytes_read;
	}

	inline size_t readBinaryData(uint64_t byte_start, uint64_t byte_size, std::vector<uint8_t>& data)
	{
		// Resize the data vector if necessary
		if (data.size() < byte_size)
		{
			data.resize(byte_size);
		}

		return readBinaryData(byte_start, byte_size, data.data());
	}

	static void readBinaryFile(std::string path, std::vector<uint8_t>& data, uint64_t start, uint64_t size) 
	{
		OctreeFileReader reader(path);
		reader.readBinaryData(start, size, data);
	}
};

#endif