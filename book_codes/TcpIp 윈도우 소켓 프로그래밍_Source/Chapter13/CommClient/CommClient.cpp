#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFSIZE 512

void err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

int main(int argc, char *argv[])
{
	int retval;

	// ���� ��Ʈ ����
	HANDLE hComm;
	hComm = CreateFile("COM1", GENERIC_READ|GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);
	if(hComm == INVALID_HANDLE_VALUE) err_quit("CreateFile()");

	// ���� ��Ʈ ������ ���
	DCB dcb;
	if(!GetCommState(hComm, &dcb)) err_quit("GetCommState()");

	// ���� ��Ʈ ������ �����ϱ�
	dcb.BaudRate = CBR_57600;
	dcb.ByteSize = 8;
	dcb.fParity = FALSE;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	if(!SetCommState(hComm, &dcb)) err_quit("SetCommState()");

	// �б�� ���� Ÿ�Ӿƿ� �����ϱ�
	COMMTIMEOUTS timeouts;
	timeouts.ReadIntervalTimeout = 0;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;
	if(!SetCommTimeouts(hComm, &timeouts)) err_quit("SetCommTimeouts()");

	// ������ ��ſ� ����� ����
	char buf[BUFSIZE+1];
	int len;
	DWORD BytesRead, BytesWritten;

	// ������ ������ ���
	while(1){
		// ������ �Է�
		ZeroMemory(buf, sizeof(buf));
		printf("\n[���� ������] ");
		if(fgets(buf, BUFSIZE+1, stdin) == NULL)
			break;

		// '\n' ���� ����
		len = strlen(buf);
		if(buf[len-1] == '\n') buf[len-1] = '\0';
		if(strlen(buf) == 0) break;

		// ������ ������
		retval = WriteFile(hComm, buf, BUFSIZE, &BytesWritten, NULL);
		if(retval == 0) err_quit("WriteFile()");
		printf("[Ŭ���̾�Ʈ] %d����Ʈ�� ���½��ϴ�.\n", BytesWritten);

		// ������ �ޱ�
		retval = ReadFile(hComm, buf, BUFSIZE, &BytesRead, NULL);
		if(retval == 0) err_quit("ReadFile()");

		// ���� ������ ���
		buf[BytesRead] = '\0';
		printf("[Ŭ���̾�Ʈ] %d����Ʈ�� �޾ҽ��ϴ�.\n", BytesRead);
		printf("[���� ������] %s\n", buf);
	}

	// ���� ��Ʈ �ݱ�
	CloseHandle(hComm);
	return 0;
}