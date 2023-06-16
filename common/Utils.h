/**
 * 通用工具集。
 */

#pragma once

#include <filesystem>
#include <iostream>

#include "./DirNode.h"

class Utils {
public:
	static std::filesystem::path filenameOf(const std::filesystem::path& path);
	static std::filesystem::path filenameOf(const std::filesystem::directory_entry& dir);
	static std::filesystem::path fullpathOf(const std::filesystem::path& path);
	static std::filesystem::path fullpathOf(const std::filesystem::directory_entry& dir);
	static std::filesystem::path locationOf(const std::filesystem::path& path);
	static std::filesystem::path locationOf(const std::filesystem::directory_entry& dir);

	static void viewHex(
		const char* buffer, const int size, std::ostream& out, const int maxLen = 128
	);
};
