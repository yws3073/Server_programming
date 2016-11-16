#define _CRT_SECURE_NO_WARNINGS
//�̰� �Ƚ��ָ� fopen -> fopen_s�� �����
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#pragma comment (lib, "ws2_32.lib")

#define BUF_SIZE 512
void ErrorHandling(char *message);
int control = 0;

int main(int argc, char *argv[])
{
	WSADATA wsaData;
	SOCKET sock;
	char buf[BUF_SIZE];
	int strLen;
	FILE *fp;
	SOCKADDR_IN servAdr;

	if (argc != 4) {
		printf("Usage: %s <IP> <port>\n", argv[0]);
		exit(1);
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");
	fp = fopen(argv[3], "rb");

	sock = socket(PF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET)
		ErrorHandling("socket() error");

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = inet_addr(argv[1]);
	servAdr.sin_port = htons(atoi(argv[2]));

	{
		connect(sock, (SOCKADDR*)&servAdr, sizeof(servAdr));
		//socket�� connect�ϴµ�, IPV4 (SOCKADDR)ĳ���� ��������, ũ��� servAdr���� ��
		while (1)
		{
			control++;
			if (control % 10000 == 0)
				printf("���� �� �Դϴ�...%d\n", control / 10000);
			strLen = fread((void*)buf, 1, BUF_SIZE, fp);
			//1����Ʈ�� �����ڴٴ°ǰ�..;;

			if (strLen<BUF_SIZE)
			{
				sendto(sock, (char*)&buf, strLen, 0, (SOCKADDR *)&servAdr, sizeof(servAdr));
				break;
			}
			sendto(sock, (char*)&buf, strLen, 0, (SOCKADDR *)&servAdr, sizeof(servAdr));
		}

		shutdown(sock, SD_SEND);
	}

	puts("Sended file data");
	sendto(sock, "Thank you", 10, 0, (SOCKADDR *)&servAdr, sizeof(servAdr));
	fclose(fp);
	closesocket(sock);
	WSACleanup();
	return 0;
}

void ErrorHandling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}