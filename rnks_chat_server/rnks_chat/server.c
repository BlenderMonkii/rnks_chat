#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h> // Für kbhit() und _getch()

#include "packet.h"

#define BUFFERSIZE 1024
#define _CRT_SECURE_NO_WARNINGS

int main(int argc, char** argv) {
    WSADATA wsaData;
    SOCKET listenSocket = INVALID_SOCKET, clientSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL, hints;
    int iResult;
    u_long mode = 1;  // Non-blocking mode
    Packet packet;
    fd_set readfds;
    struct timeval tv;
    int inputIndex = 0;
    char inputBuffer[BUFFERSIZE] = { 0 };
    BOOL inputInProgress = 0;

    //Check Parameters
    if (argc != 3) {
        fprintf(stderr, "Usage: <Port> <sNumber>\n");
        return 1;
    }

    // Überprüfung des Ports
    int port = atoi(argv[1]); // Konvertiert den String zu einer Ganzzahl
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number.\n");
        return 1;
    }

    // Überprüfung der sNummer
    if (argv[2][0] != 's' || strlen(argv[2]) != 6 || strspn(argv[2] + 1, "0123456789") != 5) {
        fprintf(stderr, "Invalid sNumber format. Expected 's' followed by 5 digits.\n");
        return 1;
    }

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, argv[1], &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for server to listen on
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Set the listening socket to non-blocking mode
    iResult = ioctlsocket(listenSocket, FIONBIO, &mode);
    if (iResult != NO_ERROR) {
        printf("ioctlsocket failed with error: %ld\n", iResult);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    printf("Server started. Waiting for client messages or type message to the client...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listenSocket, &readfds);
        if (clientSocket != INVALID_SOCKET) {
            FD_SET(clientSocket, &readfds);
        }
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50 ms

        iResult = select(0, &readfds, NULL, NULL, &tv);
        if (iResult > 0) {
            if (FD_ISSET(listenSocket, &readfds)) {
                clientSocket = accept(listenSocket, NULL, NULL);
                if (clientSocket == INVALID_SOCKET) {
                    printf("accept failed: %d\n", WSAGetLastError());
                    continue;
                }
                ioctlsocket(clientSocket, FIONBIO, &mode); // Set non-blocking mode
            }

            if (clientSocket != INVALID_SOCKET && FD_ISSET(clientSocket, &readfds)) {
                iResult = recv(clientSocket, (char*)&packet, sizeof(packet), 0);
                if (iResult > 0) {
                    printf("\nReceived from %s: %s\n", packet.sNummer, packet.text);

                    if (inputInProgress) {
                        // Re-print the prompt and the user's partial input after displaying the received message
                        printf("Type your message: %s", inputBuffer);
                    }
                    else {
                        printf("Type your message: ");
                    }
                }
                else if (iResult < 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
                    printf("recv failed: %d\n", WSAGetLastError());
                    closesocket(clientSocket);
                    clientSocket = INVALID_SOCKET;
                }
            }
        }
        // Check for keyboard input without blocking
        if (_kbhit()) {
            char c = _getch(); // Read the character (non-blocking)
            if (c == '\r') { // Enter key
                inputBuffer[inputIndex] = '\0';
                printf("\nSending: %s\n", inputBuffer);
                // Send message to the server
                strncpy(packet.text, inputBuffer, sizeof(packet.text));
                strncpy(packet.sNummer, argv[2], sizeof(packet.sNummer));
                packet.text[sizeof(packet.text) - 1] = '\0'; // Ensure null-termination
                packet.sNummer[sizeof(packet.sNummer)-1] = '\0'; // Ensure null-termination for sNummer

                // Send the message to the server
                send(clientSocket, (char*)&packet, sizeof(packet), 0);

                // Clear the input buffer after sending
                memset(inputBuffer, 0, BUFFERSIZE);

                inputIndex = 0;
                inputInProgress = 0;
            }
            else if (c == '\b') { // Backspace key
                if (inputIndex > 0) {
                    inputBuffer[--inputIndex] = '\0';
                    printf("\b \b"); // Erase the character on the console
                }
            }
            else if (inputIndex < sizeof(inputBuffer) - 1) {
                inputBuffer[inputIndex++] = c;
                putchar(c); // Echo the character to the console
            }
        }
        if (inputInProgress == 0) {
            printf("Type your message: ");
            inputInProgress = 1;
        }
        fflush(stdout);
    }

    // Cleanup
    closesocket(listenSocket);
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
