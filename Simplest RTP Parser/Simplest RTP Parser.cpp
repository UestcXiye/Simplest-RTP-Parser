// Simplest RTP Parser.cpp : �������̨Ӧ�ó������ڵ㡣
//

/**
* ��򵥵� UDP-RTP Э���������
* Simplest RTP Parser
*
* ԭ����
* ������ Lei Xiaohua
* leixiaohua1020@126.com
* �й���ý��ѧ/���ֵ��Ӽ���
* Communication University of China / Digital TV Technology
* http://blog.csdn.net/leixiaohua1020
*
* �޸ģ�
* ���ĳ� Liu Wenchen
* 812288728@qq.com
* ���ӿƼ���ѧ/������Ϣ
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* ����Ŀ��һ�� FLV ��װ��ʽ�������򣬿��Է��� UDP/RTP/MPEG-TS ���ݰ���
*
* This project is the simplest UDP-RTP protocol parser,
* can analyze UDP/RTP/MPEG-TS packets.
*
*/

#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996) // ��� fopen ����ȫ����
#pragma pack(1)

/*
* [memo] FFmpeg stream Command:
* ffmpeg -re -i sintel.ts -f mpegts udp://127.0.0.1:8880
* ffmpeg -re -i sintel.ts -f rtp_mpegts udp://127.0.0.1:8880
*/

typedef struct RTP_FIXED_HEADER
{
	/* byte 0 */
	unsigned char csrc_count : 4; // CSRC ��������ָʾ CSRC ��ʶ���ĸ���
	unsigned char extension : 1; // ��� X=1����չͷ������� CSRC ֮��Я��һЩ������Ϣ
	unsigned char padding : 1; // ��� P=1����� RTP ����β���͸���һ����������İ�λ�飬���ǲ�����Ч�غɵ�һ����
	unsigned char version : 2; // �汾�ţ�����ʹ�õĶ��ǵ� 2 ���汾�����Ը��ֶι̶�Ϊ 2
	/* byte 1 */
	unsigned char payload_type : 7; // ��ʶ�� RTP �غɵ�����
	unsigned char marker : 1; // ��λ�Ľ����������ĵ�������һ����������ڱ�ʶ�߽磨������Ƶ�����һ֡�Ľ�����������Ƶ����ǻỰ�Ŀ�ʼ����

	/* bytes 2, 3 */
	unsigned short sequence_number; // RTP �����к�

	/* bytes 4-7 */
	unsigned  long timestamp; // ʱ�������ӳ�˸� RTP �������ݵĵ�һ����λ��Ĳ���ʱ��

	/* bytes 8-11 */
	unsigned long ssrc; // ͬ��Դ��ʶ������ʶ RTP ��������Դ����ͬԴ��������֮���� SSRC �ֶ�����

} RTP_FIXED_HEADER; // RTP Header ռ 12 �ֽ�


typedef struct MPEGTS_FIXED_HEADER
{
	/* byte 0 */
	unsigned sync_byte : 8; // ͬ���ֽڣ�ֵΪ 0x47

	/* byte 1, 2 */
	unsigned transport_error_indicator : 1; // �������ָʾλ���� 1 ʱ����ʾ���Ͱ���������һ�����ɾ����Ĵ���λ
	unsigned payload_unit_start_indicator : 1; // ���ص�Ԫ��ʼָ��λ����ʾ�� TS ���� PES ���ĵ�һ�����ص�Ԫ
	unsigned transport_priority : 1; // �������ȼ��������ð���ͬ�� PID �ĵ�δ��λ�� TS ���и��ߵ����ȼ�
	unsigned PID : 13; // �� TS ���� ID �ţ���������� PAT ������ PID �̶�Ϊ 0x00

	/* byte 3 */
	unsigned transport_scrambling_control : 2; // ������ſ���λ
	unsigned adaptation_field_control : 2; // ����Ӧ���������λ����λ������� TS ����������Ӧ�����ֶ�
	unsigned continuity_counter : 4;// ���������������ž�����ͬ PID �� TS �������Ӷ����ӣ��ﵽ���ʱ�ָ�Ϊ 0
	/* �������������ͬ PID �� TS ��������ͬ�ļ��������������������һ���ģ�ֻȡһ���������ɡ� */
} MPEGTS_FIXED_HEADER; // MPEG-TS ��ͷռ 4 �ֽ�


