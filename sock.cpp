//to compile use -> "gcc sock.cpp -mwindows -lWs2_32 -m32 -w"

#define NDEBUGS

//For Visual C Compiler
#pragma comment(lib, "AdvApi32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <ctype.h>

#ifdef NDEBUG

	#include <stdio.h>
	#include <stdarg.h>

	void out(const char* format, ...) {

		va_list args;

		va_start(args, format);

		vprintf(format, args);

		va_end(args);


	}

#else

	void out(const char* format, ...) {
		//continue;
	}

#endif // NDEBUG

#define CONNECT 1
#define SEND 2
#define EXIT 3
#define CLEAR 4

#define DEFAULT_LENGTH 500
#define DEFAULT_PORT "30001"
#define DEFAULT_SERVER "52.20.16.20"

//unused
typedef struct data{

	SOCKET* sock;

	char* recvBuffer;

	void (*function) (void);

} DATA;

bool running = false;

HWND btn, hMain, hImg, hCom, hEdit, hQuit, hPort, hip, hSend, hClear;

HANDLE hThread;

HINSTANCE hGlobalInst;

ULONG NonBlock = 1;

CRITICAL_SECTION lock;

char sendBuffer[DEFAULT_LENGTH] = {0};

char recvBuffer[DEFAULT_LENGTH] = {0};

//net structs

struct addrinfo* result = NULL;

struct addrinfo hints;

SOCKET Client;

FD_SET ReadSet;

//
void AddControls(HWND);

void LoadImages();

void netClose();

inline void AppendText(char * string, HWND handle);

int netCleanup();

int netStartup(char *, char *);

int netConnect();

int netSend(SOCKET*, const char*, int);

int netReceive(SOCKET*, char *);

HANDLE netReceiveAsync();

DWORD WINAPI  netReceiveAsyncHelper(LPVOID param);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wPara, LPARAM lPara);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int ncmd) {

	const wchar_t * windowClass = L"HARSIMIOT CONNECTOR";

	WNDCLASSW window = { 0 };

	window.hbrBackground = (HBRUSH)COLOR_WINDOW;

	window.hCursor = LoadCursor(NULL, IDC_CROSS);

	window.hInstance = hInst;

	hGlobalInst = hInst;

	window.lpszClassName = windowClass;

	window.lpfnWndProc = WindowProc;

	window.hIcon = (HICON)LoadImage(NULL, "mc.ico", IMAGE_ICON, 48, 48, LR_LOADFROMFILE);

	InitializeCriticalSection(&lock);

	if (!RegisterClassW(&window)) {

		out("Error could not register window class\n");
		return -1;

	}

	hMain = CreateWindowW(windowClass, windowClass, WS_VISIBLE| WS_OVERLAPPED | WS_SYSMENU, 0, 0, 485, 470, NULL, NULL, NULL, NULL);

	MSG msg = { 0 };

	#ifdef NDEBUG

        AllocConsole();

	#endif // NDEBUG

	while (GetMessage(&msg, NULL, 0, 0)) {

		TranslateMessage((&msg));

		DispatchMessage(&msg);

	}

    return 0;

}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wPara, LPARAM lPara) {

	switch (msg) {

	case  WM_CREATE:

		AddControls(hwnd);

		break;

	case WM_DESTROY:

        netClose();

		PostQuitMessage(0);

		break;

	case WM_COMMAND:

		switch (wPara) {

		case CONNECT:

			if (!running) {

                running = true;

				DATA toPass = { &Client, NULL, NULL };

				char address[DEFAULT_LENGTH], port[DEFAULT_LENGTH];

				GetWindowText(hip, address, DEFAULT_LENGTH);

				GetWindowText(hPort, port, DEFAULT_LENGTH);

				if (netStartup(address, port) != 0) out("error\n");

				if (netConnect() != 0)
                {
                    out("Error connecting to server\n");

                    AppendText("Error connecting to server.", hEdit);

                }

				else {

                    AppendText("Successfully connected to server.\n", hEdit);

                    hThread = netReceiveAsync();

                    SetWindowText(btn, "STOP");
                }

			}

			else {

				SetWindowText(btn, "START");

                EnterCriticalSection(&lock);

				running = false;

				LeaveCriticalSection(&lock);

			}

			break;

		case SEND:

            EnterCriticalSection(&lock);

            out ("%d\n", running);

			if (running) {

				int iResult;

				GetWindowText(hCom, sendBuffer, DEFAULT_LENGTH);

				iResult = netSend(&Client, sendBuffer, GetWindowTextLength(hCom));

				out ("sent %d\n", iResult);

			}

			LeaveCriticalSection(&lock);

			break;

		case EXIT:

			if (!running) {

				PostQuitMessage(0);

				break;
			}

			else {

                 netClose();

                 PostQuitMessage(0);
			}

			break;

        case CLEAR:

            SetWindowText(hEdit, "");

            break;

		default:

			break;
		}

		break;

	default:

		DefWindowProc(hwnd, msg, wPara, lPara);

		break;

	}

}

