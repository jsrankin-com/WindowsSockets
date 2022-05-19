#include<stdio.h>
#include<winsock2.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <iostream>
#include <cstdlib>
#include <urlmon.h>
#include <sstream>
#include <atlcomcli.h>
#include <comip.h>
#pragma comment(lib, "ws2_32.lib") //Winsock Library
#include <windows.h>
#include <string.h>
#include <fstream>
#include <iostream>       // std::cout, std::hex
#include <string>         // std::string, std::u32string
#include <locale>         // std::wstring_convert
#include <codecvt>        // std::codecvt_utf8
#include <cstdint>        // std::uint_least32_t


#include <stdio.h>

// Generic-Text Mappings which are Microsoft extensions

// that are not ANSI compatible. However those related

// string/text functions can be 'ported'/migrated easily to ANSI

// http://msdn.microsoft.com/en-us/library/c426s321.aspx

#include <tchar.h>

// http://msdn.microsoft.com/en-us/library/ms647466.aspx

#include <strsafe.h>



// Constants

#define PIPE_TIMEOUT 5000

#define BUFSIZE 4096



// Struct typedef

typedef struct

{

    OVERLAPPED oOverlap;

    HANDLE hPipeInst;

    TCHAR chRequest[BUFSIZE];

    DWORD cbRead;

    TCHAR chReply[BUFSIZE];

    DWORD cbToWrite;

} PIPEINST, * LPPIPEINST;



// Prototypes

VOID DisconnectAndClose(LPPIPEINST);

BOOL CreateAndConnectInstance(LPOVERLAPPED);

BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);

VOID GetAnswerToRequest(LPPIPEINST);

VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);

VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);



// Global var

HANDLE hPipe;



int _tmain(int argc, TCHAR** argv)

{

    HANDLE hConnectEvent;

    OVERLAPPED oConnect;

    LPPIPEINST lpPipeInst;

    DWORD dwWait, cbRet;

    BOOL fSuccess, fPendingIO;



    // Create one event object for the connect operation

    hConnectEvent = CreateEvent(

        NULL,    // default security attribute

        TRUE,    // manual reset event

        TRUE,    // initial state = signaled

        NULL);   // unnamed event object

    if (hConnectEvent == NULL)

    {

        printf("CreateEvent() failed with error code %d\n", GetLastError());

        return 0;

    }

    else

        printf("CreateEvent() is OK!\n");



    oConnect.hEvent = hConnectEvent;

    // Call a subroutine to create one instance, and wait for the client to connect

    fPendingIO = CreateAndConnectInstance(&oConnect);

    while (1)

    {

        // Wait for a client to connect, or for a read or write

        // operation to be completed, which causes a completion

        // routine to be queued for execution

        dwWait = WaitForSingleObjectEx(

            hConnectEvent,  // event object to wait for

            INFINITE,       // waits indefinitely

            TRUE);          // alertable wait enabled

        switch (dwWait)

        {

            // The wait conditions are satisfied by a completed connect operation

        case 0:

            // If an operation is pending, get the result of the connect operation

            if (fPendingIO)

            {

                fSuccess = GetOverlappedResult(

                    hPipe,     // pipe handle

                    &oConnect, // OVERLAPPED structure

                    &cbRet,    // bytes transferred

                    FALSE);    // does not wait

                if (!fSuccess)

                {

                    printf("ConnectNamedPipe() failed with error code %d\n", GetLastError());

                    return 0;

                }

                else

                    printf("ConnectNamedPipe() is OK!\n");

            }

            // Allocate storage for this instance

            lpPipeInst = (LPPIPEINST)GlobalAlloc(GPTR, sizeof(PIPEINST));

            if (lpPipeInst == NULL)

            {

                printf("GlobalAlloc() for buffer failed with error code %d\n", GetLastError());

                return 0;

            }

            else

                printf("GlobalAlloc() for buffer is OK!\n");



            lpPipeInst->hPipeInst = hPipe;

            // Start the read operation for this client

            // Note that this same routine is later used as a

            // completion routine after a write operation

            lpPipeInst->cbToWrite = 0;

            CompletedWriteRoutine(0, 0, (LPOVERLAPPED)lpPipeInst);

            // Create new pipe instance for the next client

            fPendingIO = CreateAndConnectInstance(&oConnect);

            break;

            // The wait is satisfied by a completed read or write

            // operation. This allows the system to execute the completion routine

        case WAIT_IO_COMPLETION:

            break;

            // An error occurred in the wait function

        default:

        {

            printf("WaitForSingleObjectEx() failed with error code %d\n", GetLastError());

            return 0;

        }

        }

    }

    return 0;

}









// CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED)

// This routine is called as a completion routine after writing to

// the pipe, or when a new client has connected to a pipe instance, it starts another read operation

VOID WINAPI CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten, LPOVERLAPPED lpOverLap)

{

    LPPIPEINST lpPipeInst;

    BOOL fRead = FALSE;



    // lpOverlap points to storage for this instance

    lpPipeInst = (LPPIPEINST)lpOverLap;

    // The write operation has finished, so read the next request (if there is no error)

    if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))

        fRead = ReadFileEx(

            lpPipeInst->hPipeInst,

            lpPipeInst->chRequest,

            BUFSIZE * sizeof(TCHAR),

            (LPOVERLAPPED)lpPipeInst,

            (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);

    // Disconnect if an error occurred

    if (!fRead)

        DisconnectAndClose(lpPipeInst);

}



// CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED)

// This routine is called as an I/O completion routine after reading

