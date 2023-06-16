/**
 * 客户端。运行于本地环境，负责将编辑好的文件发送到服务器。
 */

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <vector>
#include <WinSock2.h>
#include <Windows.h>
#include <cstring>

#include "../common/Utils.h"
#include "../common/DirNode.h"
#include "../common/FileTransportationUtils.h"

using namespace std;
namespace fs = filesystem;

#pragma comment(lib, "ws2_32.lib")

/*------------------ 命令行解析 ------------------*/

struct ClientParams {
	string rootPath;
	string serverIp;
	int serverPort = -1;
};


void printUsage(const char* prompt = nullptr) {
	if (prompt != nullptr) {
		cout << prompt << endl << endl;
	}

	cout << "Usage: client.exe [options]" << endl;
	cout << "Options:" << endl;
	cout << "  -ip <ip address>  The IP Address of the server.           Example: 192.168.80.230" << endl;
	cout << "  -port <port>      The port to be connected on the server. Example: 80" << endl;
	cout << "  -dir <path>       The root of the path to be sent.        Example: ../workspace" << endl;
	cout << endl;
	cout << "all options are required." << endl;
}

// 解析命令行参数。成功时返回0，否则返回非0数。
int analyzeParameters(int argc, const char* argv[], ClientParams& params) {	
	if (argc < 6) {
		printUsage("error: at least 6 parameters required for the client.");
		return -1;
	}

	for (int i = 0; i < argc; i++) {
		string param = argv[i];
		if (param == "-ip") {
			if (params.serverIp != "") {
				cout << "warning: \"-ip\" was set before. This param will be ignored...";
				continue;
			}
			else if (i + 1 >= argc) {
				cout << "error: no argument for \"-ip\"" << endl;
				return -1;
			}
			else {
				int ipSegments[4];
				if (sscanf(argv[i + 1], "%d.%d.%d.%d", ipSegments + 0, ipSegments + 1, ipSegments + 2, ipSegments + 3) != 4)
				{
					cout << "error: ip address analyzation failed." << endl;
					return -1;
				}

				for (int j = 0; j < 4; j++) {
					if (ipSegments[j] < 0 || ipSegments[j] > 255) {
						cout << "error: ip address analyzation failed for invalid range." << endl;
						return -1;
					}
				}

				params.serverIp = to_string(ipSegments[0]);
				for (int j = 1; j < 4; j++) {
					params.serverIp += '.';
					params.serverIp += to_string(ipSegments[j]);
				}

				i += 1; // 这里多读了一个参数，应该往后跳一个。
			}
		} // -ip
		else if (param == "-port") {
			if (params.serverPort != -1) {
				cout << "warning: \"-port\" was set before. This param will be ignored...";
				continue;
			}
			else if (i + 1 >= argc) {
				cout << "error: no argument for \"-ip\"" << endl;
				return -1;
			}
			else {
				int port;
				if (sscanf(argv[i + 1], "%d", &port) != 1) {
					cout << "error: port analyzation failed." << endl;
					return -1;
				}
				else if (port < 0 || port > 65535) {
					cout << "error: port analyzation failed for invalid range." << endl;
					return -1;
				}

				params.serverPort = port;
				i += 1;
			}
		} // -port
		else if (param == "-dir") {
			if (params.rootPath != "") {
				cout << "warning: \"-dir\" was set before. This param will be ignored...";
				continue;
			}
			else if (i + 1 >= argc) {
				cout << "error: no argument for \"-dir\"" << endl;
				return -1;
			}
			else {
				params.rootPath = argv[i + 1];
				i += 1;
			}
		} // -dir
		else {
			cout << "warning: unrecognized argument: " << param << ", ignored." << endl;
		}
	} // for (int i = 0; i < argc; i++)
	
	if (params.rootPath == "") {
		cout << "error: \"-dir\" not set." << endl;
		return -1;
	}
	else if (params.serverIp == "") {
		cout << "error: \"-ip\" not set." << endl;
		return -1;
	}
	else if (params.serverPort == -1) {
		cout << "error: \"-port\" not set." << endl;
		return -1;
	}

	return 0;
}

/*------------------ 目录树构建 ------------------*/

