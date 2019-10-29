# Win32 API
Windows Vista부터 새로 생긴 API에 대해서 정리한다.
  

## VirtualAlloc2
[MS Docs](https://docs.microsoft.com/ko-kr/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2)  
지정된 프로세스의 가상 주소 공간 내에서 메모리 영역의 상태를 예약, 커밋 또는 변경합니다. 이 함수는 할당 한 메모리를 0으로 초기화합니다.  
  
```
PVOID VirtualAlloc2(
  HANDLE                 Process,
  PVOID                  BaseAddress,
  SIZE_T                 Size,
  ULONG                  AllocationType,
  ULONG                  PageProtection,
  MEM_EXTENDED_PARAMETER *ExtendedParameters,
  ULONG                  ParameterCount
);
```  
  
### 특징
이 API는 가상 주소 공간 관리와 관련된 특정 요구 사항이 있는 고성능 게임 및 서버 응용 프로그램을 지원한다. 예를 들어, 이전에 예약된 영역 위에 메모리를 매핑한다. **링 버퍼를 구현하는데 유용하다**. 특정 정렬로 메모리를 할당한다. 예를 들어, 응용 프로그램이 필요에 따라 대규모/대형 페이지 매핑 영역을 커밋 할 수 있다.  
  
VirtualAlloc2의 기능은 아래와 같은 작업을 수행 할 수 있다 :
- 예약된 페이지 영역을 커밋
- free 페이지 지역 예약
- free 페이지 영역을 동시에 예약 및 커밋
  
VirtualAlloc2는 예약된 페이지를 예약 할 수 없다. 이미 커밋된 페이지를 커밋 할 수 있다. 즉, 이미 커밋 되었는지 여부에 관계 없이 다양한 페이지를 커밋 할 수 있으며 기능이 실패하지 않는다.  
  
VirtualAlloc2를 사용하여 페이지 블록을 예약한 다음 VirtualAlloc2를 추가 호출하여 예약된 블록에서 개별 페이지를 커밋 할 수 있다. 이를 통해 프로세스는 필요할 때까지 물리적 스토리지를 사용하지 않고도 가상 주소 공간 범위를 예약 할 수 있다.  
  
만약 lpAddress의 매개 변수가 NULL이 아니라면 함수는 lpAddress를 사용하고, dwSize 파라미터를 페이지의 영역을 할당하는데 계산한다. 전체 페이지 범위의 현재 상태는 flAllocationType 매개 변수로 지정된 할당 유형과 호환 되어야 한다. 그렇지 않으면, 기능이 실패하고 페이지가 할당되지 않는다. 이 호환성 요구 사항으로 이미 커밋된 페이지를 커밋 할 수 없다.    
  
동적으로 생성된 코드를 실행하려면 VirtualAlloc2를 사용하여 메모리를 할당하고,VirtualProtectEx 함수를 사용하여 PAGE_EXECUTE 액세스 권한을 부여한다.  
   
이 API의 설명 문서를 보면 아래의 예제 코드가 있다.  
- 동일한 공유 메모리 섹션의 인접한 두 보기를 맵핑한 순환 버퍼 
    - 이 예제의 테스트 코드는 `VirtualAlloc2RingBufferUnitTest`에 있다.
	- 할당 크기의 2배 이내까지만 접근 가능하다. 즉 1024로 할당한 경우 1030 위치로 접근하면 6번째 위치에 접근한다. 그러나 2048로 하면 메모리를 벗어난 것으로 된다.
- 메모리 할당 시 선호하는 NUMA 노드를 지정
- 특정 가상 주소 범위(이 예에서는 4GB 미만)와 특정 정렬로 메모리를 할당
  