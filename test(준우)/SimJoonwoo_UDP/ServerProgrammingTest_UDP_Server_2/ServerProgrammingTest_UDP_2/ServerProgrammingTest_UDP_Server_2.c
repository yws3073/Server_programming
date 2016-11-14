#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#pragma comment (lib, "ws2_32.lib")
#if defined(_WIN32)
#include <windows.h>
#define sleep(sec) Sleep((sec)*1000)
//��ó���� ��������, windows.h�� ����, sleep��ɾ Sleep���� ����
#elif defined(__linux__)
#include <sys/utime.h>
#inclue <unistd.h>
//���������, sys/utie.h, unistd.h�� ���
#endif

typedef unsigned long long u64;
#define BUF_SIZE 512
/* BUF_SIZE �ʰ��� ���ڿ� ���۽� �����ߴµ�*/
void ErrorHandling(char *message);
u64 GetMicroCounter();
void ErrorHandling(char *message);
int control = 0;

int main(int argc, char *argv[])
{
	u64 start, end;
	int sum, i;

	WSADATA wsaData;
	SOCKET servSock;
	char message[BUF_SIZE];
	int strLen;
	int clntAdrSz;
	FILE *fp;
	SOCKADDR_IN servAdr, clntAdr;
	if (argc != 3) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	fp = fopen(argv[2], "wb");

	servSock = socket(PF_INET, SOCK_DGRAM, 0);
	if (servSock == INVALID_SOCKET)
		ErrorHandling("UDP socket creation error");

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(atoi(argv[1]));

	if (bind(servSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("bind() error");

	puts("Server connected");
	//bind����? serverconnected��� ���â

	{

		clntAdrSz = sizeof(clntAdr);
		int retval;
		char buf[BUF_SIZE];
		int flag = 0;

		while (strLen = recvfrom(servSock, message, BUF_SIZE, 0, (SOCKADDR*)&clntAdr, &clntAdrSz))
			//Ư���ϳ�, 0�̵Ǹ� �˾Ƽ� �׸��δϱ�, �̷����� ���ϴ�. ��..
		{
			control++;
			if (control % 10000 == 0)
				printf("���� �� �Դϴ�...%d\n", control / 10000);
			if (!flag) {
				start = GetMicroCounter();
				flag = 1;
			}
			fwrite((void*)message, 1, strLen, fp);

			if (strLen != BUF_SIZE) {
				//puts("���!");
				break;
			}
		}
	}
	printf("Received file [%s] from [IP: %s, Port: %d]\n", argv[2], inet_ntoa(clntAdr.sin_addr), ntohs(clntAdr.sin_port));
	//argv[2] -> �Է¹޾Ҵ� file, ip, port ���
	puts("Sended file data");
	closesocket(servSock);
	WSACleanup();
	fclose(fp);
	end = GetMicroCounter();
	printf("Elapsed Time (micro seconds) : %d\n", end - start);
	//end - start�� �� �ð��ΰ��� getMicroCounter�� �׷� ����ε��ϴ�.

	return 0;
}

void ErrorHandling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

u64 GetMicroCounter()
{
	u64 Counter;
#if defined(_WIN32)
	u64 Frequency;
	QueryPerformanceFrequency((LARGE_INTEGER *)&Frequency);
	QueryPerformanceCounter((LARGE_INTEGER *)&Counter);
	Counter = 1000000 * Counter / Frequency;
#elif defined(__linux__)
	struct timeval t;
	gettimeofday(&t, 0);
	Counter = 1000000 * t.tv_sec + t.tv_usec;
#endif

	return Counter;
}