int simplest_udp_parser(int port)
{
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	int cnt = 0;

	// FILE *myout = fopen("output_log.txt", "wb+");
	FILE *myout = stdout;
	FILE *fp1 = fopen("output_dump.ts", "wb+");

	// ���ȵ��� WSAStartup ������ɶ� Winsock ����ĳ�ʼ��
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		return INVALID_SOCKET;
	}
	/* ���� socket ��������������������
	1. af������ʹ�õ�ͨ��Э���壬���� TCP/IP��ֵΪ AF_INET
	2. type��Ҫ�������׽������ͣ����׽�������Ϊ SOCK_STREAM��TCP�������ݱ��׽�������Ϊ SOCK_DGRAM��UDP�������� SOCK_RAW��ԭʼ socket��
	3. protocol������ʹ�õ�ͨ��Э�飬���� 0��ϵͳ���Զ����������Э��
	*/
	SOCKET serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serSocket == INVALID_SOCKET)
	{
		printf("socket error!");
		return 0;
	}

	sockaddr_in serAddr;
	// sin_family ָ����Э����/��ͨ�� AF_INET��AF_INET6��AF_LOCAL ��
	serAddr.sin_family = AF_INET;
	// sin_port ���˿ںţ�ʹ�������ֽ��򣬼����ģʽ
	serAddr.sin_port = htons(port); // htons(port) �� 16 λ���������ֽ���С���ֽ���ת���������ֽ��򣨴���ֽ���
	// sin_addr �洢 IP ��ַ��ʹ�� in_addr ������ݽṹ��Ҳʹ�������ֽ���
	// ����� INADDR_ANY ����ָ����ַΪ 0.0.0.0 �ĵ�ַ�������ַ��ʵ�ϱ�ʾ��ȷ����ַ��һ���ڸ���ϵͳ�о�����Ϊȫ 0
	serAddr.sin_addr.S_un.S_addr = INADDR_ANY;

	// bind �������Խ�һ��̶��ĵ�ַ�󶨵� sockfd ��
	if (bind(serSocket, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
	{
		printf("bind error!");
		closesocket(serSocket);
		return 0;
	}

	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr);

	// How to parse?
	int parse_rtp = 1;
	int parse_mpegts = 1;

	printf("Listening on port %d.\n", port);

	char recvData[10000];
	while (1)
	{
		// �� socket ���ջ������������ݵ� recvData
		int pktsize = recvfrom(serSocket, recvData, 10000, 0, (sockaddr *)&remoteAddr, &nAddrLen);
		if (pktsize > 0)
		{
			// printf("Addr:%s\r\n", inet_ntoa(remoteAddr.sin_addr));
			// printf("packet size:%d\r\n", pktsize);

			// Parse RTP
			if (parse_rtp != 0)
			{
				char payload_str[10] = { 0 };
				RTP_FIXED_HEADER rtp_header;
				int rtp_header_size = sizeof(RTP_FIXED_HEADER);
				// RTP Header
				memcpy((void *)&rtp_header, recvData, rtp_header_size);

				// RFC3551
				char payloadType = rtp_header.payload_type;
				switch (payloadType)
				{
				case 0: sprintf(payload_str, "PCMU"); break;
				case 1:
				case 2: sprintf(payload_str, "reserved"); break;
				case 3: sprintf(payload_str, "GSM"); break;
				case 4: sprintf(payload_str, "G723"); break;
				case 5:
				case 6: sprintf(payload_str, "DVI4"); break;
				case 7: sprintf(payload_str, "LPC"); break;
				case 8: sprintf(payload_str, "PCMA"); break;
				case 9: sprintf(payload_str, "G722"); break;
				case 10:
				case 11: sprintf(payload_str, "L16"); break;
				case 12: sprintf(payload_str, "QCELP"); break;
				case 13: sprintf(payload_str, "CN"); break;
				case 14: sprintf(payload_str, "MPA"); break;
				case 15: sprintf(payload_str, "G728"); break;
				case 16:
				case 17: sprintf(payload_str, "DVI4"); break;
				case 18: sprintf(payload_str, "G729"); break;
				case 19: sprintf(payload_str, "reserved"); break;
				case 25: sprintf(payload_str, "CelB"); break;
				case 26: sprintf(payload_str, "JPEG"); break;
				case 28: sprintf(payload_str, "nv"); break;
				case 31: sprintf(payload_str, "H.261"); break;
				case 32: sprintf(payload_str, "MPV"); break;
				case 33: sprintf(payload_str, "MP2T"); break;
				case 34: sprintf(payload_str, "H.263"); break;
				case 72:
				case 73:
				case 74:
				case 75:
				case 76: sprintf(payload_str, "reserved"); break;
				case 96: sprintf(payload_str, "H.264"); break;
				default: sprintf(payload_str, "other"); break;
				}

				unsigned int timestamp = ntohl(rtp_header.timestamp);
				unsigned int seq_no = ntohs(rtp_header.sequence_number);

				fprintf(myout, "[RTP Pkt] %5d| %5s| %10u| %5d| %5d|\n", cnt, payload_str, timestamp, seq_no, pktsize);

				// RTP Data
				char *rtp_data = recvData + rtp_header_size;
				int rtp_data_size = pktsize - rtp_header_size;
				fwrite(rtp_data, rtp_data_size, 1, fp1);

				// Parse MPEGTS
				if (parse_mpegts != 0 && payloadType == 33)
				{
					MPEGTS_FIXED_HEADER mpegts_header;
					// һ�� TS �����ȹ̶� 188 �ֽ�
					for (int i = 0; i < rtp_data_size; i = i + 188)
					{
						if (rtp_data[i] != 0x47) // �ж�ͬ���ֽڣ��̶�Ϊ 0x47
							break;
						// MPEGTS Header
						// memcpy((void *)&mpegts_header, rtp_data + i, sizeof(MPEGTS_FIXED_HEADER));
						fprintf(myout, "   [MPEGTS Pkt]\n");
					}
				}
			}
			else
			{
				fprintf(myout, "[UDP Pkt] %5d| %5d|\n", cnt, pktsize);
				fwrite(recvData, pktsize, 1, fp1);
			}

			cnt++;
		}
	}

	closesocket(serSocket);
	WSACleanup();
	fclose(fp1);

	return 0;
}

int main()
{
	simplest_udp_parser(8880);

	system("pause");
	return 0;
}