// 创建目录树。成功时返回0，否则返回非0数。
int buildDirTree(string path, DirNode& root, vector<DirNode*>& fileList) {
	// 检查根目录位置。
	fs::path rootPath(path);
	cout << "info: trying to open path: " << rootPath << endl;

	if (!fs::exists(rootPath)) { // 根路径不存在。
		cout << "error: root path doesn't exists!" << endl;
		return -1;
	}

	vector<DirNode*> pending;
	fs::directory_entry rootNode(rootPath);

	if (!rootNode.is_directory() && !rootNode.is_regular_file()) {
		cout << "error: root path is neither file nor directory!" << endl;
		return -1;
	}

	root.filename = Utils::filenameOf(rootNode);
	root.location = Utils::locationOf(rootNode);
	root.isFile = rootNode.is_regular_file();
	pending.push_back(&root);

	cout << "info: discovered: " << root.filename << ", type is " << (root.isFile ? "file" : "directory") << endl;

	while (!pending.empty()) {
		DirNode* curr = pending.back();
		pending.pop_back();
		if (curr->isFile) { // 普通文件。
			fileList.push_back(curr);
			continue;
		}

		for (auto& dirEntry : fs::directory_iterator(fs::path(curr->location / curr->filename)))
		{
			if (!dirEntry.is_directory() && !dirEntry.is_regular_file()) {
				continue;
			}

			DirNode* node = new DirNode; // 注：暂不考虑申请失败的情况。
			node->parent = curr;
			node->isFile = dirEntry.is_regular_file();
			node->filename = Utils::filenameOf(dirEntry);
			node->location = Utils::locationOf(dirEntry);
			curr->children.push_back(node);
			pending.push_back(node);	
			
			cout << "info: discovered: " << node->filename << ", type is " << (node->isFile ? "file" : "directory") << endl;
		}
	}

	return 0;
}

/*------------------ 文件树清理 ------------------*/

void cleanup(DirNode* node, bool shouldFreeEntry = false) {
	for (auto& it : node->children) {
		cleanup(it, true);
	}

	if (shouldFreeEntry) {
		delete node;
	}
}

/*------------------ 上传文件 ------------------*/

static const uint64_t FILE_BLOCK_MAX_SIZE = 8 * 1024 * 1024; // 8 MB

/**
 * @return 0 成功  -1 致命错误
 */
int connectToServerUntilSuccess(const string& ip, int port, SOCKET& client)
{
	while (true) {
		SOCKET sclient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sclient == INVALID_SOCKET) {
			cout << "error: invalid socket!" << endl;
			return -1;
		}

		sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(port);
		serverAddr.sin_addr.S_un.S_addr = inet_addr(ip.c_str());

		cout << "info: connecting to " << ip << " -p " << port << endl;
		Sleep(100);
		int connectResult = connect(sclient, (sockaddr*)&serverAddr, sizeof(serverAddr));
		if (connectResult == SOCKET_ERROR) {
			cout << "warning: connection failed! retrying in 3 seconds..." << endl;
			Sleep(3000);
			continue;
		}
		else {
			client = sclient;
			cout << "info: connection established." << endl;
			return 0;
		}
	}
}

int transferFileHeaderAndFilePath(SOCKET client, const DirNode* dirNode, ifstream& fin) {
	fin.clear();

	streampos oriPos = fin.tellg();

	fin.seekg(0, ios::end);
	FileTransportHeader header;
	header.fileSize = fin.tellg();

	fin.seekg(oriPos);

	string filePath = dirNode->fullPathFromParentRoot().generic_string();
	header.pathSize = strlen(filePath.c_str()) + 1;

	cout << "info: sending data (excluding header):" << endl;
	Utils::viewHex(reinterpret_cast<const char*>(&header), sizeof(header), cout);

	if (send(client, TRANS_DATA_TYPE_FTH, TRANS_DATA_HEADER_SIZE, 0) == SOCKET_ERROR || 
		send(client, reinterpret_cast<const char*>(&header), sizeof(header), 0) == SOCKET_ERROR)
	{
		cout << "error: socket failed while sending file header!" << endl;
		return -1;
	}

	cout << "info: path is: " << filePath.c_str() << endl;
	cout << "info: path size is: " << header.pathSize << endl;

	cout << "info: sending data (excluding header):" << endl;
	Utils::viewHex(filePath.c_str(), header.pathSize, cout);

	if (send(client, TRANS_DATA_TYPE_DAT, TRANS_DATA_HEADER_SIZE, 0) == SOCKET_ERROR ||
		send(client, filePath.c_str(), header.pathSize, 0) == SOCKET_ERROR) 
	{
		cout << "error: socket failed while sending file path!" << endl;
		return -1;
	}

	int8_t response = 0;
	if (recv(client, reinterpret_cast<char*>(&response), sizeof(int8_t), 0) != sizeof(int8_t)) 
	{
		cout << "error: failed while exchanging file header!" << endl;
		return -1;
	}
	else if (response == 0x01) {
		cout << "error: failed while exchanging file header. response is 0x01!" << endl;
		return -1;
	}
	else if (response == 0x00) {
		cout << "info: file header exchanged." << endl;
		return 0;
	}
	else {
		cout << "error: failed while exchanging file header. invalid response!" << endl;
		return -1;
	}
}

