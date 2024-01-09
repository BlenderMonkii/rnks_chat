#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>  // Für kbhit() und _getch()

#include "packet.h"

#define BUFFERSIZE 1024
//#define PORT "49152"
//#define SERVER "2a02:810a:8280:b4c:86b6:32d3:a10c:6ae8"

int main(int argc, char** argv) {
    WSADATA wsaData;
    SOCKET sockfd = INVALID_SOCKET;
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    Packet packet;
    int iResult;
    u_long mode = 1; // Enable non-blocking mode
    int inputIndex = 0;
    char inputBuffer[BUFFERSIZE] = { 0 };
    BOOL inputInProgress = 0;

    //Check Parameters
    if (argc != 4) {
        fprintf(stderr, "Usage: <IPv6 address> <Port> <sNumber>\n");
        return 1;
    }

    // Überprüfung der IPv6-Adresse
    struct sockaddr_in6 sa;
    if (inet_pton(AF_INET6, argv[1], &(sa.sin6_addr)) != 1) {
        fprintf(stderr, "Invalid IPv6 address.\n");
        return 1;
    }

    // Überprüfung des Ports
    int port = atoi(argv[2]); // Konvertiert den String zu einer Ganzzahl
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number.\n");
        return 1;
    }

    // Überprüfung der sNummer
    if (argv[3][0] != 's' || strlen(argv[3]) != 6 || strspn(argv[3] + 1, "0123456789") != 5) {
        fprintf(stderr, "Invalid sNumber format. Expected 's' followed by 5 digits.\n");
        return 1;
    }
    //packet.sNummer = argv[3];

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    // Setup hints for getaddrinfo
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address returned by getaddrinfo
    ptr = result;
    sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sockfd == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Set the socket to non-blocking mode
    iResult = ioctlsocket(sockfd, FIONBIO, &mode);
    if (iResult != NO_ERROR) {
        printf("ioctlsocket failed with error: %ld\n", iResult);
        freeaddrinfo(result);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    // Connect to server
    iResult = connect(sockfd, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            printf("Connect failed with error: %d\n", WSAGetLastError());
            closesocket(sockfd);
            WSACleanup();
            return 1;
        }
    }
    freeaddrinfo(result);

    fd_set readfds;
    struct timeval tv;

    printf("Client started. Type your messages below...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50 ms

        iResult = select(0, &readfds, NULL, NULL, &tv);
        if (iResult > 0 && FD_ISSET(sockfd, &readfds)) {
            iResult = recv(sockfd, (char*)&packet, sizeof(packet), 0);
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
                printf("recv failed with error: %d\n", WSAGetLastError());
                break;
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
                strncpy(packet.sNummer, argv[3], sizeof(packet.sNummer));
                packet.text[sizeof(packet.text) - 1] = '\0'; // Ensure null-termination
                packet.sNummer[sizeof(packet.sNummer)-1] = '\0'; // Ensure null-termination for sNummer

                // Send the message to the server
                send(sockfd, (char*)&packet, sizeof(packet), 0);

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
    closesocket(sockfd);
    WSACleanup();
    return 0;
}
