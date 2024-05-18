// Simplest RTP Parser.cpp : 定义控制台应用程序的入口点。
//

/**
* 最简单的 UDP-RTP 协议解析程序
* Simplest RTP Parser
*
* 原程序：
* 雷霄骅 Lei Xiaohua
* leixiaohua1020@126.com
* 中国传媒大学/数字电视技术
* Communication University of China / Digital TV Technology
* http://blog.csdn.net/leixiaohua1020
*
* 修改：
* 刘文晨 Liu Wenchen
* 812288728@qq.com
* 电子科技大学/电子信息
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* 本项目是一个 FLV 封装格式解析程序，可以分析 UDP/RTP/MPEG-TS 数据包。
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
#pragma warning(disable:4996) // 解决 fopen 不安全报错
#pragma pack(1)

/*
* [memo] FFmpeg stream Command:
* ffmpeg -re -i sintel.ts -f mpegts udp://127.0.0.1:8880
* ffmpeg -re -i sintel.ts -f rtp_mpegts udp://127.0.0.1:8880
*/

typedef struct RTP_FIXED_HEADER
{
	/* byte 0 */
	unsigned char csrc_count : 4; // CSRC 计数器，指示 CSRC 标识符的个数
	unsigned char extension : 1; // 如果 X=1，拓展头部会放在 CSRC 之后，携带一些附加信息
	unsigned char padding : 1; // 如果 P=1，则该 RTP 包的尾部就附加一个或多个额外的八位组，它们不是有效载荷的一部分
	unsigned char version : 2; // 版本号，现在使用的都是第 2 个版本，所以该字段固定为 2
	/* byte 1 */
	unsigned char payload_type : 7; // 标识了 RTP 载荷的类型
	unsigned char marker : 1; // 该位的解释由配置文档决定，一般情况下用于标识边界（对于视频，标记一帧的结束；对于音频，标记会话的开始）。

	/* bytes 2, 3 */
	unsigned short sequence_number; // RTP 包序列号

	/* bytes 4-7 */
	unsigned  long timestamp; // 时间戳，反映了该 RTP 报文数据的第一个八位组的采样时刻

	/* bytes 8-11 */
	unsigned long ssrc; // 同步源标识符，标识 RTP 包流的来源，不同源的数据流之间用 SSRC 字段区分

} RTP_FIXED_HEADER; // RTP Header 占 12 字节


typedef struct MPEGTS_FIXED_HEADER
{
	/* byte 0 */
	unsigned sync_byte : 8; // 同步字节，值为 0x47

	/* byte 1, 2 */
	unsigned transport_error_indicator : 1; // 传输错误指示位，置 1 时，表示传送包中至少有一个不可纠正的错误位
	unsigned payload_unit_start_indicator : 1; // 负载单元起始指标位，表示该 TS 包是 PES 包的第一个负载单元
	unsigned transport_priority : 1; // 传输优先级，表明该包比同个 PID 的但未置位的 TS 包有更高的优先级
	unsigned PID : 13; // 该 TS 包的 ID 号，如果净荷是 PAT 包，则 PID 固定为 0x00

	/* byte 3 */
	unsigned transport_scrambling_control : 2; // 传输加扰控制位
	unsigned adaptation_field_control : 2; // 自适应调整域控制位，置位则表明该 TS 包存在自适应调整字段
	unsigned continuity_counter : 4;// 连续计数器，随着具有相同 PID 的 TS 包的增加而增加，达到最大时恢复为 0
	/* 如果两个连续相同 PID 的 TS 包具有相同的计数，则表明这两个包是一样的，只取一个解析即可。 */
} MPEGTS_FIXED_HEADER; // MPEG-TS 包头占 4 字节


int simplest_udp_parser(int port)
{
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	int cnt = 0;

	// FILE *myout = fopen("output_log.txt", "wb+");
	FILE *myout = stdout;
	FILE *fp1 = fopen("output_dump.ts", "wb+");

	// 首先调用 WSAStartup 函数完成对 Winsock 服务的初始化
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		return INVALID_SOCKET;
	}
	/* 调用 socket 函数，它有三个参数：
	1. af：程序使用的通信协议族，对于 TCP/IP，值为 AF_INET
	2. type：要创建的套接字类型，流套接字类型为 SOCK_STREAM（TCP），数据报套接字类型为 SOCK_DGRAM（UDP），还有 SOCK_RAW（原始 socket）
	3. protocol：程序使用的通信协议，若置 0，系统会自动决定传输层协议
	*/
	SOCKET serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serSocket == INVALID_SOCKET)
	{
		printf("socket error!");
		return 0;
	}

	sockaddr_in serAddr;
	// sin_family 指明了协议族/域，通常 AF_INET、AF_INET6、AF_LOCAL 等
	serAddr.sin_family = AF_INET;
	// sin_port 即端口号，使用网络字节序，即大端模式
	serAddr.sin_port = htons(port); // htons(port) 将 16 位数从主机字节序（小端字节序）转换成网络字节序（大端字节序）
	// sin_addr 存储 IP 地址，使用 in_addr 这个数据结构，也使用网络字节序
	// 这里的 INADDR_ANY 就是指定地址为 0.0.0.0 的地址，这个地址事实上表示不确定地址，一般在各个系统中均定义为全 0
	serAddr.sin_addr.S_un.S_addr = INADDR_ANY;

	// bind 函数可以将一组固定的地址绑定到 sockfd 上
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
		// 从 socket 接收缓冲区拷贝数据到 recvData
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
					// 一个 TS 包长度固定 188 字节
					for (int i = 0; i < rtp_data_size; i = i + 188)
					{
						if (rtp_data[i] != 0x47) // 判断同步字节，固定为 0x47
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