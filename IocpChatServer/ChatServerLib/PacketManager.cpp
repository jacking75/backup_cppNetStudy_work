#include <utility>
#include <cstring>

#include "Packet.h"
#include "PacketManager.h"


using namespace Common;

namespace ChatServerLib
{
	void PacketManager::Init(UserManager* pUserManager, RoomManager* pRoomManager) 
	{
		m_RecvFuntionDictionary = std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

		m_RecvFuntionDictionary[(int)Common::PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
		m_RecvFuntionDictionary[(int)PACKET_ID::ROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
		m_RecvFuntionDictionary[(int)PACKET_ID::ROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
		m_RecvFuntionDictionary[(int)PACKET_ID::ROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;
				
		m_pUserManager = pUserManager;
		m_pRoomManager = pRoomManager;
	}



	void PacketManager::ProcessRecvPacket(INT32 connectionIndex, char* pBuf, INT16 copySize) 
	{
		PACKET_HEADER* pHeader = reinterpret_cast<PACKET_HEADER*>(pBuf);

		auto iter = m_RecvFuntionDictionary.find(pHeader->PacketId);
		if (iter != m_RecvFuntionDictionary.end())
		{
			(this->*(iter->second))(connectionIndex, pBuf, copySize);
		}

	}
		
	void PacketManager::ProcessLogin(const INT32 connIndex, char* pBuf, INT16 copySize) 
	{ 
		if (LOGIN_REQUEST_PACKET_SZIE != copySize)
		{
			return;
		}

		auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pBuf);

		char* userID = pLoginReqPacket->UserID;
		printf("requested user id = %s\n", userID);

		LOGIN_RESPONSE_PACKET loginResPacket;
		loginResPacket.PacketId = (UINT16)PACKET_ID::LOGIN_RESPONSE;
		loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

		if (m_pUserManager->GetCurrentUserCnt() >= m_pUserManager->GetMaxUserCnt()) 
		{ 
			//접속자수가 최대수를 차지해서 접속불가
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
			SendPacketFunc(connIndex, &loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));
			return;
		}

		//여기에서 이미 접속된 유저인지 확인하고, 접속된 유저라면 실패한다.
		if (m_pUserManager->FindUserByID(userID) == -1) 
		{ 
			//접속중이 아닌 유저라면
			//유저를 관리하는 객체에 넣는다.
			m_pUserManager->AddUser(userID, connIndex);
			loginResPacket.Result = (UINT16)ERROR_CODE::NONE;
		}
		else 
		{
			//접속중인 유저여서 실패를 반환한다.
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_ALREADY;
			SendPacketFunc(connIndex, &loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));
			return;
		}

		SendPacketFunc(connIndex, &loginResPacket, sizeof(LOGIN_RESPONSE_PACKET));
		//스택할당을 하면 위의 함수에서 RingBuffer에 공간을 복사함. 따라서 동적할당처럼 관리할 필요가 없어짐

		//해당 클라이언트와 연결된 세션에 로그인 정보를 저장한다.

	}



	void PacketManager::ProcessEnterRoom(INT32 connIndex, char* pBuf, INT16 copySize) 
	{
		UNREFERENCED_PARAMETER(copySize);

		auto pRoomEnterReqPacket = reinterpret_cast<ROOM_ENTER_REQUEST_PACKET*>(pBuf);
		auto reqUser = m_pUserManager->GetUserByConnIdx(connIndex);

		if (!reqUser || reqUser == nullptr) 
		{
			return;
		}

		auto roomNum = pRoomEnterReqPacket->RoomIndex;
		auto enterRoom = m_pRoomManager->GetRoomByIndex(roomNum);

		if (enterRoom == nullptr) 
		{
			return;
		}


		ROOM_ENTER_RESPONSE_PACKET roomEnterResPacket;
		roomEnterResPacket.PacketId = (UINT16)PACKET_ID::ROOM_ENTER_RESPONSE;
		roomEnterResPacket.PacketLength = sizeof(ROOM_ENTER_RESPONSE_PACKET);

		/*if ( reqUser->GetUserId().compare( enterRoom->GetAuthUser(0) ) == 0 ) 
		{
			strncpy_s(roomEnterResPacket.RivalUserID ,enterRoom->GetAuthUser(1).c_str(), MAX_USER_ID_LEN);
		}
		else if (reqUser->GetUserId().compare(enterRoom->GetAuthUser(1)) == 0) 
		{
			strncpy_s(roomEnterResPacket.RivalUserID, enterRoom->GetAuthUser(0).c_str(), MAX_USER_ID_LEN);
		}
		else 
		{
			(UINT16)ERROR_CODE::ENTER_ROOM_NOT_FINDE_USER;
			SendPacketFunc(connIndex, &roomEnterResPacket, sizeof(ROOM_ENTER_RESPONSE_PACKET));
			return;
		}*/



		if (m_pRoomManager != nullptr) 
		{
			roomEnterResPacket.Result = m_pRoomManager->EnterUser(pRoomEnterReqPacket->RoomIndex, reqUser);
		}
		else 
		{
			roomEnterResPacket.Result = (UINT16)ERROR_CODE::USER_MGR_INVALID_USER_INDEX;
		}

		SendPacketFunc(connIndex, &roomEnterResPacket, sizeof(ROOM_ENTER_RESPONSE_PACKET));
		printf("Response Packet Sended");

	}


	void PacketManager::ProcessLeaveRoom(INT32 connIndex, char* pBuf, INT16 copySize) 
	{
		UNREFERENCED_PARAMETER(pBuf);
		UNREFERENCED_PARAMETER(copySize);

		ROOM_LEAVE_RESPONSE_PACKET roomLeaveResPacket;
		roomLeaveResPacket.PacketId = (UINT16)PACKET_ID::ROOM_LEAVE_RESPONSE;
		roomLeaveResPacket.PacketLength = sizeof(ROOM_LEAVE_RESPONSE_PACKET);

		auto reqUser = m_pUserManager->GetUserByConnIdx(connIndex);
		auto roomNum = reqUser->GetCurrentRoom();

		//TODO 오류
		if (roomNum < 0 || roomNum > (INT32)m_pRoomManager->GetMaxRoomCount()) 
		{
			//유저가 ChatRoom에 들어가있지 않거나 값이 잘못된 경우
			roomLeaveResPacket.Result = (UINT16)ERROR_CODE::CHAT_ROOM_INVALID_ROOM_INDEX;
			return;
		}

		//TODO Room안의 UserList객체의 값 확인하기
		m_pRoomManager->LeaveUser(roomNum, reqUser);
		roomLeaveResPacket.Result = (UINT16)ERROR_CODE::NONE;
		SendPacketFunc(connIndex, &roomLeaveResPacket, sizeof(ROOM_LEAVE_RESPONSE_PACKET));

	}


	void PacketManager::ProcessRoomChatMessage(INT32 connIndex, char* pBuf, INT16 copySize) 
	{
		UNREFERENCED_PARAMETER(copySize);

		auto pRoomChatReqPacketet = reinterpret_cast<ROOM_CHAT_REQUEST_PACKET*>(pBuf);
		
		ROOM_CHAT_RESPONSE_PACKET roomChatResPacket;
		roomChatResPacket.PacketId = (UINT16)PACKET_ID::ROOM_CHAT_RESPONSE;
		roomChatResPacket.PacketLength = sizeof(ROOM_CHAT_RESPONSE_PACKET);

		auto reqUser = m_pUserManager->GetUserByConnIdx(connIndex);
		auto roomNum = reqUser->GetCurrentRoom();

		//TODO 오류
		if (roomNum < 0 || roomNum >(INT32)m_pRoomManager->GetMaxRoomCount())
		{
			//유저가 ChatRoom에 들어가있지 않거나 값이 잘못된 경우
			roomChatResPacket.Result = (UINT16)ERROR_CODE::CHAT_ROOM_INVALID_ROOM_INDEX;
			return;
		}

		//TODO Room안의 UserList객체의 값 확인하기
		auto chatRoom = m_pRoomManager->GetRoomByIndex(roomNum);

		chatRoom->NotifyChat(connIndex, reqUser->GetUserId().c_str(), pRoomChatReqPacketet->Message);
		roomChatResPacket.Result = (INT16)ERROR_CODE::NONE;
		
		//TODO NofifyChat도 오류값을 분류하면 그에 따른 ERROR_CODE값을 응답패킷의 Result로 보낸다.
		SendPacketFunc(connIndex, &roomChatResPacket, sizeof(ROOM_CHAT_RESPONSE_PACKET));

	}		   

	void PacketManager::ClearConnectionInfo(INT32 connIndex) 
	{
		auto reqUser = m_pUserManager->GetUserByConnIdx(connIndex);

		if (reqUser->GetDomainState() == User::DOMAIN_STATE::ROOM) 
		{
			auto roomNum = reqUser->GetCurrentRoom();
			m_pRoomManager->LeaveUser(roomNum, reqUser);
		}

		if (reqUser->GetDomainState() != User::DOMAIN_STATE::NONE) 
		{
			m_pUserManager->DeleteUserInfo(reqUser);
		}
	}




}