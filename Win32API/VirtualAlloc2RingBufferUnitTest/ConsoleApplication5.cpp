#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib,"mincore.lib")

void*
CreateRingBuffer(
	unsigned int bufferSize,
	_Outptr_ void** secondaryView
)
{
	BOOL result;
	HANDLE section = nullptr;
	SYSTEM_INFO sysInfo;
	void* ringBuffer = nullptr;
	void* placeholder1 = nullptr;
	void* placeholder2 = nullptr;
	void* view1 = nullptr;
	void* view2 = nullptr;

	GetSystemInfo(&sysInfo);

	if ((bufferSize % sysInfo.dwAllocationGranularity) != 0) {
		return nullptr;
	}

	//
	// Reserve a placeholder region where the buffer will be mapped.
	//

	placeholder1 = (PCHAR)VirtualAlloc2(
		nullptr,
		nullptr,
		2 * bufferSize,
		MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
		PAGE_NOACCESS,
		nullptr, 0
	);

	if (placeholder1 == nullptr) {
		printf("VirtualAlloc2 failed, error %#x\n", GetLastError());
		goto Exit;
	}

	//
	// Split the placeholder region into two regions of equal size.
	//

	result = VirtualFree(
		placeholder1,
		bufferSize,
		MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER
	);

	if (result == FALSE) {
		printf("VirtualFreeEx failed, error %#x\n", GetLastError());
		goto Exit;
	}

	placeholder2 = (void*)((ULONG_PTR)placeholder1 + bufferSize);

	//
	// Create a pagefile-backed section for the buffer.
	//

	section = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		nullptr,
		PAGE_READWRITE,
		0,
		bufferSize, nullptr
	);

	if (section == nullptr) {
		printf("CreateFileMapping failed, error %#x\n", GetLastError());
		goto Exit;
	}

	//
	// Map the section into the first placeholder region.
	//

	view1 = MapViewOfFile3(
		section,
		nullptr,
		placeholder1,
		0,
		bufferSize,
		MEM_REPLACE_PLACEHOLDER,
		PAGE_READWRITE,
		nullptr, 0
	);

	if (view1 == nullptr) {
		printf("MapViewOfFile3 failed, error %#x\n", GetLastError());
		goto Exit;
	}

	//
	// Ownership transferred, don’t free this now.
	//

	placeholder1 = nullptr;

	//
	// Map the section into the second placeholder region.
	//

	view2 = MapViewOfFile3(
		section,
		nullptr,
		placeholder2,
		0,
		bufferSize,
		MEM_REPLACE_PLACEHOLDER,
		PAGE_READWRITE,
		nullptr, 0
	);

	if (view2 == nullptr) {
		printf("MapViewOfFile3 failed, error %#x\n", GetLastError());
		goto Exit;
	}

	//
	// Success, return both mapped views to the caller.
	//

	ringBuffer = view1;
	*secondaryView = view2;

	placeholder2 = nullptr;
	view1 = nullptr;
	view2 = nullptr;

Exit:

	if (section != nullptr) {
		CloseHandle(section);
	}

	if (placeholder1 != nullptr) {
		VirtualFree(placeholder1, 0, MEM_RELEASE);
	}

	if (placeholder2 != nullptr) {
		VirtualFree(placeholder2, 0, MEM_RELEASE);
	}

	if (view1 != nullptr) {
		UnmapViewOfFileEx(view1, 0);
	}

	if (view2 != nullptr) {
		UnmapViewOfFileEx(view2, 0);
	}

	return ringBuffer;
}

int main()
{
	char* ringBuffer;
	void* secondaryView;
	unsigned int bufferSize = 0x10000;

	ringBuffer = (char*)CreateRingBuffer(bufferSize, &secondaryView);

	if (ringBuffer == nullptr) {
		printf("CreateRingBuffer failed\n");
		return 0;
	}

	//
	// Make sure the buffer wraps properly.
	//

	ringBuffer[0] = 'a';

	if (ringBuffer[bufferSize] == 'a') {
		printf("The buffer wraps as expected\n");
	}

	UnmapViewOfFile(ringBuffer);
	UnmapViewOfFile(secondaryView);

	return 0;
}

