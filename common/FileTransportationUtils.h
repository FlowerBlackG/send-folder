/**
 * 文件传输工具集。
 * 
 * 规定每个文件的传输方式：
 *   client: 发送 FileTransportHeader 
 *   server: 获取 FileTransportHeader
 *   client: 发送文件路径字符串，然后等待回应
 *           ! 连接中断，则重新发送 FileTransportHeader
 *   server: 获取文件路径字符串，并创建缓存文件
 *   server: 用 8 bit 返回缓存文件创建结果
 *           0x00: 成功
 *           0x01: 致命错误。如文件创建失败
 *   client: 获取返回状态。失败则退出，成功则循环发送文件块
 *   
 * 规定每个文件块的传输方式：
 *   client: 发送 FileBlockHeader
 *   server: 获取 FileBlockHeader, 并创建缓冲
 *   client: 发送 1 个文件块，然后等待回应
 *   server: 接收文件块，写文件，并用 8 bit 返回结果
 *           0x00: 成功
 *           0x01: 致命错误。如缓冲区创建失败、写文件失败
 *           0x02: 非致命错误。需要重新传输块
 *   client: 获取返回状态。致命则退出，需要重新传输则重发文件块，成功则继续
 *           ! 连接中断，则重新交接文件信息，然后重新发送块信息（含头）
 * 
 * 若 FileTransportHeader 中的 fileSize 为 -1, 表示文件传输完毕，可关闭连接
 * 若 FileBlockHeader 中的 blockSize 为 -1, 表示文件传输完毕，可保存文件
 */

#pragma once

#include <cstdint>

const char* TRANS_DATA_TYPE_FTH = "dtfth";
const char* TRANS_DATA_TYPE_FBH = "dtfbh";
const char* TRANS_DATA_TYPE_DAT = "dtdat";
const int TRANS_DATA_HEADER_SIZE = 6;

struct FileTransportHeader {
	uint64_t fileSize; // 文件大小。
	uint64_t pathSize; // 文件路径字符串大小。含尾0。
};

struct FileBlockHeader {
	uint64_t offset; // 文件位置位移。
	uint64_t blockSize; // 文件块尺寸。
};
