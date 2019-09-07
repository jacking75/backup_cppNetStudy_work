#pragma once
#include <string>

#include "Packet.h"



namespace ChatServerLib
{
	class User
	{
	public:
		enum class DOMAIN_STATE 
		{
			NONE = 0,
			LOGIN = 1,
			ROOM = 2
		};


		User() = default;
		~User() = default;

		void Init(const INT32 index);

		int SetLogin(INT32 conn_idx, char* login_id);
		
		void EnterRoom(INT32 roomIndex);
		
		void Clear();
		
		void SetDomainState(DOMAIN_STATE val);

		INT32 GetCurrentRoom() 
		{
			return m_RoomIndex;
		}

		INT32 GetNetConnIdx() 
		{
			return m_NetConnIdx;
		}

		std::string GetUserId() 
		{
			return  m_UserId;
		}

		DOMAIN_STATE GetDomainState() {
			return m_CurDomainState;
		}
				

	private:
		//TODO type을 window에서 정의된것으로 (INT16 INT32 INT64) 이 형식이 없으면 header에 window.h
		//이것은 cross 플랫폼이 아니라 windows에서만 사용하기에 쓰는 type. 만일 크로스플랫폼을 쓴다면 c++에서 정의된 타입을 씀

		INT32 m_UserIndex = -1;
		INT32 m_SessionIndex = -1;
		INT32 m_RoomIndex = -1;

		std::string m_UserId;
		bool m_IsConfirm = false;
		std::string AuthToken;

		INT32 m_NetConnIdx = -1;

		DOMAIN_STATE m_CurDomainState = DOMAIN_STATE::NONE;		
	};

}