void AddControls(HWND hwnd) {

	LoadLibrary("Msftedit.dll");

	CreateWindowW(L"static", L"IP", WS_CHILD | WS_VISIBLE, 370, 5, 100, 20, hwnd, NULL, NULL, NULL);

	CreateWindowW(L"static", L"PORT", WS_CHILD | WS_VISIBLE, 370, 45, 100, 20, hwnd, NULL, NULL, NULL);

	hQuit = CreateWindowW(L"Button", L"EXIT", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_OVERLAPPED, 10, 415, 100, 20, hwnd, (HMENU) EXIT, NULL, NULL);

	hEdit = CreateWindowExW(0, L"RICHEDIT50W", L"", WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_READONLY | ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_BORDER, 10, 10, 350, 400, hwnd, NULL, NULL, NULL);

	btn = CreateWindowW(L"Button", L"START", WS_VISIBLE | WS_CHILD | SS_CENTER, 250, 415, 100, 20, hwnd, (HMENU) CONNECT, NULL, NULL);

	hip = CreateWindowW(L"edit", L"", WS_CHILD | WS_VISIBLE | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL | WS_BORDER , 370, 20, 100, 20, hwnd, NULL, hGlobalInst, NULL);

	hPort = CreateWindowW(L"edit", L"", WS_CHILD | WS_VISIBLE |  ES_LEFT | WS_BORDER | ES_AUTOHSCROLL, 370, 60, 100, 20, hwnd, NULL, NULL, NULL);

	 CreateWindowW(L"static", L"COMMAND", WS_CHILD | WS_VISIBLE, 370, 85, 100, 20, hwnd, NULL, NULL, NULL);

	hCom = CreateWindowW(L"edit", L"", WS_CHILD | WS_VISIBLE | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL | WS_BORDER, 370, 100, 100, 20, hwnd, NULL, hGlobalInst, NULL);

	hSend = CreateWindowW(L"button", L"SEND", WS_CHILD | WS_VISIBLE, 370, 125, 100, 20, hwnd, (HMENU)SEND, NULL, NULL);

	hClear = CreateWindowW(L"button", L"CLEAR", WS_CHILD | WS_VISIBLE, 370, 150, 100, 20, hwnd, (HMENU)CLEAR, NULL, NULL);

	SetWindowText(hCom, "BOB");

	SetWindowText(hQuit, "EXIT");

	SetWindowText(hip, DEFAULT_SERVER);

	SetWindowText(hPort, DEFAULT_PORT);

}

inline void AppendText(char* string, HWND edit) {

	int  ndx = GetWindowTextLength(edit);

	SendMessage(edit, EM_SETSEL, (WPARAM)ndx, (LPARAM)ndx);

	SendMessage(edit, EM_REPLACESEL, 0, (LPARAM)((LPSTR)(string)));

}

