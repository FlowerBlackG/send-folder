#pragma once

#include <filesystem>
#include <vector>

/**
 * 文件结构节点。
 */
struct DirNode {
	bool isFile = false; // 是否为文件。如果不是文件，则为文件夹。
	std::filesystem::path filename;
	std::filesystem::path location;
	DirNode* parent = nullptr;
	std::vector<DirNode*> children;

	std::filesystem::path fullPathFromParentRoot() const;
	std::filesystem::path absolutePath() const;
};
