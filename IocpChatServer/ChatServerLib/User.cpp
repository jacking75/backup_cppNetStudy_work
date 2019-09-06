#include "User.h"


namespace ChatServerLib
{
	//TODO 최흥배 User 클래스의 코드 크기는 작으므로 .cpp와 .h 파일 함치기
	//User의 크기가 작다면 공통으로 사용하는 header에 넣는것이 좋다.


	void User::Init(const int index) {
		m_UserIndex = index;
	}

	int User::SetLogin(int conn_idx, char* login_id) {

		m_NetConnIdx = conn_idx;
		m_CurDomainState = DOMAIN_STATE::LOGIN;
		m_UserId = login_id;

		return 0;
	}

	void User::EnterRoom(int roomIndex) {
		m_RoomIndex = roomIndex;
		m_CurDomainState = DOMAIN_STATE::ROOM;
	}


	void User::Clear() {
		m_UserIndex = -1;
		m_SessionIndex = -1;
		m_RoomIndex = -1;

		m_UserId = "";
		m_IsConfirm = false;

		m_NetConnIdx = -1;

		DOMAIN_STATE m_CurDomainState = DOMAIN_STATE::NONE;
	}

	void User::SetDomainState(DOMAIN_STATE val) {
		m_CurDomainState = val;
	}
}