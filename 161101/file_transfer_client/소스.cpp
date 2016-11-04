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
	SOCKET hSocket;

	SOCKADDR_IN servAddr;

	int len;
	int recv_size, send_size;
	char buffer[BUFSIZE];

	if (argc != 5)
		error_handling("Insert Ex : f_serv [send_file] [recv_file] [IP] [port]");

	fp = fopen(argv[2], "wb");
	if (fp == NULL)
		error_handling("RECV FILE OPEN ERROR");

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		error_handling("WSASTARTUP ERROR");

	hSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
		error_handling("SOCKET OPEN ERROR");

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(argv[3]);
	servAddr.sin_port = htons(atoi(argv[4]));

	if (connect(hSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		error_handling("CONNECTION ERROR");

	len = recv(hSocket, buffer, sizeof(buffer) - 1, 0);
	buffer[len] = '\0';
	recv_size = atoi(buffer);
	printf("FILE SIZE SYNC ====> %s BYTES     [ OK ]\n", buffer);
	Sleep(2000);

	len = send(hSocket, buffer, (int)strlen(buffer), 0);
	len = recv(hSocket, buffer, sizeof(buffer) - 1, 0);
	buffer[len] = '\0';
	printf("DATA SENDING FROM SERVER\n");
	fwrite(buffer, sizeof(char), len, fp);
	Sleep(2000);

	if (recv_size == len) {
		send(hSocket, "DATA TRANSMISSION FILE SIZE     [ OK ]\nTHANK YOU! COMPLETE!\n", 70, 0);
	}
	else {
		send(hSocket, "DATA TRANSEMISSION FILE SIZE     [ BAD ]\nSEND DATA FAILED..\n", 70, 0);
		printf("DATA TRANSEMISSION FILE SIZE     [ BAD ]\nRECV DATA FAILED..\n");
	}
	Sleep(2000);
	fclose(fp);

	fp = fopen(argv[1], "rb");
	if (fp == NULL)
		error_handling("SEND FILE OPEN ERROR");

	len = recv(hSocket, buffer, sizeof(buffer) - 1, 0);
	buffer[len] = '\0';
	printf("%s", buffer);

	fseek(fp, 0, SEEK_END);
	send_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	_snprintf(buffer, sizeof(buffer), "%ld", send_size);
	len = send(hSocket, buffer, sizeof(char) * 1024, 0);
	if (len == SOCKET_ERROR)
		error_handling("FILE SIZE SYNC ERROR");

	len = recv(hSocket, buffer, sizeof(buffer) - 1, 0);
	buffer[len] = '\0';
	printf("FILE SIZE SYNC ====> %s BYTES     [ OK ]\n", buffer);

	len = fread(buffer, sizeof(char), send_size, fp);
	len = send(hSocket, buffer, send_size, 0);
	if (len == SOCKET_ERROR)
		error_handling("FILE SEND ERROR");

	len = recv(hSocket, buffer, sizeof(buffer) - 1, 0);
	buffer[len] = '\0';
	printf("%s", buffer);

	closesocket(hSocket);
	WSACleanup();

	return 0;
}