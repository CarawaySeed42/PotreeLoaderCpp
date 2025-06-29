#pragma once
#ifndef OCTREELOADER_H
#define OCTREELOADER_H
#include "OctreeData.h"
#include "OctreeFileReader.h"

class OctreeLoader
{
private:
	std::shared_ptr<Octree> octree;
	Octree* pOctree;
	OctreeFileReader OctreeReader;
	int64_t max_node_bytes;


public:
	struct PCloudByteOffsets
	{
		int xyz;
		int intensity;
		int rgb;
		int gps_time;
	} pcloud_byte_offsets;

private:
	PCloudByteOffsets SetAttributeByteOffsets()
	{
		PCloudByteOffsets offsets = {-1};

		if (nullptr == pOctree)
		{
			return offsets;
		}

		auto& attributes = pOctree->geometry.pointAttributes;

		int rgbOffset = 0;
		int current_offset = 0;

		for (Attribute attribute : attributes.list) 
		{
			if (attribute.name == "position") {
				offsets.xyz = current_offset;
			}
			else if (attribute.name == "intensity")
			{
				offsets.intensity = current_offset;
			}
			else if (attribute.name == "rgb")
			{
				offsets.rgb = current_offset;
			}
			else if (attribute.name == "gps-time")
			{
				offsets.gps_time = current_offset;
			}

			current_offset += attribute.size;
		}

		return offsets;
	}

	size_t GetMaxNodeSize()
	{
		int64_t buffer_size = 0;
		pOctree->geometry.root->traverse([&buffer_size](OctreeGeometryNode* node, int level) {
			buffer_size = (std::max)(buffer_size, node->byteSize);
			});

		return static_cast<size_t>((std::max)(buffer_size, 0ll));
	}

public:
	OctreeLoader(std::shared_ptr<Octree>& octree) : 
		octree(octree), pOctree(this->octree.get()), pcloud_byte_offsets(SetAttributeByteOffsets()), OctreeReader(pOctree->files.octree), max_node_bytes(GetMaxNodeSize()) {};

	OctreeLoader(Octree* octreePtr) : 
		octree(nullptr), pOctree(octreePtr), pcloud_byte_offsets(SetAttributeByteOffsets()), OctreeReader(pOctree->files.octree), max_node_bytes(GetMaxNodeSize()) {};


	std::vector<uint8_t> CreateMaxNodeBuffer() const
	{
		std::vector<uint8_t> buffer;
		int64_t buffer_size = static_cast<size_t>((std::max)(max_node_bytes, 0ll));
		buffer.resize(buffer_size);
		return buffer;
	}

	OctreeData CreateMaxNodeData() const
	{
		OctreeData RawNodeData((std::max)(max_node_bytes, 0ll));
		return RawNodeData;
	}

	OctreeData& LoadNodeData(OctreeGeometryNode* node, OctreeData& RawNodeData)
	{
		RawNodeData.Extend(node->byteSize);
		OctreeReader.readBinaryData(node->byteOffset, node->byteSize, RawNodeData.data_raw);
		return RawNodeData;
	}

	OctreeData LoadNodeData(OctreeGeometryNode* node)
	{
		OctreeData RawNodeData(node->byteSize);
		LoadNodeData(node, RawNodeData);
		return RawNodeData;
	}

	int64_t LoadNodeData(OctreeGeometryNode* node, std::vector<uint8_t>& buffer)
	{
		buffer.resize(node->byteSize);
		OctreeReader.readBinaryData(node->byteOffset, node->byteSize, buffer);
		return static_cast<int64_t>(buffer.size());
	}
};

#endif