#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2bth.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2, 2), &wsa)!=0) return 1;

	// ������� ��ġ �˻� �غ�
	DWORD qslen = sizeof(WSAQUERYSET);
	WSAQUERYSET *qs = (WSAQUERYSET *)malloc(qslen);
	ZeroMemory(qs, qslen);
	qs->dwSize = qslen;
	qs->dwNameSpace = NS_BTH;
	DWORD flags = LUP_CONTAINERS; /* �ʼ�! */
	flags |= LUP_FLUSHCACHE | LUP_RETURN_NAME | LUP_RETURN_ADDR;

	// ������� ��ġ �˻� ����
	HANDLE hLookup;
	retval = WSALookupServiceBegin(qs, flags, &hLookup);
	if(retval == SOCKET_ERROR){
		printf("[����] �߰ߵ� ������� ��ġ ����!\n");
		exit(1);
	}

	// �˻��� ������� ��ġ ���� ���
	SOCKADDR_BTH *sa = NULL;
	bool done = false;
	while(!done){
		retval = WSALookupServiceNext(hLookup, flags, &qslen, qs);
		if(retval == NO_ERROR){
			// ������� ��ġ ������ ��� �ִ� ���� �ּ� ����ü�� ����
			sa = (SOCKADDR_BTH *)qs->lpcsaBuffer->RemoteAddr.lpSockaddr;
			// ������� ��ġ �ּҸ� ���ڿ��� ���
			char addr[40] = {0};
			DWORD addrlen = sizeof(addr);
			WSAAddressToString((SOCKADDR *)sa, sizeof(SOCKADDR_BTH),
				NULL, addr, &addrlen);
			printf("������� ��ġ �߰�! %s - %s\n",
				addr, qs->lpszServiceInstanceName);
		}
		else{
			if(WSAGetLastError() == WSAEFAULT){
				free(qs);
				qs = (WSAQUERYSET *)malloc(qslen);
			}
			else{
				done = true;
			}
		}
	}

	// ������� ��ġ �˻� ����
	WSALookupServiceEnd(hLookup);
	free(qs);

	// ���� ����
	WSACleanup();
	return 0;
}