/**
 * @return 0 成功  -1 致命错误  -2 请求重新传输文件块  -3 请求重新传输文件（连接中断，断点续传）
 */
int transferFileBlock(SOCKET client, const FileBlockHeader& header, const char* buffer) {
	// 先交接头信息

	cout << "info: sending data (excluding header):" << endl;
	Utils::viewHex(reinterpret_cast<const char*>(&header), sizeof(header), cout);

	if (send(client, TRANS_DATA_TYPE_FBH, TRANS_DATA_HEADER_SIZE, 0) == SOCKET_ERROR ||
		send(client, reinterpret_cast<const char*>(&header), sizeof(header), 0) == SOCKET_ERROR)
	{
		cout << "error: socket failed while sending block header!" << endl;
		return -3;
	}

	// 发送文件块

	cout << "info: sending data (excluding header):" << endl;
	Utils::viewHex(buffer, header.blockSize, cout);

	if (send(client, TRANS_DATA_TYPE_DAT, TRANS_DATA_HEADER_SIZE, 0) == SOCKET_ERROR ||
		send(client, buffer, header.blockSize, 0) == SOCKET_ERROR)
	{
		cout << "error: socket failed while sending block!" << endl;
		return -3;
	}

	// 等待回应
	int8_t response;
	if (recv(client, reinterpret_cast<char*>(&response), sizeof(int8_t), 0) != sizeof(int8_t)) 
	{
		cout << "error: failed while exchanging block!" << endl;
		return -1;
	}
	else if (response == 0x01) {
		cout << "error: failed while exchanging block. response is 0x01!" << endl;
		return -1;
	}
	else if (response == 0x02) {
		cout << "error: block not accepted by server!" << endl;
		return -2;
	}
	else if (response == 0x00) {
		cout << "info: block uploaded." << endl;
		return 0;
	}
	else {
		cout << "error: invalid response while exchanging block!" << endl;
		return -1;
	}
}

/**
 * @return 0 成功  -1 致命错误  -2 源文件打开失败  -3 其他非致命错误
 */
