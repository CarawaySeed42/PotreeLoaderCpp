#pragma once
#ifndef OCTREE_H
#define OCTREE_H

#include "..\OctreeCore.h"
#include "..\ThirdParty/PotreeConverter/Attributes.h"
#include "..\ThirdParty/PotreeConverter/Geometry.h"
#include "..\ThirdParty/PotreeConverter/Buffer.h"
#include "..\ThirdParty/json/json.hpp"

#include <map>
#include <any>
#include <stdexcept>
#include <unordered_map>
#include <regex>

using std::vector;
using std::string;
using std::unordered_map;
using std::map;
using std::shared_ptr;
using json = nlohmann::json;
using BoundingBox = geometry::BoundingBox;
using Vector3 = geometry::Vector3;

enum NODETYPE {
	NORMAL = 0,
	LEAF = 1,
	PROXY = 2,
};

struct OctreeGeometryNode;

struct Hierarchy
{
	int64_t firstChunkSize;
	int64_t stepSize;
	int64_t depth;
};

struct OctreeFiles
{
	string hierarchy;
	string octree;
	string metadata;
};

class OctreeGeometry
{
public:
	string url;
	string projection;
	double spacing;
	BoundingBox boundingBox;
	BoundingBox tightBoundingBox;
	vector<double> offset;
	vector<double> scale;
	Attributes pointAttributes;
	std::shared_ptr<OctreeGeometryNode> root;
	std::vector<shared_ptr<OctreeGeometryNode>> nodes;
	int64_t traversableNodes;
};

struct OctreeGeometryNode
{
	string name;
	int64_t index;
	BoundingBox boundingBox;
	int64_t numPoints;
	int64_t level;
	
	OctreeGeometry* octreeGeometry;
	vector<OctreeGeometryNode*> children;

	OctreeGeometryNode* parent;
	int64_t byteOffset;
	int64_t byteSize;
	double spacing;
	NODETYPE nodeType;

	OctreeGeometryNode() : OctreeGeometryNode("", nullptr, BoundingBox()) {};

	OctreeGeometryNode(const string& name, OctreeGeometry* octreeGeometry, const BoundingBox& boundingBox)
		: name(name), octreeGeometry(octreeGeometry), boundingBox(boundingBox)
	{
		index = name.empty() ? -1 : name.at(name.size() - 1) - '0';
		numPoints = 0;
		level = 0;
		parent = nullptr;
		byteOffset = 0;
		byteSize = 0;
		nodeType = NODETYPE::NORMAL;
		spacing = 0;
	}

	bool operator==(const OctreeGeometryNode& rhs) const
	{
		return this->name == rhs.name;
	}

	bool hasChildren() const
	{
		return !children.empty();
	}

	void traverse(std::function<void(OctreeGeometryNode*, int)> callback, int level = 0) {

		callback(this, level);

		for (auto& child : children) {
			if (child != nullptr) {
				child->traverse(callback, level + 1);
			}
		}
	}

	void traverse(std::function<void(OctreeGeometryNode*)> callback) {

		callback(this);

		for (auto& child : children) {
			if (child != nullptr) {
				child->traverse(callback);
			}
		}
	}

	void traverse_conditional(std::function<void(OctreeGeometryNode*, int)> callback, std::function<bool(OctreeGeometryNode*, int level)> condition, int level = 0) {

		if (!condition(this, level)){
			return;
		}

		callback(this, level);

		for (auto& child : children) {
			if (child != nullptr) {
				child->traverse(callback, level + 1);
			}
		}
	}

	void traverse_conditional(std::function<void(OctreeGeometryNode*)> callback, std::function<bool(OctreeGeometryNode*)> condition) {

		if (!condition(this)){
			return;
		}

		callback(this);

		for (auto& child : children) {
			if (child != nullptr) {
				child->traverse(callback);
			}
		}
	}
};

