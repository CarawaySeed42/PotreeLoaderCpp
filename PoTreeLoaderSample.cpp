// PoTreeLoader.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <fstream>
#include <iostream>
#include <execution>
#include <map>
#include <any>
#include <stdexcept>
#include <unordered_map>
#include <regex>
#include <math.h>
#include <filesystem>

#include "include/OctreeCore.h"

using std::vector;
using std::string;
using std::unordered_map;
using std::map;
using json = nlohmann::json;
namespace fs = std::filesystem;

#ifndef FP_FAST_FMA
#define FP_FAST_FMA // FP_FAST_FMA makes multiply add faster than x * y + z, if they do not compile to the same instructions anyway. Used out of curiosity :)
#endif

struct PointCloudItem {
	double x, y, z;
	int16_t r, g, b;
	int16_t intensity;
};

//----------------------------------------------------------------------------------------------
int main(int argc, char** argv) {

	int64_t maxLevel = 100; // The max depth we load data from in this test

	// Directory path to PoTree converted data
	auto current_path = fs::current_path();
	string file_searchpath = (current_path / "sample_data" / "sparse_junction").string();


	/*------------Initialize Octree class with PoTree metadata ------------------------*/
	auto octreeFiles = octree_files::SearchOctreeFiles(file_searchpath);
	string metafile(octreeFiles["metadata"]);
	Octree octree(metafile);

	
	/*----------- Extract data with Octree Loader Class ---------------------------*/
	OctreeLoader loader(&octree);
	auto nodeData = loader.CreateMaxNodeData();

	int bytesPerPoint = octree.geometry.pointAttributes.bytes;
	auto scale = octree.geometry.pointAttributes.posScale;
	const auto offset = octree.geometry.pointAttributes.posOffset;
	int pos_index = loader.pcloud_byte_offsets.xyz;
	int int_index = loader.pcloud_byte_offsets.intensity;
	int rgb_index = loader.pcloud_byte_offsets.rgb;

	bool hasIntensity = true;
	bool hasRGB = true;

	if (pos_index < 0)
	{
		std::cerr << "'position' not found in attribute list" << std::endl;
		return 1;
	}
	if (int_index < 0)
	{
		std::cerr << "'intensity' not found in attribute list" << std::endl;
		hasIntensity = false;
	}
	if (rgb_index < 0)
	{
		std::cerr << "'rgb' not found in attribute list" << std::endl;
		hasRGB = false;
	}

	std::vector<PointCloudItem> cloudpoints;
	cloudpoints.reserve(octree.points);

	// Start reading and extracting until max level
	{
		octree.geometry.nodes[0]->traverse(
			[maxLevel, &loader, &nodeData, bytesPerPoint, scale, offset, pos_index, int_index, rgb_index, &cloudpoints, hasRGB, hasIntensity](OctreeGeometryNode* node, int level) {
			
			if (node->level > maxLevel)
			{
				return;
			}

			//auto data = loader.LoadNodeData(node); // Create new node data object with a new buffer every iteration
			auto& data = loader.LoadNodeData(node, nodeData); // Reuse the node data object's buffer

			size_t pointCountInBuffer = node->byteSize / bytesPerPoint;
			uint8_t* pBuffer = data.data();

			for (int i = 0; i < pointCountInBuffer; ++i)
			{
				const int offsetToPointStart = i * bytesPerPoint;

#ifdef FP_FAST_FMA
				double x = std::fma(static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + pos_index)), scale.x, offset.x);
				double y = std::fma(static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + pos_index + sizeof(int32_t))), scale.y, offset.y);
				double z = std::fma(static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + pos_index + 2 * sizeof(int32_t))), scale.z, offset.z);
#else
				double x = static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + pos_index)) * scale.x + offset.x;
				double y = static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + pos_index + 4)) * scale.y + offset.y;
				double z = static_cast<double>(*reinterpret_cast<int32_t*>(pBuffer + offsetToPointStart + pos_index + 8)) * scale.z + offset.z;
#endif
				uint16_t r = 0, g = 0, b = 0;
				if (hasRGB)
				{
					r = *reinterpret_cast<uint16_t*>(pBuffer + offsetToPointStart + rgb_index);
					g = *reinterpret_cast<uint16_t*>(pBuffer + offsetToPointStart + rgb_index + sizeof(uint16_t));
					b = *reinterpret_cast<uint16_t*>(pBuffer + offsetToPointStart + rgb_index + 2 * sizeof(uint16_t));
				}
				
				uint16_t intensity = 0;
				if (hasIntensity)
				{
					intensity = *reinterpret_cast<uint16_t*>(pBuffer + offsetToPointStart + int_index);
				}
				
				cloudpoints.emplace_back(x, y, z, r, g, b, intensity);
			}

			});
		
	}

#ifdef FP_FAST_FMA
#undef FP_FAST_FMA
#endif

	std::printf("Finished\n");

	return 0;
}
