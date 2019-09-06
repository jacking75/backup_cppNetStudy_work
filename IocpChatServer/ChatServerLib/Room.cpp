#include "Room.h"
#include "Packet.h"
#define MAX_USER_ID_LEN  20

namespace ChatServerLib
{
	void Room::Init(UINT16 roomNum) 
	{
		m_RoomNum = roomNum;
		m_UserList = std::vector<User*>();
		m_AuthUserList[0] = "";
		m_AuthUserList[1] = "";
	}

	UINT16 Room::EnterUser(User* pUser) 
	{
		if (m_CurrentUserCount >= MAX_USER_COUNT) 
		{
			return (UINT16)Common::ERROR_CODE::ENTER_ROOM_FULL_USER;
		}

		if (pUser->GetUserId().compare(m_AuthUserList[0]) != 0 && pUser->GetUserId().compare(m_AuthUserList[1]) != 0) 
		{
			return (UINT16)Common::ERROR_CODE::ENTER_ROOM_NOT_FINDE_USER;
		}

		m_UserList.push_back(pUser);
		m_CurrentUserCount++;

		pUser->EnterRoom(m_RoomNum);
		return (UINT16)Common::ERROR_CODE::NONE;
	}


	UINT16 Room::LeaveUser(User* pUser) 
	{
		bool find = false;

		for (auto itr = begin(m_UserList); itr != end(m_UserList);) 
		{
			if ((*itr)->GetUserId() == pUser->GetUserId()) {
				itr = m_UserList.erase(itr);
				m_CurrentUserCount--;
			}
			else
			{
				itr++;
			}
		}

		if (find == false) 
		{
			return (UINT16)Common::ERROR_CODE::ROOM_NOT_USED;
		}

		if (pUser->GetUserId().compare(m_AuthUserList[0]) == 0) 
		{
			m_AuthUserList[0] = '\0';
		}

		if (pUser->GetUserId().compare(m_AuthUserList[1]) == 0) 
		{
			m_AuthUserList[1] = '\0';
		}

		pUser->SetDomainState(User::DOMAIN_STATE::LOGIN);
		return (UINT16)Common::ERROR_CODE::NONE;

	}


	void Room::SetAuthUserList(char* UserID1, char* UserID2)
	{
		m_AuthUserList[0] = UserID1;
		m_AuthUserList[1] = UserID2;
	}


	void Room::NotifyChat(INT32 connIndex, const char* UserID, const char* Msg) 
	{
		Common::ROOM_CHAT_NOTIFY_PACKET roomChatNtfyPkt;
		roomChatNtfyPkt.PacketId = (UINT16)Common::PACKET_ID::ROOM_CHAT_NOTIFY;
		roomChatNtfyPkt.PacketLength = sizeof(roomChatNtfyPkt);

		CopyMemory(roomChatNtfyPkt.Msg, Msg, sizeof(roomChatNtfyPkt.Msg));
		CopyMemory(roomChatNtfyPkt.UserID, UserID, sizeof(roomChatNtfyPkt.UserID));
		SendToAllUser((UINT16)Common::PACKET_ID::ROOM_CHAT_NOTIFY, sizeof(roomChatNtfyPkt), &roomChatNtfyPkt, connIndex, false);
	}


	void Room::SendToAllUser(const UINT16 packetID, const UINT16 dataSize, void* pData, const UINT32 passUserindex, bool exceptMe) 
	{

		for (auto pUser : m_UserList)
		{
			if (pUser == nullptr) {
				continue;
			}

			if (exceptMe == true && pUser->GetNetConnIdx() == passUserindex) {
				continue;
			}
						
			SendPacketFunc(pUser->GetNetConnIdx(), pData, dataSize);
		}
	}
}