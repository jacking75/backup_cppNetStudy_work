//#include "RoomManager.h"
//
//
//namespace ChatServerLib
//{
//	void RoomManager::Init() 
//	{
//		m_RoomList = std::vector<Room*>(MAX_ROOM_COUNT);
//
//		for (int i = 0; i < MAX_ROOM_COUNT; i++) 
//		{
//			m_RoomList[i] = new Room();
//			m_RoomList[i]->SendPacketFunc = SendPacketFunc;
//			m_RoomList[i]->Init(i);
//		}
//
//	}
//
//	UINT16 RoomManager::EnterUser(int RoomIdx, User* pUser) 
//	{
//		if (RoomIdx <0 || RoomIdx > MAX_ROOM_COUNT || m_RoomList[RoomIdx] == nullptr) 
//		{
//			return (UINT16)Common::ERROR_CODE::ROOM_INVALID_INDEX;
//		}
//		
//		return m_RoomList[RoomIdx]->EnterUser(pUser);
//	}
//
//	UINT16 RoomManager::LeaveUser(int RoomIdx, User* pUser) 
//	{
//		if (RoomIdx <0 || RoomIdx >MAX_ROOM_COUNT || m_RoomList[RoomIdx] == nullptr) 
//		{
//			return (UINT16)Common::ERROR_CODE::ROOM_INVALID_INDEX;
//		}
//
//		return m_RoomList[RoomIdx]->LeaveUser(pUser);
//	}
//
//	
//}