/**
 * 服务器端。运行于实验虚拟环境，负责从客户端（物理机）接收文件并存储。
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

struct ServerParams {
	string rootPath;
	int serverPort = -1;
};

void printUsage(const char* prompt = nullptr) {
	if (prompt != nullptr) {
		cout << prompt << endl << endl;
	}

	cout << "Usage: server.exe [options]" << endl;
	cout << "Options:" << endl;
	cout << "  -port <port>  The port for listening.           Example: 80" << endl;
	cout << "  -dir <path>   The root path files to be stored. Example: ../workspace" << endl;
	cout << endl;
	cout << "all options are required." << endl;
}

// 解析命令行参数。成功时返回0，否则返回非0数。
int analyzeParameters(int argc, const char* argv[], ServerParams& params) {
	if (argc < 4) {
		printUsage("error: at least 4 parameters required for the client.");
		return -1;
	}

	for (int i = 0; i < argc; i++) {
		string param = argv[i];
		if (param == "-port") {
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
	else if (params.serverPort == -1) {
		cout << "error: \"-port\" not set." << endl;
		return -1;
	}

	return 0;
}


/*------------------ 连接处理 ------------------*/

void processConnection(SOCKET sclient, const fs::path& rootPath)
{
	cout << "info: processing connection..." << endl;
	char transDataType[TRANS_DATA_HEADER_SIZE];
	char* buffer = new(nothrow) char[TRANS_DATA_HEADER_SIZE];
	if (buffer == nullptr) {
		cout << "error: buffer primary allocation failed!" << endl;
		return;
	}
	uint64_t bufferSize = TRANS_DATA_HEADER_SIZE;
	ofstream fout;

	int8_t msg;

	FileBlockHeader blockHeader;
	FileTransportHeader fileHeader;

	bool isBlockHeaderUsed = true;
	bool isFileHeaderUsed = true;

	fs::path targetFilePath;
	fs::path tmpFilePath;

	while (true) {
		// 接收数据类型信息
		int recSize = recv(sclient, transDataType, TRANS_DATA_HEADER_SIZE, 0);
		if (recSize == SOCKET_ERROR) {
			cout << "error: socket error occurred!" << endl;
			if (buffer != nullptr) {
				delete[] buffer;
				bufferSize = 0;
			}
			if (fout.is_open()) {
				fout.close();
			}
			return;
		}

		cout << "info: header:" << endl;
		Utils::viewHex(transDataType, TRANS_DATA_HEADER_SIZE, cout);

		if (recSize != TRANS_DATA_HEADER_SIZE) {
			cout << "error: trans data type size error!" << endl;
			if (buffer != nullptr) {
				delete[] buffer;
				bufferSize = 0;
			}
			if (fout.is_open()) {
				fout.close();
			}
			return; // 数据长度错误。
		}

		/*---------------- 文件信息头 ----------------*/

		if (strcmp(transDataType, TRANS_DATA_TYPE_FTH) == 0) {
			cout << "info: data type is fth." << endl;
			if (fout.is_open()) {
				cout << "error: file not completed!" << endl;
				fout.close();
			}

			/*-------- 拓展 buffer 尺寸 --------*/

			if (bufferSize < sizeof(FileTransportHeader)) {
				delete[] buffer;
				buffer = new(nothrow) char[sizeof(FileTransportHeader)];
				if (buffer == nullptr) {
					cout << "error: buffer realloc failed!" << endl;
					return;
				}
				bufferSize = sizeof(FileTransportHeader);
			}

			/*-------- 读入头信息 --------*/

			int recSize = recv(
				sclient, reinterpret_cast<char*>(&fileHeader),
				sizeof(FileTransportHeader), 0
			);

			cout << "info: data received:" << endl;
			Utils::viewHex(reinterpret_cast<char*>(&fileHeader), 
				sizeof(FileTransportHeader), cout);

			if (recSize != sizeof(FileTransportHeader))
			{
				cout << "error: header size incorrect." << endl;
				cout << "       required is: " << sizeof(FileTransportHeader)
					<< "        received is: " << recSize << endl;
				continue;
			}

			// 未处理头信息怪异的问题。如，尺寸超大。

			isFileHeaderUsed = false;

			/*-------- 检查是否所有文件传输完毕 --------*/

			if (fileHeader.fileSize == -1) {
				cout << "info: all files received." << endl;
				delete[] buffer;
				return;
			}

		} // strcmp(transDataType, TRANS_DATA_TYPE_FTH) == 0

		/*---------------- 块信息头 ----------------*/

		else if (strcmp(transDataType, TRANS_DATA_TYPE_FBH) == 0) {
			cout << "info: data type is fbh." << endl;
			if (!fout.is_open()) {
				cout << "error: file is not opened!" << endl;
				continue;
			}

			/*-------- 拓展 buffer 尺寸 --------*/

			if (bufferSize < sizeof(FileBlockHeader)) {
				delete[] buffer;
				buffer = new(nothrow) char[sizeof(FileBlockHeader)];
				if (buffer == nullptr) {
					cout << "error: buffer realloc failed!" << endl;
					return;
				}
				bufferSize = sizeof(FileBlockHeader);
			}

			/*-------- 读入头信息 --------*/

			int recSize = recv(
				sclient, reinterpret_cast<char*>(&blockHeader),
				sizeof(FileBlockHeader), 0
			);

			cout << "info: data received:" << endl;
			Utils::viewHex(reinterpret_cast<char*>(&blockHeader),
				sizeof(FileBlockHeader), cout);

			if (recSize != sizeof(FileBlockHeader))
			{
				cout << "error: header size incorrect! ignored." << endl;
				continue;
			}

			// 未处理头信息怪异的问题。如，尺寸超大。

			isBlockHeaderUsed = false;

			/*-------- 检查是否所有文件块传输完毕 --------*/
			
			if (blockHeader.blockSize == -1) {
				cout << "info: all blocks received." << endl;
				fout.close();
				fs::remove(targetFilePath);
				fs::rename(tmpFilePath, targetFilePath);

				cout << "info: file " << Utils::filenameOf(targetFilePath) << " saved." << endl;
			}

		} // strcmp(transDataType, TRANS_DATA_TYPE_FBH) == 0

		/*---------------- 数据 ----------------*/

		else if (strcmp(transDataType, TRANS_DATA_TYPE_DAT) == 0) {
			cout << "info: data type is dat." << endl;
			if (!fout.is_open() && isFileHeaderUsed) {
				cout << "error: can't determine dat usage!" << endl;
				continue;
			} // !fout.is_open() && isFileHeaderUsed

			/*---------------- 文件路径 ----------------*/

			else if (!fout.is_open()) { // 读入文件路径
				cout << "info: treating dat as filepath." << endl;

				/*-------- 拓展 buffer 尺寸 --------*/

				if (bufferSize < fileHeader.pathSize) {
					delete[] buffer;
					buffer = new(nothrow) char[fileHeader.pathSize];
					if (buffer == nullptr) {
						cout << "error: buffer realloc failed!" << endl;
						return;
					}
					bufferSize = fileHeader.pathSize;
				}

				int recSize = 0;
				
				while (true) {
					int rec = recv(sclient, buffer + recSize, fileHeader.pathSize - recSize, 0);

					if (rec == SOCKET_ERROR) {
						cout << "error: socket error while receiving file path!" << endl;
						break;
					}

					cout << "info: data received. accumulated size: " << recSize << endl;
					Utils::viewHex(buffer + recSize, rec, cout);

					recSize += rec;

					if (recSize == fileHeader.pathSize) {
						break;
					}
					else if (recSize > fileHeader.pathSize) {
						cout << "warning: more data than required." << endl;
						cout << "         required: " << fileHeader.pathSize << endl;
						cout << "         actually: " << recSize << endl;
						break;
					}
				}

				cout << "info: data received:" << endl;
				Utils::viewHex(buffer, fileHeader.pathSize, cout);

				if (recSize != fileHeader.pathSize) {
					cout << "error: dat size doesn't pairs path size!" << endl;
					continue;
				}

				targetFilePath = rootPath / buffer;
				tmpFilePath = targetFilePath;
				tmpFilePath += ".tmp";

				fs::create_directories(tmpFilePath.parent_path()); // 创建目录
				// 创建文件
				if (!ofstream(tmpFilePath, ios::out | ios::binary).is_open()) {
					cout << "error: failed to create file!" << endl;
					msg = 0x01;
					send(sclient, reinterpret_cast<char*>(&msg), sizeof(msg), 0);
					continue;
				}
				fs::resize_file(tmpFilePath, fileHeader.fileSize); // 有隐患，未处理
				fout.open(tmpFilePath, ios::out | ios::binary);
				if (!fout.is_open()) {
					cout << "error: failed to open file!" << endl;
					msg = 0x01;
					send(sclient, reinterpret_cast<char*>(&msg), sizeof(msg), 0);
					continue;
				}

				msg = 0x00;
				send(sclient, reinterpret_cast<char*>(&msg), sizeof(msg), 0);
				isFileHeaderUsed = true;
			} // !fout.is_open()

			/*---------------- 文件块 ----------------*/

			else if (isBlockHeaderUsed) {
				cout << "error: can't determine dat usage!" << endl;
				continue;
			}
			else {

				/*-------- 拓展 buffer 尺寸 --------*/

				if (bufferSize < blockHeader.blockSize) {
					delete[] buffer;
					buffer = new(nothrow) char[blockHeader.blockSize];
					if (buffer == nullptr) {
						cout << "error: buffer realloc failed!" << endl;
						return;
					}
					bufferSize = blockHeader.blockSize;
				}

				/*-------- 读入块数据 --------*/

				int recSize = 0;
				
				while (true) {
					int rec = recv(sclient, buffer + recSize, blockHeader.blockSize - recSize, 0);
					if (rec == SOCKET_ERROR) {
						cout << "error: socket error while receiving data block!" << endl;
						break;
					}

					cout << "info: data received. accumulated size: " << recSize << endl;
					Utils::viewHex(buffer + recSize, rec, cout);

					recSize += rec;

					if (recSize >= blockHeader.blockSize) {
						if (recSize > blockHeader.blockSize) {
							cout << "warning: more data than required." << endl
								<< "         required: " << blockHeader.blockSize << endl
								<< "         actually: " << recSize << endl;
						}

						break;
					}
				}

				isBlockHeaderUsed = true;

				if (recSize != blockHeader.blockSize) {
					cout << "warning: block size incorrect. resend required." << endl;
					cout << "         expected: " << blockHeader.blockSize << endl;
					cout << "         actually: " << recSize << endl;
					msg = 0x02;
					send(sclient, reinterpret_cast<char*>(&msg), sizeof(msg), 0);
					continue;
				}

				/*-------- 写文件 --------*/

				else {
					fout.seekp(blockHeader.offset, ios::beg);
					fout.write(buffer, blockHeader.blockSize);
					cout << "info: block wrote. size is: " 
						<< blockHeader.blockSize / 1024 
						<< " KB." << endl;

					msg = 0x00;
					send(sclient, reinterpret_cast<char*>(&msg), sizeof(msg), 0);
				}
			}

		} // strcmp(transDataType, TRANS_DATA_TYPE_DAT) == 0

		/*---------------- 无法识别的数据类型 ----------------*/

		else {
			cout << "error: trans data type unrecognized!" << endl;

			cout << "info: trans data header is:" << endl;
			Utils::viewHex(buffer, recSize, cout);

			if (buffer != nullptr) {
				delete[] buffer;
			}
			if (fout.is_open()) {
				fout.close();
			}
			return; // 类型无法识别。
		}
	}
}

