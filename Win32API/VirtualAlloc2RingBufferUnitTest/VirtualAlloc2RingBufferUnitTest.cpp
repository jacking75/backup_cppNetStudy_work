#include "pch.h"
#include "RingBuffer.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace VirtualAlloc2RingBufferUnitTest
{
	TEST_CLASS(VirtualAlloc2RingBufferUnitTest)
	{
	public:
		
		TEST_METHOD(Create_success1)
		{
			const UINT32 bufferSize = 65'536;

			RingBuffer rBuffer;
			auto isResult = rBuffer.Create(bufferSize);

			Assert::IsTrue(isResult);
		}

		TEST_METHOD(Create_success2)
		{
			const UINT32 bufferSize = 65'536 * 2;

			RingBuffer rBuffer;
			auto isResult = rBuffer.Create(bufferSize);

			Assert::IsTrue(isResult);
		}

		TEST_METHOD(Create_fail1)
		{
			const UINT32 bufferSize = 1024;

			RingBuffer rBuffer;
			auto isResult = rBuffer.Create(bufferSize);

			Assert::IsTrue(isResult == false);
		}

		TEST_METHOD(Create_fail2)
		{
			const UINT32 bufferSize = 65'536 - 1;

			RingBuffer rBuffer;
			auto isResult = rBuffer.Create(bufferSize);

			Assert::IsTrue(isResult == false);
		}


		TEST_METHOD(WriteRead_1)
		{
			const UINT32 bufferSize = 65'536;
			RingBuffer rBuffer;
			auto isResult = rBuffer.Create(bufferSize);

			const UINT32 tempData1Size = 211;
			char tempData1[tempData1Size] = { 0, };
			gen_random(tempData1, tempData1Size -1);
			
			isResult = rBuffer.Write(tempData1Size, tempData1);
			Assert::IsTrue(isResult);

			auto pReadBuf = rBuffer.Read(tempData1Size);
			Assert::IsTrue(pReadBuf != nullptr);
			
			Assert::AreEqual(pReadBuf, tempData1);
		}

		// 위치가 UINT32 보다 큰 경우도 문제 없나?
		TEST_METHOD(WriteRead_UINT64_Pos)
		{
			const UINT32 bufferSize = 4096;
			RingBuffer rBuffer;
			auto isResult = rBuffer.Create(bufferSize);

			const UINT32 tempData1Size = 211;
			char tempData1[tempData1Size] = { 0, };
			gen_random(tempData1, tempData1Size - 1);

			UINT64 movePos = bufferSize;
			rBuffer.UnitTest_SetWritePos(movePos);
			isResult = rBuffer.Write(tempData1Size, tempData1);
			Assert::IsTrue(isResult);

			movePos = (bufferSize * 2) - tempData1Size;
			rBuffer.UnitTest_SetWritePos(movePos);
			isResult = rBuffer.Write(tempData1Size, tempData1);
			Assert::IsTrue(isResult);

			//rBuffer.UnitTest_SetWritePos(movePos);
			//auto pReadBuf = rBuffer.Read(tempData1Size);
			//Assert::IsTrue(pReadBuf != nullptr);
			
			//Assert::AreEqual(pReadBuf, tempData1);
		}

		// 쓰고-읽고, 쓰고-읽고

		// 2번 쓰고/읽기
		TEST_METHOD(WriteRead_2)
		{
			const UINT32 bufferSize = 65'536;
			RingBuffer rBuffer;
			auto isResult = rBuffer.Create(bufferSize);

			// 쓰기 1
			const UINT32 tempDataSize1 = 35;
			char tempData1[tempDataSize1] = { 0, };
			gen_random(tempData1, tempDataSize1 - 1);

			isResult = rBuffer.Write(tempDataSize1, tempData1);
			Assert::IsTrue(isResult);

			// 쓰기 2
			const UINT32 tempDataSize2 = 57;
			char tempData2[tempDataSize2] = { 0, };
			gen_random(tempData2, tempDataSize2 - 1);

			isResult = rBuffer.Write(tempDataSize2, tempData2);
			Assert::IsTrue(isResult);

			// 읽기
			auto pReadBuf = rBuffer.Read(tempDataSize1);
			Assert::IsTrue(pReadBuf != nullptr);
			Assert::AreEqual(pReadBuf, tempData1);

			pReadBuf = rBuffer.Read(tempDataSize2);
			Assert::IsTrue(pReadBuf != nullptr);
			Assert::AreEqual(pReadBuf, tempData2);
		}

		// 3번 쓰고/읽기. 두 번째 쓸 때는 버퍼 범위 벗어나기
		TEST_METHOD(WriteRead_3)
		{
			const UINT32 bufferSize = 4096;// 65'536;
			RingBuffer rBuffer;
			auto isResult = rBuffer.Create(bufferSize);
			Assert::IsTrue(isResult);

						
			const UINT32 tempDataSize1 = 35;
			char tempData1[tempDataSize1] = { 0, };
			gen_random(tempData1, tempDataSize1 - 1);

			auto movePos = bufferSize - (tempDataSize1 - 10);
			rBuffer.UnitTest_SetWritePos(movePos);
			rBuffer.UnitTest_SetReadPos(movePos);

			// 쓰기 1
			isResult = rBuffer.Write(tempDataSize1, tempData1);
			Assert::IsTrue(isResult);

			// 쓰기 2
			const UINT32 tempDataSize2 = 57;
			char tempData2[tempDataSize2] = { 0, };
			gen_random(tempData2, tempDataSize2 - 1);

			isResult = rBuffer.Write(tempDataSize2, tempData2);
			Assert::IsTrue(isResult);

			// 쓰기 3
			const UINT32 tempDataSize3 = 157;
			char tempData3[tempDataSize3] = { 0, };
			gen_random(tempData3, tempDataSize3 - 1);

			isResult = rBuffer.Write(tempDataSize3, tempData3);
			Assert::IsTrue(isResult);


			// 읽기
			auto pReadBuf = rBuffer.Read(tempDataSize1);
			Assert::IsTrue(pReadBuf != nullptr);
			Assert::AreEqual(pReadBuf, tempData1);

			pReadBuf = rBuffer.Read(tempDataSize2);
			Assert::IsTrue(pReadBuf != nullptr);
			Assert::AreEqual(pReadBuf, tempData2);

			pReadBuf = rBuffer.Read(tempDataSize3);
			Assert::IsTrue(pReadBuf != nullptr);
			Assert::AreEqual(pReadBuf, tempData3);
		}



		//https://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c/440240 
		void gen_random(char* s, const int len) 
		{
			static const char alphanum[] =
				"0123456789"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz";

			for (int i = 0; i < len; ++i) {
				s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
			}

			s[len] = 0;
		}
	};
}