BoundingBox createChildAABB(BoundingBox aabb, size_t index) {
	auto min = aabb.min;
	auto max = aabb.max;
	auto size = Vector3(max.x - min.x, max.y - min.y, max.z - min.z);

	if ((index & 0b0001) > 0) {
		min.z += size.z / 2;
	}
	else {
		max.z -= size.z / 2;
	}

	if ((index & 0b0010) > 0) {
		min.y += size.y / 2;
	}
	else {
		max.y -= size.y / 2;
	}

	if ((index & 0b0100) > 0) {
		min.x += size.x / 2;
	}
	else {
		max.x -= size.x / 2;
	}

	return BoundingBox(min, max);
}

vector<shared_ptr<OctreeGeometryNode>> CreateHierarchyNodes(shared_ptr<OctreeGeometryNode>& node, Buffer* buffer)
{
	auto octree = node->octreeGeometry;
	auto bytesPerNode = 22;
	int64_t nodeCount = node->byteSize / bytesPerNode;
	int64_t startByte = node->byteOffset;
	vector<shared_ptr<OctreeGeometryNode>> nodes(nodeCount);
	nodes[0] = node;
	auto nodePos = 1;
	auto pBuffer = buffer->data_u8 + startByte;

	if ((startByte + node->byteSize) > buffer->size)
	{
		throw std::out_of_range("Hierarchy Node count overruns the buffer size!");
	}

	for (auto i = 0; i < nodeCount; ++i) {
		auto& current = nodes[i];

		uint8_t type = *reinterpret_cast<uint8_t*> (pBuffer + (i * bytesPerNode + 0));
		uint8_t childMask = *reinterpret_cast<uint8_t*> (pBuffer + (i * bytesPerNode + 1));
		uint32_t numPoints = *reinterpret_cast<uint32_t*>(pBuffer + (i * bytesPerNode + 2));
		uint64_t byteOffset = *reinterpret_cast<uint64_t*>(pBuffer + (i * bytesPerNode + 6));
		uint64_t byteSize = *reinterpret_cast<uint64_t*>(pBuffer + (i * bytesPerNode + 14));

		current->byteOffset = byteOffset;
		current->byteSize = byteSize;
		current->numPoints = numPoints;

		//if (current->nodeType == NODETYPE::PROXY) {
		//	// replace proxy with real node
		//	current->byteOffset = byteOffset;
		//	current->byteSize = byteSize;
		//	current->numPoints = numPoints;
		//}
		//else if (type == NODETYPE::PROXY) {
		//	// load proxy
		//	current->byteOffset = byteOffset; //hierarchyByteOffset
		//	current->byteSize = byteSize;    //hierarchyByteSize
		//	current->numPoints = numPoints;
		//}
		//else {
		//	// load real node 
		//	current->byteOffset = byteOffset;
		//	current->byteSize = byteSize;
		//	current->numPoints = numPoints;
		//}

		if (current->byteSize == 0) { // Byte Size is only 0 now, if proxy -> delete hierarchyByteOffset and hierarchyByteSize member in the future
			current->numPoints = 0;
		}

		current->nodeType = static_cast<NODETYPE>(type);

		if (current->nodeType == NODETYPE::PROXY) {
			continue;
		}

		for (auto childIndex = 0; childIndex < 8; childIndex++) {
			auto childExists = ((1 << childIndex) & childMask) != 0;

			if (!childExists) {
				continue;
			}

			auto childName = current->name + to_string(childIndex);

			auto childAABB = createChildAABB(current->boundingBox, childIndex);
			auto child = make_shared< OctreeGeometryNode>(childName, octree, childAABB);
			child->name = childName;
			child->spacing = current->spacing / 2;
			child->level = current->level + 1;
			child->parent = current.get();

			current->children.resize( 8 );
			current->children[childIndex] = child.get();

			// Push child to nodes
			nodes[nodePos] = child;
			nodePos++;
		}
	}

	return nodes;
}


json loadMetadataJson(const string& filepath)
{
	string metadataText = readTextFile(filepath);
	return json::parse(metadataText);
}