// a request from the client. It gets data and writes it to the pipe

VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead, LPOVERLAPPED lpOverLap)

{

    LPPIPEINST lpPipeInst;

    BOOL fWrite = FALSE;



    // lpOverlap points to storage for this instance

    lpPipeInst = (LPPIPEINST)lpOverLap;

    // The read operation has finished, so write a response (if no error occurred)

    if ((dwErr == 0) && (cbBytesRead != 0))

    {

        GetAnswerToRequest(lpPipeInst);

        fWrite = WriteFileEx(

            lpPipeInst->hPipeInst,

            lpPipeInst->chReply,

            lpPipeInst->cbToWrite,

            (LPOVERLAPPED)lpPipeInst,

            (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);

    }

    // Disconnect if an error occurred

    if (!fWrite)

        DisconnectAndClose(lpPipeInst);

}



// DisconnectAndClose(LPPIPEINST)

// This routine is called when an error occurs or the client closes its handle to the pipe

VOID DisconnectAndClose(LPPIPEINST lpPipeInst)

{

    // Disconnect the pipe instance

    if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))

    {

        printf("DisconnectNamedPipe() failed with error code %d.\n", GetLastError());

    }

    else

        printf("DisconnectNamedPipe() is fine!\n");



    // Close the handle to the pipe instance

    printf("Closing the handle...\n");

    CloseHandle(lpPipeInst->hPipeInst);

    // Release the storage for the pipe instance

    printf("De-allocate the buffer...\n");

    if (lpPipeInst != NULL)

        GlobalFree(lpPipeInst);

}



BOOL CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)

{

    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\namedpipe");



    hPipe = CreateNamedPipe(

        lpszPipename,             // pipe name

        PIPE_ACCESS_DUPLEX |      // read/write access

        FILE_FLAG_OVERLAPPED,     // overlapped mode

        PIPE_TYPE_MESSAGE |       // message-type pipe

        PIPE_READMODE_MESSAGE |   // message read mode

        PIPE_WAIT,                // blocking mode

        PIPE_UNLIMITED_INSTANCES, // unlimited instances

        BUFSIZE * sizeof(TCHAR),    // output buffer size

        BUFSIZE * sizeof(TCHAR),    // input buffer size

        PIPE_TIMEOUT,             // client time-out

        NULL);                    // default security attributes

    if (hPipe == INVALID_HANDLE_VALUE)

    {

        printf("CreateNamedPipe() failed with error code %d.\n", GetLastError());

        return 0;

    }

    else

        printf("CreateNamedPipe() is OK!\n");



    // Call a subroutine to connect to the new client

    return ConnectToNewClient(hPipe, lpoOverlap);

}



BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)

{

    BOOL fConnected, fPendingIO = FALSE;



    // Start an overlapped connection for this pipe instance

    fConnected = ConnectNamedPipe(hPipe, lpo);

    // Overlapped ConnectNamedPipe should return zero

    if (fConnected)

    {

        printf("ConnectNamedPipe() failed with error code %d.\n", GetLastError());

        return 0;

    }

    else

        printf("ConnectNamedPipe() should be OK!\n");



    switch (GetLastError())

    {

        // The overlapped connection in progress

    case ERROR_IO_PENDING:

        fPendingIO = TRUE;

        break;

        // Client is already connected, so signal an event

    case ERROR_PIPE_CONNECTED:

        if (SetEvent(lpo->hEvent))

            break;

        // If an error occurs during the connect operation...

    default:

    {

        printf("ConnectNamedPipe() failed with error code %d.\n", GetLastError());

        return 0;

    }

    }

    return fPendingIO;

}

struct ComInit
{
    HRESULT hr;
    ComInit() : hr(::CoInitialize(nullptr)) {}
    ~ComInit() { if (SUCCEEDED(hr)) ::CoUninitialize(); }
};

VOID GetAnswerToRequest(LPPIPEINST pipe)

{

    wprintf(TEXT("[%d] %s\n"), pipe->hPipeInst, pipe->chRequest);
    //Sleep(5000);
    // the URL to download from 
    const wchar_t* srcURL = L"https://google.com";

    // the destination file 
    const LPWSTR destFile = L"myfile.html";

    ComInit init;

    // use CComPtr so you don't have to manually call Release()
    CComPtr<IStream> pStream;

    // Open the HTTP request.
    init.hr = URLOpenBlockingStreamW(nullptr, srcURL, &pStream, 0, nullptr);
    if (FAILED(init.hr))
    {
        std::cout << "ERROR: Could not connect. HRESULT: 0x" << std::hex << init.hr << std::dec << "\n";
    }

    // Download the response and write it to stdout.
    // Download the response and write it to stdout.
    char buffer[BUFSIZE];


    if (FAILED(init.hr))
    {
        std::cout << "ERROR: Download failed. HRESULT: 0x" << std::hex << init.hr << std::dec << "\n";
    }

    std::cout << "\n";

    // URLDownloadToFile returns S_OK on success 
    if (S_OK == URLDownloadToFile(NULL, srcURL, destFile, 0, NULL))
    {

        printf("Saved to 'myfile.txt'");



    }

    else
    {

        printf("Failed");


    }
    DWORD bytesRead = 0;
    init.hr = pStream->Read(buffer, BUFSIZE, &bytesRead);
    std::cout.write(buffer, bytesRead);
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
    std::wstring wstr = conv.from_bytes(std::string(buffer, bytesRead));
    StringCchCopy(pipe->chReply, bytesRead, wstr.c_str());
    pipe->cbToWrite = bytesRead;

}
