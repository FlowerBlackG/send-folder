/**
 * 通用工具集。
 */

#include <iostream>
#include <iomanip>

#include "../common/Utils.h"

std::filesystem::path Utils::filenameOf(const std::filesystem::path& path)
{
	return Utils::fullpathOf(path).filename();
}

std::filesystem::path Utils::filenameOf(
	const std::filesystem::directory_entry& dir
) 
{
	return Utils::filenameOf(dir.path());
}

std::filesystem::path Utils::fullpathOf(const std::filesystem::path& path)
{
	return std::filesystem::canonical(path);
}

std::filesystem::path Utils::fullpathOf(const std::filesystem::directory_entry& dir)
{
	return Utils::fullpathOf(dir.path());
}

std::filesystem::path Utils::locationOf(const std::filesystem::path& path)
{
	return Utils::fullpathOf(path).parent_path();
}

std::filesystem::path Utils::locationOf(const std::filesystem::directory_entry& dir)
{
	return Utils::locationOf(dir.path());
}

void Utils::viewHex(
	const char* buffer, const int size, std::ostream& out, const int maxLen
)
{
	if (size <= 0 || maxLen <= 0) {
		return;
	}

	int tarSize = size < maxLen ? size : maxLen;
	
	out << std::hex
		<< std::setiosflags(std::ios::right)
		<< std::setfill('0');

	for (int i = 0; i < tarSize; i++) {
		if (i % 16 == 0) {
			out << std::setw(8) << i << "   ";
		}
		else if (i % 8 == 0) {
			out << "- ";
		}

		out << std::setw(2) << (int(buffer[i]) & 0xff) << " ";

		if ((i + 1) % 16 == 0) {
			out << "  ";
		}
		else if (i + 1 == tarSize) {
			out << std::setfill(' ')
				<< std::setw(3 * (16 - (i + 1) % 16) + (i % 16 < 8 ? 2 : 0) + 2)
				<< "";
		}
		

		if (i + 1 == tarSize || (i + 1) % 16 == 0) {
			for (int j = i / 16 * 16; j <= i; j++) {
				if (buffer[j] <= 32 || buffer[j] > 126) {
					out << '.';
				}
				else {
					out << buffer[j];
				}
			}

			out << std::endl;
		}
	}

	if (tarSize < size) {
		out << "..." << std::endl;
		for (int i = (size / 16 - 2) * 16; i < size; i++) {
			if (i % 16 == 0) {
				out << std::setw(8) << i << "   ";
			}
			else if (i % 8 == 0) {
				out << "- ";
			}

			out << std::setw(2) << (int(buffer[i]) & 0xff) << " ";

			if ((i + 1) % 16 == 0) {
				out << "  ";
			}
			else if (i + 1 == size) {
				out << std::setfill(' ')
					<< std::setw(3 * (16 - (i + 1) % 16) + (i % 16 < 8 ? 2 : 0) + 2)
					<< "";
			}


			if (i + 1 == size || (i + 1) % 16 == 0) {
				for (int j = i / 16 * 16; j <= i; j++) {
					if (buffer[j] <= 32 || buffer[j] > 126) {
						out << '.';
					}
					else {
						out << buffer[j];
					}
				}

				out << std::endl;
			}
		}
	}

	out << std::setfill(' ')
		<< std::resetiosflags(std::ios::right)
		<< std::dec;
}
