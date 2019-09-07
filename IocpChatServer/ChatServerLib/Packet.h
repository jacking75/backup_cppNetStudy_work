#pragma once
#include <queue>

#include "Define.h"
//#include "../../IocpNetLib/IOCPServerNet.h" //TODO 필요한 헤더파일만 포함하기


namespace Common 
{

	// 100 ~ 1000 사이의 번호만 사용 가능!
	enum class  PACKET_ID : UINT16 
	{
		
		/// ChatClient용 패킷 ID

		LOGIN_REQUEST = 201,
		LOGIN_RESPONSE = 202,
				
		ROOM_ENTER_REQUEST = 206,
		ROOM_ENTER_RESPONSE = 207,

		ROOM_LEAVE_REQUEST = 209,
		ROOM_LEAVE_RESPONSE = 210,

		ROOM_CHAT_REQUEST = 214,
		ROOM_CHAT_RESPONSE = 215,
		ROOM_CHAT_NOTIFY = 216,

		GAME_START_REQUEST = 301,
		GAME_START_RESPONSE = 302,
		GAME_START_NOTIFY = 303,

		GAME_SYNC_REQUEST = 304,
		GAME_SYNC_NOTIFY = 305,

		GAME_END_REQUEST = 311,
		GAME_END_RESPONSE = 312,
		GAME_END_NOTIFY = 313,

		NTF_MATCHING_RESULT = 401,
	};

	enum class ERROR_CODE : UINT16
	{
		NONE = 0,

		USER_MGR_INVALID_USER_INDEX = 11,
		USER_MGR_INVALID_USER_UNIQUEID = 12,

		LOGIN_USER_ALREADY = 31,
		LOGIN_USER_USED_ALL_OBJ = 32,

		NEW_ROOM_USED_ALL_OBJ = 41,
		NEW_ROOM_FAIL_ENTER = 42,

		ENTER_ROOM_NOT_FINDE_USER = 51,
		ENTER_ROOM_INVALID_USER_STATUS = 52,
		ENTER_ROOM_NOT_USED_STATUS = 53,
		ENTER_ROOM_FULL_USER = 54,

		ROOM_INVALID_INDEX = 61,
		ROOM_NOT_USED = 62,
		ROOM_TOO_MANY_PACKET = 63,

		LEAVE_ROOM_INVALID_ROOM_INDEX = 71,

		CHAT_ROOM_INVALID_ROOM_INDEX = 81,
	};

	


#pragma pack(push,1)
	struct PACKET_HEADER
	{
		UINT16 PacketLength;
		UINT16 PacketId;
		UINT8 Type; //압축여부 암호화여부 등 속성을 알아내는 값
	};

	//- 로그인 요청
	const int MAX_USER_ID_LEN = 20;
	const int MAX_USER_PW_LEN = 20;

	struct LOGIN_REQUEST_PACKET : public PACKET_HEADER 
	{
		char UserID[MAX_USER_ID_LEN+1];
		char UserPW[MAX_USER_PW_LEN+1];
	};
	const size_t LOGIN_REQUEST_PACKET_SZIE = sizeof(LOGIN_REQUEST_PACKET);


	struct LOGIN_RESPONSE_PACKET : public PACKET_HEADER 
	{
		UINT16 Result;
	};



	//- 룸에 들어가기 요청
	const int MAX_ROOM_TITLE_SIZE = 16;
	struct ROOM_ENTER_REQUEST_PACKET : public PACKET_HEADER
	{
		INT32 RoomIndex;
	};

	struct ROOM_ENTER_RESPONSE_PACKET : public PACKET_HEADER 
	{
		UINT16 Result;
		char RivalUserID[MAX_USER_ID_LEN + 1] = {0, };
	};


	//- 룸 나가기 요청
	struct ROOM_LEAVE_REQUEST_PACKET : public PACKET_HEADER 
	{

	};

	struct ROOM_LEAVE_RESPONSE_PACKET : public PACKET_HEADER 
	{
		UINT16 Result;
	};



	// 룸 채팅
	const int MAX_CHAT_MSG_SIZE = 256;
	struct ROOM_CHAT_REQUEST_PACKET : public PACKET_HEADER
	{
		char Message[MAX_CHAT_MSG_SIZE +1] = { 0, };
	};

	struct ROOM_CHAT_RESPONSE_PACKET : public PACKET_HEADER
	{
		UINT16 Result;
	};

	struct ROOM_CHAT_NOTIFY_PACKET : public PACKET_HEADER
	{
		char UserID[MAX_USER_ID_LEN + 1] = { 0, };
		char Msg[MAX_CHAT_MSG_SIZE + 1] = { 0, };
	};
#pragma pack(pop) //위에 설정된 패킹설정이 사라짐

}