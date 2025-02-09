#define WIN32_LEAN_AND_MEAN

#include "socketio.h"
#include "logging.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma warning(disable : 4996)
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

SOCKET aSocket = INVALID_SOCKET;

int SocketIO_write(const char* dataToSend, int strSize) {
    int totalSent = 0;
    int iResult;
    while (totalSent < strSize) {
        iResult = send(aSocket, dataToSend + totalSent, strSize - totalSent, 0);
        if (iResult == SOCKET_ERROR) {
            closesocket(aSocket);
            WSACleanup();
            return 1;
        }
        totalSent += iResult;
    }
    return 0;
}

int SocketIO_init(const char* portNumber) {
    WSADATA wsaData;

    struct addrinfo* result = NULL, * ptr = NULL, hints;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        logPrintf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(nullptr, portNumber, &hints, &result);
    if (iResult != 0) {
        logPrintf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        aSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (aSocket == INVALID_SOCKET) {
            logPrintf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(aSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(aSocket);
            aSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (aSocket == INVALID_SOCKET) {
        logPrintf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    logPrintf("Finished connecting to socket\n");
    return 0;
}

