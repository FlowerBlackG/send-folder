/**
 * �ļ����乤�߼���
 * 
 * �涨ÿ���ļ��Ĵ��䷽ʽ��
 *   client: ���� FileTransportHeader 
 *   server: ��ȡ FileTransportHeader
 *   client: �����ļ�·���ַ�����Ȼ��ȴ���Ӧ
 *           ! �����жϣ������·��� FileTransportHeader
 *   server: ��ȡ�ļ�·���ַ����������������ļ�
 *   server: �� 8 bit ���ػ����ļ��������
 *           0x00: �ɹ�
 *           0x01: �����������ļ�����ʧ��
 *   client: ��ȡ����״̬��ʧ�����˳����ɹ���ѭ�������ļ���
 *   
 * �涨ÿ���ļ���Ĵ��䷽ʽ��
 *   client: ���� FileBlockHeader
 *   server: ��ȡ FileBlockHeader, ����������
 *   client: ���� 1 ���ļ��飬Ȼ��ȴ���Ӧ
 *   server: �����ļ��飬д�ļ������� 8 bit ���ؽ��
 *           0x00: �ɹ�
 *           0x01: ���������绺��������ʧ�ܡ�д�ļ�ʧ��
 *           0x02: ������������Ҫ���´����
 *   client: ��ȡ����״̬���������˳�����Ҫ���´������ط��ļ��飬�ɹ������
 *           ! �����жϣ������½����ļ���Ϣ��Ȼ�����·��Ϳ���Ϣ����ͷ��
 * 
 * �� FileTransportHeader �е� fileSize Ϊ -1, ��ʾ�ļ�������ϣ��ɹر�����
 * �� FileBlockHeader �е� blockSize Ϊ -1, ��ʾ�ļ�������ϣ��ɱ����ļ�
 */

#pragma once

#include <cstdint>

const char* TRANS_DATA_TYPE_FTH = "dtfth";
const char* TRANS_DATA_TYPE_FBH = "dtfbh";
const char* TRANS_DATA_TYPE_DAT = "dtdat";
const int TRANS_DATA_HEADER_SIZE = 6;

struct FileTransportHeader {
	uint64_t fileSize; // �ļ���С��
	uint64_t pathSize; // �ļ�·���ַ�����С����β0��
};

struct FileBlockHeader {
	uint64_t offset; // �ļ�λ��λ�ơ�
	uint64_t blockSize; // �ļ���ߴ硣
};
