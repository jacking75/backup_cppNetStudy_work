#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE    512

// ���� �Լ� ���� ��� �� ����
void err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// TCP ����(IPv4)
DWORD WINAPI TCPServer4(LPVOID arg)
{
	int retval;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if(retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	char buf[BUFSIZE+1];

	while(1){
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
		if(client_sock == INVALID_SOCKET){
			err_display("accept()");
			break;
		}

		// ������ Ŭ���̾�Ʈ ���� ���
		printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		// Ŭ���̾�Ʈ�� ������ ���
		while(1){
			// ������ �ޱ�
			retval = recv(client_sock, buf, BUFSIZE, 0);
			if(retval == SOCKET_ERROR){
				err_display("recv()");
				break;
			}
			else if(retval == 0)
				break;

			// ���� ������ ���
			buf[retval] = '\0';
			printf("%s", buf);
		}

		// closesocket()
		closesocket(client_sock);
		printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
	}

	// closesocket()
	closesocket(listen_sock);

	return 0;
}

// TCP ����(IPv6)
DWORD WINAPI TCPServer6(LPVOID arg)
{
	int retval;

	// socket()
	SOCKET listen_sock = socket(AF_INET6, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN6 serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin6_family = AF_INET6;
	serveraddr.sin6_addr = in6addr_any;
	serveraddr.sin6_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if(retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	SOCKADDR_IN6 clientaddr;
	int addrlen;
	char buf[BUFSIZE+1];

	while(1){
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
		if(client_sock == INVALID_SOCKET){
			err_display("accept()");
			break;
		}

		// ������ Ŭ���̾�Ʈ ���� ���
		char ipaddr[50];
		DWORD ipaddrlen = sizeof(ipaddr);
		WSAAddressToString((SOCKADDR *)&clientaddr, sizeof(clientaddr),
			NULL, ipaddr, &ipaddrlen);
		printf("\n[TCP ����] Ŭ���̾�Ʈ ����: %s\n", ipaddr);

		// Ŭ���̾�Ʈ�� ������ ���
		while(1){
			// ������ �ޱ�
			retval = recv(client_sock, buf, BUFSIZE, 0);
			if(retval == SOCKET_ERROR){
				err_display("recv()");
				break;
			}
			else if(retval == 0)
				break;

			// ���� ������ ���
			buf[retval] = '\0';
			printf("%s", buf);
		}

		// closesocket()
		closesocket(client_sock);
		printf("[TCP ����] Ŭ���̾�Ʈ ����: %s\n", ipaddr);
	}

	// closesocket()
	closesocket(listen_sock);

	return 0;
}

int main(int argc, char *argv[])
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, TCPServer4, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, TCPServer6, NULL, 0, NULL);

	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	// ���� ����
	WSACleanup();
	return 0;
}