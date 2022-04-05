#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFSIZE 1500

// IP ���
typedef struct _IPHEADER
{
	u_char  ip_hl:4;  // header length
	u_char  ip_v:4;   // version
	u_char  ip_tos;   // type of service
	short   ip_len;   // total length
	u_short ip_id;    // identification
	short   ip_off;   // flags & fragment offset field
	u_char  ip_ttl;   // time to live
	u_char  ip_p;     // protocol
	u_short ip_cksum; // checksum
	IN_ADDR ip_src;   // source address
	IN_ADDR ip_dst;   // destination address
} IPHEADER;

// ICMP �޽���
typedef struct _ICMPMESSAGE
{
	u_char  icmp_type;  // type of message
	u_char  icmp_code;  // type sub code
	u_short icmp_cksum; // checksum
	u_short icmp_id;    // identifier
	u_short icmp_seq;   // sequence number
} ICMPMESSAGE;

#define ICMP_ECHOREPLY   0
#define ICMP_DESTUNREACH 3
#define ICMP_ECHOREQUEST 8
#define ICMP_TIMEOUT     11

// ICMP �޽��� �м� �Լ�
BOOL DecodeICMPMessage(char *, int, SOCKADDR_IN *, SOCKADDR_IN *);
// ������ �̸� -> IPv4 �ּ� ��ȯ �Լ�
BOOL GetIPAddr(char *name, IN_ADDR *addr);
// üũ�� ��� �Լ�
u_short checksum(u_short *buffer, int size);
// ���� ��� �Լ�
void err_quit(char *msg);
void err_display(char *msg);

int main(int argc, char *argv[])
{
	int retval;

	if(argc < 2){
		printf("Usage: %s <host_name>\n", argv[0]);
		return 1;
	}

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if(sock == INVALID_SOCKET) err_quit("socket()");

	// ���� �ɼ� ����
	int optval = 1000;
	retval = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
		(char *)&optval, sizeof(optval));
	if(retval == SOCKET_ERROR) err_quit("setsockopt()");

	retval = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
		(char *)&optval, sizeof(optval));
	if(retval == SOCKET_ERROR) err_quit("setsockopt()");

	// ���� �ּ� ����ü �ʱ�ȭ
	SOCKADDR_IN destaddr;
	ZeroMemory(&destaddr, sizeof(destaddr));
	destaddr.sin_family = AF_INET;
	IN_ADDR addr;
	if(GetIPAddr(argv[1], &addr))
		destaddr.sin_addr = addr;
	else
		return 1;

	// ������ ��ſ� ����� ����
	ICMPMESSAGE icmpmsg;
	char buf[BUFSIZE];
	SOCKADDR_IN peeraddr;
	int addrlen;

	for(int ttl=1; ttl<=30; ttl++){
		// TTL �� ����
		optval = ttl;
		retval = setsockopt(sock, IPPROTO_IP, IP_TTL,
			(char *)&optval, sizeof(optval));
		if(retval == SOCKET_ERROR) err_quit("setsockopt()");

		// ICMP �޽��� �ʱ�ȭ
		ZeroMemory(&icmpmsg, sizeof(icmpmsg));
		icmpmsg.icmp_type = ICMP_ECHOREQUEST;
		icmpmsg.icmp_code = 0;
		icmpmsg.icmp_id = (u_short)GetCurrentProcessId();
		icmpmsg.icmp_seq = ttl;
		icmpmsg.icmp_cksum = checksum((u_short *)&icmpmsg, sizeof(icmpmsg));

		// ���� ��û ICMP �޽��� ������
		retval = sendto(sock, (char *)&icmpmsg, sizeof(icmpmsg), 0,
			(SOCKADDR *)&destaddr, sizeof(destaddr));
		if(retval == SOCKET_ERROR){
			if(WSAGetLastError() == WSAETIMEDOUT){
				printf("[����] Send timed out!\n");
				continue;
			}
			err_display("sendto()");
			break;
		}

		// ICMP �޽��� �ޱ�
		addrlen = sizeof(peeraddr);
		retval = recvfrom(sock, buf, BUFSIZE, 0,
			(SOCKADDR *)&peeraddr, &addrlen);
		if(retval == SOCKET_ERROR){
			if(WSAGetLastError() == WSAETIMEDOUT){
				printf("[����] Request timed out!\n");
				continue;
			}
			err_display("recvfrom()");
			break;
		}

		// ICMP �޽��� �м�
		if(!DecodeICMPMessage(buf, retval, &peeraddr, &destaddr))
			break;
	}

	// closesocket()
	closesocket(sock);

	// ���� ����
	WSACleanup();
	return 0;
}

// ICMP �޽��� �м� �Լ�
BOOL DecodeICMPMessage(char *buf, int len, SOCKADDR_IN *from, SOCKADDR_IN *to)
{
    IPHEADER *iphdr = (IPHEADER *)buf;
    int iphdrlen = iphdr->ip_hl * 4;
    ICMPMESSAGE *icmpmsg = (ICMPMESSAGE *)(buf + iphdrlen);

	// ���� üũ
	if((len - iphdrlen) < 8){
		printf("[����] ICMP packet is too short!\n");
		return FALSE;
	}

	switch(icmpmsg->icmp_type){
	case ICMP_TIMEOUT:
	    printf("%s\n", inet_ntoa(from->sin_addr));
		return TRUE;
	case ICMP_ECHOREPLY:
	    printf("%s\n", inet_ntoa(from->sin_addr));
		return FALSE;
	case ICMP_DESTUNREACH:
		printf("[����] Host unreachable!\n");
		return FALSE;
	default:
		printf("[����] Unknown icmp packet!\n");
		return TRUE;
	}

    return TRUE;
}

// ������ �̸� -> IPv4 �ּ� ��ȯ �Լ�
BOOL GetIPAddr(char *name, IN_ADDR *addr)
{
	HOSTENT *ptr = gethostbyname(name);
	if(ptr == NULL){
		err_display("gethostbyname()");
		return FALSE;
	}
	if(ptr->h_addrtype != AF_INET)
		return FALSE;
	memcpy(addr, ptr->h_addr, ptr->h_length);
	return TRUE;
}

// üũ�� ��� �Լ�
u_short checksum(u_short *buf, int len)
{
	u_long cksum = 0;
	u_short *ptr = buf;
	int left = len;

	while(left > 1){
		cksum += *ptr++;
		left -= sizeof(u_short);
	}

	if(left == 1)
		cksum += *(u_char *)buf;

	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
	return (u_short)(~cksum);
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