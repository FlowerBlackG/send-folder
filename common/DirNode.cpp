
#include <filesystem>
#include "../common/DirNode.h"

std::filesystem::path DirNode::fullPathFromParentRoot() const
{
	std::filesystem::path res;
	const DirNode* curr = this;
	while (curr != nullptr) {
		if (res.empty()) {
			res = curr->filename;
		}
		else {
			res = curr->filename / res;
		}
		curr = curr->parent;
	}

	return res;
}

std::filesystem::path DirNode::absolutePath() const {
	return this->location / this->filename;
}
