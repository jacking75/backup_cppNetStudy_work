#include <windows.h>
#include <stdio.h>

// ������ ���ν���
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char *fmt, ...);

HINSTANCE hInst; // �ν��Ͻ� �ڵ�
HWND hEdit; // ���� ��Ʈ��

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
	hInst = hInstance;

	// ������ Ŭ���� ���
	WNDCLASS wndclass;
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = "MyWndClass";
	if(!RegisterClass(&wndclass)) return 1;

	// ������ ����
	HWND hWnd = CreateWindow("MyWndClass", "WinApp", WS_OVERLAPPEDWINDOW,
		0, 0, 600, 200, NULL, NULL, hInstance, NULL);
	if(hWnd == NULL) return 1;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// �޽��� ����
	MSG msg;
	while(GetMessage(&msg, 0, 0, 0) > 0){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

// ������ ���ν���
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
	case WM_CREATE:
		hEdit = CreateWindow("edit", NULL, 
			WS_CHILD | WS_VISIBLE | WS_HSCROLL | 
			WS_VSCROLL | ES_AUTOHSCROLL | 
			ES_AUTOVSCROLL | ES_MULTILINE,
			0, 0, 0, 0, hWnd, (HMENU)100, hInst, NULL);
		DisplayText("������ GUI ���� ���α׷��Դϴ�.\r\n");
		DisplayText("�ν��Ͻ� �ڵ鰪�� %#x�Դϴ�.\r\n", hInst);
		return 0;
	case WM_SIZE:
		MoveWindow(hEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		return 0;
	case WM_SETFOCUS:
		SetFocus(hEdit);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	
	char cbuf[256];
	vsprintf(cbuf, fmt, arg);
	
	int nLength = GetWindowTextLength(hEdit);
	SendMessage(hEdit, EM_SETSEL, nLength, nLength);
	SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
	
	va_end(arg);
}