#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define DEFAULT_PORT 8888
#define BUFFER_SIZE 1024
#define MAX_NAME_LEN 32
#define MAX_CLIENTS 50
#define MAX_ROOMS 20

typedef enum {
    MSG_CONNECT_REQ,     
    MSG_CONNECT_RESP,    
    MSG_JOIN_ROOM_REQ,   
    MSG_JOIN_ROOM_RESP,  
    MSG_GAME_START,      
    MSG_STATE_UPDATE,    // Serverul trimite matricea jocului
    MSG_TURN_NOTIFY,     // Notificare rand
    MSG_MOVE_SUBMIT,     // Clientul trimite o miscare (1-9)
    MSG_MOVE_RESULT,     // Rezultat mutare (invalida, ocupata etc.)
    MSG_GAME_OVER,       // Castig, pierdere sau remiza
    MSG_CHAT_MSG,        
    MSG_ERROR,           
    MSG_DISCONNECT       
} MessageType;

typedef struct {
    MessageType type;
    char sender[MAX_NAME_LEN];
    char text[BUFFER_SIZE];
    int status_code;
    int extra_val;       // Folosit pentru a transmite mutarea (1-9) sau ID camera
    char board[9];       // Tabla de joc ('X', 'O' sau ' ')
} Packet;

#define COLOR_RESET   "[0m"
#define COLOR_RED     "[1;31m"
#define COLOR_GREEN   "[1;32m"
#define COLOR_YELLOW  "[1;33m"
#define COLOR_BLUE    "[1;34m"
#define COLOR_CYAN    "[1;36m"
#define COLOR_MAGENTA "[1;35m"

#endif // COMMON_H