Attributes parseMetadataAtrributes(const json& js)
{
	vector<Attribute> attributeList;
	auto& jsAttributes = js["attributes"];
	for (auto& jsAttribute : jsAttributes) {

		string name = jsAttribute["name"];
		string description = jsAttribute["description"];
		int size = jsAttribute["size"];
		int numElements = jsAttribute["numElements"];
		int elementSize = jsAttribute["elementSize"];
		AttributeType type = typenameToType(jsAttribute["type"]);

		auto& jsMin = jsAttribute["min"];
		auto& jsMax = jsAttribute["max"];

		Attribute attribute(name, size, numElements, elementSize, type);

		if (numElements >= 1) {
			attribute.min.x = jsMin[0] == nullptr ? Infinity : double(jsMin[0]);
			attribute.max.x = jsMax[0] == nullptr ? Infinity : double(jsMax[0]);
		}
		if (numElements >= 2) {
			attribute.min.y = jsMin[1] == nullptr ? Infinity : double(jsMin[1]);
			attribute.max.y = jsMax[1] == nullptr ? Infinity : double(jsMax[1]);
		}
		if (numElements >= 3) {
			attribute.min.z = jsMin[2] == nullptr ? Infinity : double(jsMin[2]);
			attribute.max.z = jsMax[2] == nullptr ? Infinity : double(jsMax[2]);
		}

		attributeList.push_back(attribute);
	}

	double scaleX = js["scale"][0];
	double scaleY = js["scale"][1];
	double scaleZ = js["scale"][2];

	double offsetX = js["offset"][0];
	double offsetY = js["offset"][1];
	double offsetZ = js["offset"][2];

	Attributes attributes(attributeList);
	attributes.posScale = { scaleX, scaleY, scaleZ };
	attributes.posOffset = { offsetX, offsetY, offsetZ };

	return attributes;
}

Hierarchy parseMetadataHierarchy(json js)
{
	auto& hierarchyjs = js["hierarchy"];
	Hierarchy hierarchy({ hierarchyjs["firstChunkSize"] , hierarchyjs["stepSize"], hierarchyjs["depth"] });
	return hierarchy;
}

namespace octree_files
{
	map<string, string> SearchOctreeFiles(const string& octreeFilepath)
	{
		fs::path searchFolder(octreeFilepath);

		if (fs::is_regular_file(searchFolder))
		{
			searchFolder = searchFolder.parent_path();
		}

		if (!fs::is_directory(searchFolder))
		{
			throw std::invalid_argument("Argument search path can not be resolved to a directory");
		}

		// regex pattern for search
		const std::regex hierarchy_pattern("hierarchy.*.bin");
		const std::regex octree_pattern("octree.*.bin");
		const std::regex metadata_pattern("metadata.*.json");

		std::map<string, string> octreeFiles =
		{
			{"hierarchy", ""},
			{"octree",    ""},
			{"metadata",  ""}
		};

		// Look for files with pattern
		for (auto& entry : std::filesystem::directory_iterator(searchFolder))
		{
			if (entry.is_regular_file())
			{
				auto& filepath = entry.path();
				string filename = filepath.filename().string();

				if (std::regex_match(filename, hierarchy_pattern))
				{
					octreeFiles["hierarchy"] = filepath.string();
				}
				else if (std::regex_match(filename, octree_pattern))
				{
					octreeFiles["octree"] = filepath.string();
				}
				else if (std::regex_match(filename, metadata_pattern))
				{
					octreeFiles["metadata"] = filepath.string();
				}
			}
		}

		return octreeFiles;
	}
}

class Octree
{
public:
	string version;
	string name;
	string description;
	int64_t points;
	OctreeFiles files;

	OctreeGeometry geometry;
	Hierarchy hierarchy;

private:
	void SearchOctreeFiles(const string& searchFolderPath)
	{
		auto files_map  = octree_files::SearchOctreeFiles(searchFolderPath);
		files.hierarchy = files_map["hierarchy"];
		files.octree    = files_map["octree"];
		files.metadata  = files_map["metadata"];
		return;
	}

public:
	Octree() = default;

	Octree(const string& metadataFilePath)
	{
		LoadFromMetadataFile(metadataFilePath);
	};

	const OctreeGeometry& Geometry() const
	{
		return this->geometry;
	}

