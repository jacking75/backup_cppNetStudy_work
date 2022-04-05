#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE    512

// ���� ���� ������ ���� ����ü�� ����
struct SOCKETINFO
{
	WSAOVERLAPPED overlapped;
	SOCKET sock;
	char buf[BUFSIZE+1];
	int recvbytes;
	int sendbytes;
	WSABUF wsabuf;
};

int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[WSA_MAXIMUM_WAIT_EVENTS];
WSAEVENT EventArray[WSA_MAXIMUM_WAIT_EVENTS];
CRITICAL_SECTION cs;

// �񵿱� ����� ó�� �Լ�
DWORD WINAPI WorkerThread(LPVOID arg);
// ���� ���� �Լ�
BOOL AddSocketInfo(SOCKET sock);
void RemoveSocketInfo(int nIndex);
// ���� ��� �Լ�
void err_quit(char *msg);
void err_display(char *msg);

int main(int argc, char *argv[])
{
	int retval;
	InitializeCriticalSection(&cs);

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;

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

	// ����(dummy) �̺�Ʈ ��ü ����
	WSAEVENT hEvent = WSACreateEvent();
	if(hEvent == WSA_INVALID_EVENT)
		err_quit("WSACreateEvent()");
	EventArray[nTotalSockets++] = hEvent;

	// ������ ����
	HANDLE hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
	if(hThread == NULL) return 1;
	CloseHandle(hThread);

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	DWORD recvbytes, flags;

	while(1){
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
		if(client_sock == INVALID_SOCKET){
			err_display("accept()");
			break;
		}
		printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		// ���� ���� �߰�
		if(AddSocketInfo(client_sock) == FALSE){
			closesocket(client_sock);
			printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
				inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
			continue;
		}

		// �񵿱� ����� ����
		SOCKETINFO *ptr = SocketInfoArray[nTotalSockets-1];
		flags = 0;
		retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes,
			&flags, &ptr->overlapped, NULL);
		if(retval == SOCKET_ERROR){
			if(WSAGetLastError() != WSA_IO_PENDING){
				err_display("WSARecv()");
				RemoveSocketInfo(nTotalSockets-1);
				continue;
			}
		}

		// ������ ����(nTotalSockets) ��ȭ�� �˸�
		WSASetEvent(EventArray[0]);
	}

	// ���� ����
	WSACleanup();
	DeleteCriticalSection(&cs);
	return 0;
}

// �񵿱� ����� ó�� �Լ�
DWORD WINAPI WorkerThread(LPVOID arg)
{
	int retval;

	while(1){
		// �̺�Ʈ ��ü ����
		DWORD index = WSAWaitForMultipleEvents(nTotalSockets,
			EventArray, FALSE, WSA_INFINITE, FALSE);
		if(index == WSA_WAIT_FAILED) continue;
		index -= WSA_WAIT_EVENT_0;
		WSAResetEvent(EventArray[index]);
		if(index == 0) continue;

		// Ŭ���̾�Ʈ ���� ���
		SOCKETINFO *ptr = SocketInfoArray[index];
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(ptr->sock, (SOCKADDR *)&clientaddr, &addrlen);

		// �񵿱� ����� ��� Ȯ��
		DWORD cbTransferred, flags;
		retval = WSAGetOverlappedResult(ptr->sock, &ptr->overlapped,
			&cbTransferred, FALSE, &flags);
		if(retval == FALSE || cbTransferred == 0){
			RemoveSocketInfo(index);
			printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
				inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
			continue;
		}

		// ������ ���۷� ����
		if(ptr->recvbytes == 0){
			ptr->recvbytes = cbTransferred;
			ptr->sendbytes = 0;
			// ���� ������ ���
			ptr->buf[ptr->recvbytes] = '\0';
			printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr),
				ntohs(clientaddr.sin_port), ptr->buf);
		}
		else{
			ptr->sendbytes += cbTransferred;
		}

		if(ptr->recvbytes > ptr->sendbytes){
			// ������ ������
			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
			ptr->overlapped.hEvent = EventArray[index];
			ptr->wsabuf.buf = ptr->buf + ptr->sendbytes;
			ptr->wsabuf.len = ptr->recvbytes - ptr->sendbytes;

			DWORD sendbytes;
			retval = WSASend(ptr->sock, &ptr->wsabuf, 1, &sendbytes,
				0, &ptr->overlapped, NULL);
			if(retval == SOCKET_ERROR){
				if(WSAGetLastError() != WSA_IO_PENDING){
					err_display("WSASend()");
				}
				continue;
			}
		}
		else{
			ptr->recvbytes = 0;

			// ������ �ޱ�
			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
			ptr->overlapped.hEvent = EventArray[index];
			ptr->wsabuf.buf = ptr->buf;
			ptr->wsabuf.len = BUFSIZE;

			DWORD recvbytes;
			flags = 0;
			retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes,
				&flags, &ptr->overlapped, NULL);
			if(retval == SOCKET_ERROR){
				if(WSAGetLastError() != WSA_IO_PENDING){
					err_display("WSARecv()");
				}
				continue;
			}
		}
	}
}

// ���� ���� �߰�
BOOL AddSocketInfo(SOCKET sock)
{
	EnterCriticalSection(&cs);
	if(nTotalSockets >= WSA_MAXIMUM_WAIT_EVENTS) return FALSE;

	SOCKETINFO *ptr = new SOCKETINFO;
	if(ptr == NULL) return FALSE;

	WSAEVENT hEvent = WSACreateEvent();
	if(hEvent == WSA_INVALID_EVENT) return FALSE;

	ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
	ptr->overlapped.hEvent = hEvent;
	ptr->sock = sock;
	ptr->recvbytes = ptr->sendbytes = 0;
	ptr->wsabuf.buf = ptr->buf;
	ptr->wsabuf.len = BUFSIZE;
	SocketInfoArray[nTotalSockets] = ptr;
	EventArray[nTotalSockets] = hEvent;
	++nTotalSockets;

	LeaveCriticalSection(&cs);
	return TRUE;
}

// ���� ���� ����
void RemoveSocketInfo(int nIndex)
{
	EnterCriticalSection(&cs);

	SOCKETINFO *ptr = SocketInfoArray[nIndex];
	closesocket(ptr->sock);
	delete ptr;
	WSACloseEvent(EventArray[nIndex]);

	if(nIndex != (nTotalSockets-1)){
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets-1];
		EventArray[nIndex] = EventArray[nTotalSockets-1];
	}
	--nTotalSockets;

	LeaveCriticalSection(&cs);
}

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