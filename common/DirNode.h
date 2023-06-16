#pragma once

#include <filesystem>
#include <vector>

/**
 * �ļ��ṹ�ڵ㡣
 */
struct DirNode {
	bool isFile = false; // �Ƿ�Ϊ�ļ�����������ļ�����Ϊ�ļ��С�
	std::filesystem::path filename;
	std::filesystem::path location;
	DirNode* parent = nullptr;
	std::vector<DirNode*> children;

	std::filesystem::path fullPathFromParentRoot() const;
	std::filesystem::path absolutePath() const;
};
