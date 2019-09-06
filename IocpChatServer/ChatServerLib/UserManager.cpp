#include <utility>

#include "UserManager.h"


namespace ChatServerLib
{
	//TODO 최흥배 크기가 작으므로 .cpp의 함수 구현을 .h 파일로 이동한다
	void UserManager::Init() {

		UserObjPool = std::vector<User*>(MAX_USER_CNT);

		for (int i = 0; i < MAX_USER_CNT; i++) {
			UserObjPool[i] = new User();
			UserObjPool[i]->Init(i);
		}


	}

	//TODO 일단 void형으로 하다가 여러 ErrorCode가 필요하다면 이를 담당하는 Enum을 만들기
	//TODO 최흥배 유저 중복 조사하기
	int UserManager::AddUser(char* userID, int conn_idx)
	{
		int user_idx = conn_idx;

		UserObjPool[user_idx]->SetLogin(conn_idx, userID);
		UserDictionary.insert(std::pair< char*, int>(userID, conn_idx));

		return 0;
	}

	//TODO 최흥배 코드 일관성 필요 
	//    {
	//} 는 아래처럼 바꾼다
	// {
	// }
	int UserManager::FindUserByID(char* userID) {

		std::unordered_map<std::string, int>::iterator res;
		res = UserDictionary.find(userID);
		if (res == UserDictionary.end()) {
			return -1;
		}
		else {
			return (*res).second;
		}
	}

	void UserManager::DeleteUserInfo(User* deleteUser) {
		UserDictionary.erase(deleteUser->GetUserId());
		deleteUser->Clear();

	}

	User* UserManager::GetUserByConnIdx(INT32 conn_idx)
	{
		return UserObjPool[conn_idx];
	}
}