	void LoadFromMetadataFile(const string& metadataFilePath)
	{
		// Load and parse metadata json
		this->geometry.url = metadataFilePath;
		SearchOctreeFiles(metadataFilePath);
		json metadata = loadMetadataJson(metadataFilePath);

		this->geometry.pointAttributes = parseMetadataAtrributes(metadata);
		{
			auto& scale = this->geometry.pointAttributes.posScale;
			this->geometry.scale = { scale.x, scale.y, scale.z };
			auto& offset = this->geometry.pointAttributes.posOffset;
			this->geometry.offset = { offset.x, offset.y, offset.z };
		}

		this->hierarchy = parseMetadataHierarchy(metadata);

		// ToDo: Maybe make all this meta data parsing a little more modular...
		version = metadata["version"];
		name = metadata["name"];
		description = metadata["description"];
		points = metadata["points"];

		this->geometry.projection = metadata["projection"];
		this->geometry.spacing = metadata["spacing"];
		this->geometry.boundingBox = {
			{metadata["boundingBox"]["min"][0], metadata["boundingBox"]["min"][1], metadata["boundingBox"]["min"][2]},
			{metadata["boundingBox"]["max"][0], metadata["boundingBox"]["max"][1], metadata["boundingBox"]["max"][2]}
		};

		geometry.tightBoundingBox = geometry.boundingBox;

		// Load all chunks of the hierarchy using the hierarchy file, starting from root
		LoadHierarchyNodes(this->files.hierarchy);

		// ToDo: Test filesize of octree.bin against last byte according to hierarchy
	}

	std::vector<shared_ptr<OctreeGeometryNode>>& LoadHierarchyNodes(std::string hierarchyFile)
	{
		return LoadHierarchyNodes(readBinaryFile(hierarchyFile));
	}

	std::vector<shared_ptr<OctreeGeometryNode>>& LoadHierarchyNodes(shared_ptr<Buffer> buffer)
	{
		const auto bytesPerNode = 22;

		// Create octree geometry root
		this->geometry.root = make_shared< OctreeGeometryNode>("r", &geometry, geometry.boundingBox);

		this->geometry.root->level = 0;
		this->geometry.root->nodeType = NODETYPE::PROXY;
		this->geometry.root->byteOffset = 0; //hierarchyByteOffset
		this->geometry.root->byteSize = hierarchy.firstChunkSize; //hierarchyByteSize
		this->geometry.root->spacing = geometry.spacing;
		this->geometry.root->byteOffset = 0;

		auto* root = &this->geometry.root;

		// Load Hierarchy
		this->geometry.nodes.resize(0);
		this->geometry.nodes.reserve(buffer->size / bytesPerNode);
		vector<int64_t> chunkStarts;
		chunkStarts.reserve(this->geometry.nodes.capacity());

		auto offset = 0;
		auto& geometryNodes = this->geometry.nodes;
		int64_t chunk_start = 0; // Can be used to save all chunk starts

		while (geometryNodes.size() < geometryNodes.capacity())
		{
			auto chunkNodes = CreateHierarchyNodes(*root, buffer.get());
			chunk_start = geometryNodes.size();
			geometryNodes.insert(geometryNodes.end(), chunkNodes.begin(), chunkNodes.end());

			for (; offset < geometryNodes.size(); ++offset)
			{
				if (geometryNodes[offset]->nodeType == NODETYPE::PROXY)
				{
					root = &geometryNodes[offset];
					break;
				}
			}

			++offset;
		}

		
		int64_t traversableNodes = 0;
		geometry.root->traverse([&traversableNodes](OctreeGeometryNode* node, int level) {
			++traversableNodes;
			});

		geometry.traversableNodes = traversableNodes;
		return this->geometry.nodes;
	}
	
	std::vector<OctreeGeometryNode*> TraversableNodeReferences()
	{
		std::vector<OctreeGeometryNode*> traversableNodeRefs;
		traversableNodeRefs.reserve(geometry.traversableNodes);
		int64_t traversableNodes = 0;
		
		geometry.root->traverse([&traversableNodeRefs](OctreeGeometryNode* node, int level) {
			traversableNodeRefs.push_back(node);
		});

		return traversableNodeRefs;
	}
	

};

#include "OctreeLoader.h"

#endif