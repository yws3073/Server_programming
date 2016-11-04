#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment (lib, "ws2_32.lib")

#define BUFSIZE 2048

void error_handling(char *message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

int main(int argc, char **argv) {

	FILE *fp;

	WSADATA wsaData;
	SOCKET hservSock, hclntSock;
	SOCKADDR_IN servAddr, clntAddr;

	int clntAddrSize;
	int len;

	char buffer[BUFSIZE] = { 0 };

	int send_fsize, recv_fsize;

	if (argc != 4)
		error_handling("Insert Ex : f_serv [send_file] [recv_file] [port]");

	fp = fopen(argv[1], "rb");
	if (fp == NULL)
		error_handling("SEND FILE OPEN ERROR");

	fseek(fp, 0, SEEK_END);
	send_fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		error_handling("WSASTARTUP ERROR");

	hservSock = socket(PF_INET, SOCK_STREAM, 0);
	if (hservSock == INVALID_SOCKET)
		error_handling("SOCKET OPEN ERROR");

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(atoi(argv[3]));

	if (bind(hservSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		error_handling("BIND ERROR");

	if (listen(hservSock, 5) == SOCKET_ERROR)
		error_handling("LISTEN ERROR");

	clntAddrSize = sizeof(clntAddr);
	hclntSock = accept(hservSock, (SOCKADDR*)&clntAddr, &clntAddrSize);
	if (hclntSock == INVALID_SOCKET)
		error_handling("ACCEPT ERROR");

	// 파일의 크기를 문자열로 바꾸어 SYNC를 맞추기 위해 전송하자.
	_snprintf(buffer, sizeof(buffer), "%d", send_fsize);
	len = send(hclntSock, buffer, sizeof(char) * 1024, 0);
	if (len == SOCKET_ERROR)
		error_handling("FILE SIZE SYNC MSG ERROR");

	len = recv(hclntSock, buffer, sizeof(buffer) - 1, 0);
	buffer[len] = '\0';
	printf("FILE SIZE SYNC ====> %s BYTES     [ OK ]\n", buffer);

	len = fread(buffer, sizeof(char), send_fsize, fp);
	len = send(hclntSock, buffer, send_fsize, 0);
	printf("DATA SENDING TO CLIENT\n");
	if (len == SOCKET_ERROR)
		error_handling("FILE SEND ERROR");

	fclose(fp);

	len = recv(hclntSock, buffer, sizeof(buffer) - 1, 0);
	buffer[len] = '\0';
	printf("%s", buffer);

	len = send(hclntSock, "SENDING TARGET CHANGE     [ CLIENT -> SERVER ]\n", 70, 0);
	if (len == SOCKET_ERROR)
		error_handling("ACK REQUEST ERROR");

	Sleep(2000);

	len = recv(hclntSock, buffer, sizeof(buffer) - 1, 0);
	recv_fsize = atoi(buffer);
	buffer[len] = '\0';
	printf("FILE SIZE SYNC ====> %s BYTES     [ OK ]\n", buffer);

	Sleep(2000);

	len = send(hclntSock, buffer, sizeof(buffer) - 1, 0);
	fp = fopen(argv[2], "wb");
	if (fp == NULL)
		error_handling("RECV FILE OPEN ERROR");

	len = recv(hclntSock, buffer, sizeof(buffer) - 1, 0);
	buffer[len] = '\0';
	printf("DATA SENDING FROM CLIENT\n");
	fwrite(buffer, sizeof(char), len, fp);
	Sleep(2000);

	if (recv_fsize == len) {
		send(hclntSock, "DATA TRANSMISSION FILE SIZE     [ OK ]\nTHANK YOU! COMPLETE!\n", 70, 0);
	}
	else {
		send(hclntSock, "DATA TRANSEMISSION FILE SIZE     [ BAD ]\nSEND DATA FAILED..\n", 70, 0);
		printf("DATA TRANSEMISSION FILE SIZE     [ BAD ]\nRECV DATA FAILED..\n");
	}

	if (shutdown(hclntSock, SD_RECEIVE) == -1)
		error_handling("SHUTDOWN ERROR(RD-MODE)");

	fclose(fp);
	closesocket(hclntSock);
	WSACleanup();

	return 0;
}