int netStartup(char * address, char * port) {

	WSADATA wsadata = {0};

	int iResult = WSAStartup(0x0202,&wsadata);

	if (iResult != 0) return -1;

	ZeroMemory(&hints, sizeof(*(&hints)));

	hints.ai_family = AF_UNSPEC;

	hints.ai_socktype = SOCK_STREAM;

	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(address, port, &hints, &result);

	Client = INVALID_SOCKET;

	return 0;
}

int netConnect() {

	struct addrinfo* ptr = NULL;

	int i = 0;

	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		if ((Client = socket(ptr->ai_family, ptr->ai_socktype,ptr->ai_protocol)) == INVALID_SOCKET)

		{

			out("Error in netConnect() %ld \n", WSAGetLastError());

			return 1;

		}


		int iResult = connect(Client, ptr->ai_addr, (int)ptr->ai_addrlen);

		if (iResult == SOCKET_ERROR) {

			out("Connection error\n");

			Client = INVALID_SOCKET;

			return 1;

		}

		iResult = ioctlsocket(Client, FIONBIO, &NonBlock);

		if (iResult != 0){

			out ("Problem in ioctl at netConnect %d %d\n", iResult, WSAGetLastError());

			return 1;

		}



		i++;

		break;
	}

	out("Looped %d times\n", i);

	return 0;

}

int netSend(SOCKET* Client, const char* message, int len) {

	int iResult = send(*Client, message, len, 0);

	if (iResult == SOCKET_ERROR) return -1;

	out ("send %d \n", iResult);

	return iResult;

}

int netReceive(SOCKET* client, char* recvBuf) {

	int iResult;

	ZeroMemory(recvBuf, sizeof(recvBuffer));

	do {

		iResult = recv(*client, recvBuf, DEFAULT_LENGTH, 0);

		if (iResult > 0)

			break;

	} while (iResult > 0);

	return iResult;

}

HANDLE netReceiveAsync()
{
    //SOCKET* client, char* recvBuf, void(*CallBack)(void)
	//DATA toPass = { client, recvBuf, CallBack };

	HANDLE h = CreateThread(NULL, 0, netReceiveAsyncHelper, NULL, NULL, NULL);

    return *(&h);
}

DWORD WINAPI  netReceiveAsyncHelper(LPVOID param) {

    TIMEVAL t;

    t.tv_sec = 1;

    t.tv_usec = 0;

    int iResult;

    while (1) {

        EnterCriticalSection(&lock);

        out ("in critical section\n");

        if (!running) {

            LeaveCriticalSection(&lock);

            break;

        }

        LeaveCriticalSection(&lock);

        FD_ZERO(&ReadSet);

        FD_SET(Client, &ReadSet);

        if ((iResult = select(0, &ReadSet, NULL, NULL, &t)) > 0){

            out ("sock select returned %d\n", iResult);

            iResult = netReceive(&Client, recvBuffer);

            if (iResult <= 0) { //connection was closed, we know this because the select call returned > 0

                EnterCriticalSection(&lock);

                running = false;

                LeaveCriticalSection(&lock);

                AppendText("Server disconnected.\n", hEdit);

                break;

            }

            AppendText(recvBuffer, hEdit);

            AppendText("\n", hEdit);

        } else {

            out ("%d ires \n", iResult);

        }

    }

    netCleanup();

    SetWindowText(btn, "START");

    out ("Leaving thread\n");

}

int netCleanup() {

	if (Client != INVALID_SOCKET) {

		closesocket(Client);

	}

	freeaddrinfo(result);

	WSACleanup();

	return 0;

}

void netClose(){

                EnterCriticalSection(&lock);

                running = false;

                LeaveCriticalSection(&lock);

                Sleep(2000);

                DeleteCriticalSection(&lock);

                netCleanup();


}



