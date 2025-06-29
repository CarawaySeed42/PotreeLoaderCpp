#pragma once
#ifndef OCTREEDATA_H
#define OCTREEDATA_H
#include <vector>

struct OctreeData {

	std::vector<uint8_t> data_raw;
	size_t size_raw = 0;

	OctreeData() {}

	OctreeData(size_t size)
	{
		data_raw.resize(size);
	}

	inline void SetSize(size_t size)
	{
		data_raw.resize(size);
		size_raw = data_raw.size();
	}

	inline void Extend(size_t size)
	{
		if (size > data_raw.size())
		{
			data_raw.resize(size);
		}
	}

	void SetCapacity(size_t capacity)
	{
		data_raw.reserve(capacity);
		size_raw = data_raw.size();
	}

	uint8_t* data()
	{
		return data_raw.data();
	}

	size_t size() const
	{
		return data_raw.size();
	}

	template<class T>
	inline void set(T value, int64_t byte_position)
	{
		memcpy(data.data() + byte_position, &value, sizeof(T));
	}

};

#endif