int uploadFile(const string& ip, int port, SOCKET client, const DirNode* dirNode) {
	cout << "info: trying to send file: " << dirNode->fullPathFromParentRoot() << endl;

	ifstream fin(dirNode->absolutePath(), ios::in | ios::binary);
	if (!fin.is_open()) {
		cout << "warning: failed to open this file." << endl;
		return -2;
	}

	char* buffer = new(nothrow) char[FILE_BLOCK_MAX_SIZE];

	if (buffer == nullptr) {
		cout << "error: buffer allocation failure!" << endl;
		return -1;
	}

	FileBlockHeader blockHeader;
	bool isBufferedBlockSent = true;
	uint64_t nextByteOffset = 0;

	fin.clear();
	fin.seekg(0, ios::end);
	int64_t bytesRemaining = fin.tellg();
	fin.seekg(0, ios::beg);

	while (true) {

		cout << "info: bytes remain : " << bytesRemaining << endl;
		cout << "info: file good?   : " << (fin.good() ? "true" : "false") << endl;
		cout << "info: file failed? : " << (fin.fail() ? "true" : "false") << endl;

		// 交接文件信息头和路径信息
		while (transferFileHeaderAndFilePath(client, dirNode, fin) != 0) {
			closesocket(client);
			int connectResult = connectToServerUntilSuccess(ip, port, client);
			if (connectResult == 0) {
				continue;
			}
			else if (connectResult == -1) {
				WSACleanup();
				delete[] buffer;
				return -1;
			}
			else {
				return -1; // 未知错误
			}
		}

		while (bytesRemaining > 0 || !isBufferedBlockSent) {
			if (isBufferedBlockSent) {
				blockHeader.offset = fin.tellg();

				fin.read(buffer, min(FILE_BLOCK_MAX_SIZE, bytesRemaining));
				int64_t bytesRead = fin.gcount();
				cout << "info: bytes read: " << bytesRead << endl;
				
				if (bytesRead <= 0) {
					cout << "warning: error occurred while uploading file. file ignored." << endl;
					return -3;
				}

				blockHeader.blockSize = bytesRead;
				bytesRemaining -= bytesRead;
				nextByteOffset = fin.tellg();

				isBufferedBlockSent = false;
			}
			else {
				cout << "info: resending block..." << endl;
			}

			// 发送文件块
			int res;
			while ((res = transferFileBlock(client, blockHeader, buffer)) || true) {
				// res: 0 成功  -1 致命错误  -2 请求重新传输文件块  -3 请求重新传输文件（连接中断，断点续传）

				if (res == 0) {
					isBufferedBlockSent = true;
					break;
				}
				else if (res == -1) {
					return -1;
				}
				else if (res == -2) {
					continue;
				}
				else if (res == -3) {
					goto endBytesRemainingLoop;
				}
				else { // 无法识别
					return -1;
				}
			} // while ((res = transferFileBlock(client, blockHeader, buffer)) != 0)

		} // while (bytesRemaining > 0)

	endBytesRemainingLoop: // 跳出双重循环。

		if (bytesRemaining == 0 && isBufferedBlockSent) { // 文件传输完毕
			cout << "info: file transferred." << endl;
			FileBlockHeader header;
			header.blockSize = -1;
			header.offset = -1;

			cout << "info: sending data (excluding header):" << endl;
			Utils::viewHex(reinterpret_cast<const char*>(&header), sizeof(header), cout);

			send(client, TRANS_DATA_TYPE_FBH, TRANS_DATA_HEADER_SIZE, 0);
			send(client, reinterpret_cast<const char*>(&header), sizeof(header), 0);
			return 0;
		}
	}
}

void uploadFiles(const string& ip, int port, vector<DirNode*>& fileList) 
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "error: wsa startup failed!" << endl;
		return;
	}

	SOCKET client;
	if (connectToServerUntilSuccess(ip, port, client) != 0) {
		WSACleanup();
		return;
	}

	cout << "info: connection establised." << endl;

	for (const auto& file : fileList) {
		while (true) {
			// res: 0 成功  -1 致命错误  -2 源文件打开失败  -3 其他非致命错误
			int res = uploadFile(ip, port, client, file);
			if (res == 0) {
				break;
			}
			else if (res == -1) {
				if (connectToServerUntilSuccess(ip, port, client) != 0) {
					WSACleanup();
					return;
				}
			}
			else if (res == -2) {
				continue;
			}
			else if (res == -3) {
				continue;
			}
			else {
				return;
			}
		}
	}

	FileTransportHeader header;
	header.fileSize = -1;
	header.pathSize = -1;

	cout << "info: sending data (excluding header):" << endl;
	Utils::viewHex(reinterpret_cast<const char*>(&header), sizeof(header), cout);

	send(client, TRANS_DATA_TYPE_FTH, TRANS_DATA_HEADER_SIZE, 0);
	send(client, reinterpret_cast<const char*>(&header), sizeof(header), 0);

	closesocket(client);
	WSACleanup();
	return;
}

/*------------------ 程序进入点 ------------------*/

int main(int argc, const char* argv[]) 
{
	ClientParams params;

	if (analyzeParameters(argc - 1, argv + 1, params) != 0) {
		return EXIT_FAILURE;
	}

	cout << "info: argument analyzation done." << endl;
	cout << "info: server ip  : " << params.serverIp << endl;
	cout << "info: server port: " << params.serverPort << endl;
	cout << "info: root path  : " << params.rootPath << endl;

	DirNode rootNode;
	rootNode.parent = nullptr;

	vector<DirNode*> fileList;

	if (buildDirTree(params.rootPath, rootNode, fileList) != 0) {
		return EXIT_FAILURE;
	}

	cout << "info: dir tree built. Files to be uploaded:" << endl;

	for (auto& it : fileList) {
		cout << "info: file: " << it->fullPathFromParentRoot() << endl;
	}

	uploadFiles(params.serverIp, params.serverPort, fileList);

	// 清理。
	cout << "info: cleaning up..." << endl;
	cleanup(&rootNode);
	return 0;
}