/*------------------ 程序进入点 ------------------*/

int main(int argc, const char* argv[])
{
	ServerParams params;

	if (analyzeParameters(argc - 1, argv + 1, params) != 0) {
		return EXIT_FAILURE;
	}

	cout << "info: argument analyzation done." << endl;
	cout << "info: server port: " << params.serverPort << endl;
	cout << "info: root path  : " << params.rootPath << endl;

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "error: failed to start wsa!" << endl;
		return EXIT_FAILURE;
	}

	SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (slisten == INVALID_SOCKET) {
		cout << "error: socket error!";
		return EXIT_FAILURE;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(params.serverPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(slisten, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR) {
		cout << "error: bind error!" << endl;
		WSACleanup();
		return EXIT_FAILURE;
	}

	if (listen(slisten, 5) == SOCKET_ERROR) {
		cout << "error: listen error!" << endl;
		WSACleanup();
		return EXIT_FAILURE;
	}

	SOCKET sclient;
	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr);

	while (true) {
		cout << "info: waiting for connection..." << endl;
		sclient = accept(slisten, (SOCKADDR*)&remoteAddr, &nAddrLen);
		if (sclient == INVALID_SOCKET) {
			cout << "info: accept error." << endl;
			continue;
		}

		cout << "info: connection establised: " 
			<< inet_ntoa(remoteAddr.sin_addr) << endl;

		processConnection(sclient, fs::path(params.rootPath));

		closesocket(sclient);
	}

	closesocket(slisten);
	WSACleanup();
	return EXIT_SUCCESS;
}
