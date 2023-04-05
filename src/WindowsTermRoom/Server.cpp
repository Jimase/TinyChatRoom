#include <cstdio>
#include <cstring>
#include <queue>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <process.h>
#pragma comment(lib, "Ws2_32.lib")

using namespace std;

#define BUFFER_LEN 1024
#define NAME_LEN 20
#define MAX_CLIENT_NUM 32

struct Client {
    int valid;
    int fd_id;
    SOCKET socket;
    char name[NAME_LEN + 1];
} client[MAX_CLIENT_NUM] = {0};

queue<string> message_q[MAX_CLIENT_NUM];

int current_client_num = 0;
CRITICAL_SECTION num_mutex;

HANDLE chat_thread[MAX_CLIENT_NUM] = {0};
HANDLE send_thread[MAX_CLIENT_NUM] = {0};

CRITICAL_SECTION mutex[MAX_CLIENT_NUM] = {0};
CONDITION_VARIABLE cv[MAX_CLIENT_NUM] = {0};

unsigned __stdcall handle_send(void *data) {
    struct Client *pipe = (struct Client *)data;
    while (1) {
        EnterCriticalSection(&mutex[pipe->fd_id]);
        while (message_q[pipe->fd_id].empty()) {
            SleepConditionVariableCS(&cv[pipe->fd_id], &mutex[pipe->fd_id], INFINITE);
        }
        while (!message_q[pipe->fd_id].empty()) {
            string message_buffer = message_q[pipe->fd_id].front();
            int n = message_buffer.length();
            int trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            while (n > 0) {
                int len = send(pipe->socket, message_buffer.c_str(), trans_len, 0);
                if (len < 0) {
                    perror("send");
                    return 0;
                }
                n -= len;
                message_buffer.erase(0, len);
                trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            }
            message_buffer.clear();
            message_q[pipe->fd_id].pop();
        }
        LeaveCriticalSection(&mutex[pipe->fd_id]);
    }
    return 0;
}

void handle_recv(void *data) {
    struct Client *pipe = (struct Client *)data;
    string message_buffer;
    int message_len = 0;
    char buffer[BUFFER_LEN + 1];
    int buffer_len = 0;

    while ((buffer_len = recv(pipe->socket, buffer, BUFFER_LEN, 0)) > 0) {
        for (int i = 0; i < buffer_len; i++) {
            if (message_len == 0) {
                char temp[100];
                sprintf(temp, "%s:", pipe->name);
                message_buffer = temp;
                message_len = message_buffer.length();
            }

            message_buffer += buffer[i];
            message_len++;

            if (buffer[i] == '\n') {
                for (int j = 0; j < MAX_CLIENT_NUM; j++) {
                    if (client[j].valid && client[j].socket != pipe->socket) {
                        EnterCriticalSection(&mutex[j]);
                        message_q[j].push(message_buffer);
                        WakeConditionVariable(&cv[j]);
                        LeaveCriticalSection(&mutex[j]);
                    }
                }
                message_len = 0;
                message_buffer.clear();
            }
        }
    }
    if (buffer_len == -1) {
        perror("recv");
    }
    closesocket(pipe->socket);
    pipe->valid = false;
}

unsigned __stdcall chat(void *data) {
    struct Client *pipe = (struct Client *) data;
    HANDLE hSendThread;
    HANDLE hRecvThread;
    hSendThread = (HANDLE) _beginthreadex(NULL, 0, handle_send, pipe, 0, NULL);
    if (hSendThread == 0) {
        perror("Create send thread");
        return 0;
    }

    hRecvThread = (HANDLE) _beginthreadex(NULL, 0, handle_recv, pipe, 0, NULL);
    if (hRecvThread == 0) {
        perror("Create recv thread");
        return 0;
    }

    WaitForSingleObject(hSendThread, INFINITE);
    WaitForSingleObject(hRecvThread, INFINITE);

    CloseHandle(hSendThread);
    CloseHandle(hRecvThread);
    return 0;
}

/*
 * The `handle_send()` function is responsible for sending messages to a client,
 * while the `handle_recv()` function is responsible for receiving messages from a client.
 * Both of these functions work on a per-client basis, with the `chat()` function handling the creation and management of both threads.
 * When the server receives a message from a client, it is broadcast to all other clients in the `handle_recv()` function.
 * The message is first added to each client's message queue,
 * and then the condition variable for each client is signaled to wake up their respective send threads.
 * The send threads, implemented in the `handle_send()` function, then send the messages to their clients.
 * */


int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }

    // Create server socket
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        perror("socket");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    // Get server port and bind
    int server_port = 0;
    while (1) {
        printf("Please enter the port number of the server: ");
        scanf("%d", &server_port);

        addr.sin_port = htons(server_port);
        if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            perror("bind");
            continue;
        }
        break;
    }

    if (listen(server_sock, MAX_CLIENT_NUM + 1) == SOCKET_ERROR) {
        perror("listen");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("Server started successfully!\n");
    printf("You can join the chatroom by connecting to 127.0.0.1:%d\n\n", server_port);

    // Wait for new clients to join
    while (1) {
        SOCKET client_sock = accept(server_sock, NULL, NULL);
        if (client_sock == INVALID_SOCKET) {
            perror("accept");
            closesocket(server_sock);
            WSACleanup();
            return 1;
        }

        char name[NAME_LEN + 1] = { 0 };
        int state = recv(client_sock, name, NAME_LEN, 0);
        if (state == SOCKET_ERROR) {
            perror("recv");
            closesocket(client_sock);
            continue;
        } else if (state == 0) {
            closesocket(client_sock);
            continue;
        }

        for (int i = 0; i < MAX_CLIENT_NUM; i++) {
            if (!client[i].valid) {
                memset(client[i].name, 0, sizeof(client[i].name));
                strcpy(client[i].name, name);

                client[i].valid = true;
                client[i].fd_id = i;
                client[i].socket = client_sock;

                InitializeCriticalSection(&mutex[i]);
                InitializeConditionVariable(&cv[i]);

                HANDLE hChatThread;
                hChatThread = (HANDLE)_beginthreadex(NULL, 0, chat, (void *)&client[i], 0, NULL);
                if (hChatThread == 0) {
                    perror("Create chat thread");
                    break;
                }

                printf("%s joined the chatroom. Online User Number: %d\n", client[i].name, ++current_client_num);
                CloseHandle(hChatThread);
                break;
            }
        }
    }

    closesocket(server_sock);
    WSACleanup();
    return 0;
}
