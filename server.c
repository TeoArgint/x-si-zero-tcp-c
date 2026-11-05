#include "common.h"

typedef struct {
    int socket_fd;
    char username[MAX_NAME_LEN];
    int room_id;
    char mark; // 'X' sau 'O'
    bool active;
} Client;

typedef struct {
    int room_id;
    char room_name[32];
    int player1_idx; // 'X'
    int player2_idx; // 'O'
    int current_turn_idx;
    char board[9];
    int moves_count;
    bool game_started;
    pthread_mutex_t room_mutex;
} GameRoom;

static Client clients[MAX_CLIENTS];
static GameRoom rooms[MAX_ROOMS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_packet(int socket_fd, Packet *pkt) {
    send(socket_fd, pkt, sizeof(Packet), 0);
}

void broadcast_room(GameRoom *room, Packet *pkt) {
    if (room->player1_idx != -1 && clients[room->player1_idx].active) {
        send_packet(clients[room->player1_idx].socket_fd, pkt);
    }
    if (room->player2_idx != -1 && clients[room->player2_idx].active) {
        send_packet(clients[room->player2_idx].socket_fd, pkt);
    }
}

void reset_board(GameRoom *room) {
    for (int i = 0; i < 9; i++) room->board[i] = ' ';
    room->moves_count = 0;
}

bool check_win(char b[9], char mark) {
    int wins[8][3] = {{0,1,2}, {3,4,5}, {6,7,8}, {0,3,6}, {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6}};
    for (int i = 0; i < 8; i++) {
        if (b[wins[i][0]] == mark && b[wins[i][1]] == mark && b[wins[i][2]] == mark) {
            return true;
        }
    }
    return false;
}

void broadcast_state(GameRoom *room) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = MSG_STATE_UPDATE;
    memcpy(pkt.board, room->board, 9);
    broadcast_room(room, &pkt);
}

void notify_turn(GameRoom *room) {
    Packet turn_pkt;
    memset(&turn_pkt, 0, sizeof(Packet));
    turn_pkt.type = MSG_TURN_NOTIFY;
    
    int current_p = room->current_turn_idx;
    int other_p = (current_p == room->player1_idx) ? room->player2_idx : room->player1_idx;

    turn_pkt.status_code = 1;
    snprintf(turn_pkt.text, BUFFER_SIZE, "Este rândul tău! Joci cu '%c'. Introdu o poziție (1-9):", clients[current_p].mark);
    send_packet(clients[current_p].socket_fd, &turn_pkt);

    turn_pkt.status_code = 0;
    snprintf(turn_pkt.text, BUFFER_SIZE, "Așteaptă... Este rândul lui %s (%c).", clients[current_p].username, clients[current_p].mark);
    send_packet(clients[other_p].socket_fd, &turn_pkt);
}

void handle_move_submission(int client_idx, int move) {
    int r_id = clients[client_idx].room_id;
    if (r_id < 0 || r_id >= MAX_ROOMS) return;

    GameRoom *room = &rooms[r_id];
    pthread_mutex_lock(&room->room_mutex);

    if (!room->game_started || room->current_turn_idx != client_idx) {
        Packet err = {.type = MSG_ERROR};
        strcpy(err.text, "Nu este rândul tău!");
        send_packet(clients[client_idx].socket_fd, &err);
        pthread_mutex_unlock(&room->room_mutex);
        return;
    }

    if (move < 1 || move > 9 || room->board[move - 1] != ' ') {
        Packet err = {.type = MSG_MOVE_RESULT, .status_code = 0};
        strcpy(err.text, "Mutare invalidă sau căsuță ocupată! Încearcă din nou:");
        send_packet(clients[client_idx].socket_fd, &err);
        pthread_mutex_unlock(&room->room_mutex);
        return;
    }

    // Aplica mutarea
    char my_mark = clients[client_idx].mark;
    room->board[move - 1] = my_mark;
    room->moves_count++;

    broadcast_state(room);

    // Verificare Win
    if (check_win(room->board, my_mark)) {
        Packet over = {.type = MSG_GAME_OVER};
        snprintf(over.text, BUFFER_SIZE, "JOC INCHEIAT! %s (%c) a castigat! 🎉", clients[client_idx].username, my_mark);
        broadcast_room(room, &over);
        room->game_started = false;
        pthread_mutex_unlock(&room->room_mutex);
        return;
    }

    // Verificare Remiza
    if (room->moves_count == 9) {
        Packet over = {.type = MSG_GAME_OVER};
        snprintf(over.text, BUFFER_SIZE, "REMIZĂ! Tabla este plină, niciun câștigător.");
        broadcast_room(room, &over);
        room->game_started = false;
        pthread_mutex_unlock(&room->room_mutex);
        return;
    }

    // Schimba runda
    room->current_turn_idx = (client_idx == room->player1_idx) ? room->player2_idx : room->player1_idx;
    notify_turn(room);

    pthread_mutex_unlock(&room->room_mutex);
}

void *client_handler(void *arg) {
    int client_idx = *(int*)arg;
    free(arg);
    int sock = clients[client_idx].socket_fd;
    Packet pkt;

    while (recv(sock, &pkt, sizeof(Packet), 0) > 0) {
        switch (pkt.type) {
            case MSG_CONNECT_REQ: {
                pthread_mutex_lock(&clients_mutex);
                strncpy(clients[client_idx].username, pkt.sender, MAX_NAME_LEN - 1);
                pthread_mutex_unlock(&clients_mutex);

                Packet resp = {.type = MSG_CONNECT_RESP, .status_code = 1};
                snprintf(resp.text, BUFFER_SIZE, "Bine ai venit la X si Zero, %s!", clients[client_idx].username);
                send_packet(sock, &resp);
                break;
            }
            case MSG_JOIN_ROOM_REQ: {
                int tr = pkt.extra_val;
                if (tr < 0 || tr >= MAX_ROOMS) break;
                GameRoom *room = &rooms[tr];
                pthread_mutex_lock(&room->room_mutex);

                if (room->player1_idx == -1) {
                    room->player1_idx = client_idx;
                    clients[client_idx].room_id = tr;
                    clients[client_idx].mark = 'X';
                } else if (room->player2_idx == -1) {
                    room->player2_idx = client_idx;
                    clients[client_idx].room_id = tr;
                    clients[client_idx].mark = 'O';
                } else {
                    Packet err = {.type = MSG_ERROR};
                    strcpy(err.text, "Camera plina!");
                    send_packet(sock, &err);
                    pthread_mutex_unlock(&room->room_mutex);
                    break;
                }

                Packet resp = {.type = MSG_JOIN_ROOM_RESP, .extra_val = tr};
                snprintf(resp.text, BUFFER_SIZE, "Ai intrat in Camera %d. Esti jucatorul '%c'.", tr, clients[client_idx].mark);
                send_packet(sock, &resp);

                if (room->player1_idx != -1 && room->player2_idx != -1) {
                    room->game_started = true;
                    room->current_turn_idx = room->player1_idx;
                    reset_board(room);

                    Packet start = {.type = MSG_GAME_START};
                    snprintf(start.text, BUFFER_SIZE, "Jocul a inceput: %s (X) vs %s (O)", 
                             clients[room->player1_idx].username, clients[room->player2_idx].username);
                    broadcast_room(room, &start);
                    broadcast_state(room);
                    notify_turn(room);
                }
                pthread_mutex_unlock(&room->room_mutex);
                break;
            }
            case MSG_MOVE_SUBMIT:
                handle_move_submission(client_idx, pkt.extra_val);
                break;
            case MSG_CHAT_MSG: {
                int r_id = clients[client_idx].room_id;
                if (r_id >= 0 && r_id < MAX_ROOMS) {
                    Packet chat = {.type = MSG_CHAT_MSG};
                    strcpy(chat.sender, clients[client_idx].username);
                    strcpy(chat.text, pkt.text);
                    broadcast_room(&rooms[r_id], &chat);
                }
                break;
            }
            default: break;
        }
    }

    pthread_mutex_lock(&clients_mutex);
    clients[client_idx].active = false;
    close(sock);
    int r_id = clients[client_idx].room_id;
    if (r_id >= 0) {
        GameRoom *room = &rooms[r_id];
        pthread_mutex_lock(&room->room_mutex);
        if (room->player1_idx == client_idx) room->player1_idx = -1;
        if (room->player2_idx == client_idx) room->player2_idx = -1;
        if (room->game_started) {
            room->game_started = false;
            Packet dc = {.type = MSG_GAME_OVER};
            strcpy(dc.text, "Oponentul s-a deconectat. Joc anulat.");
            broadcast_room(room, &dc);
        }
        pthread_mutex_unlock(&room->room_mutex);
    }
    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) port = atoi(argv[1]);

    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].room_id = i;
        rooms[i].player1_idx = -1;
        rooms[i].player2_idx = -1;
        rooms[i].game_started = false;
        pthread_mutex_init(&rooms[i].room_mutex, NULL);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    printf(COLOR_GREEN "=== SERVER X si ZERO PORNIT pe portul %d ===" COLOR_RESET "\n", port);

    while (1) {
        int new_sock = accept(server_fd, NULL, NULL);
        if (new_sock < 0) continue;

        pthread_mutex_lock(&clients_mutex);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) { slot = i; break; }
        }
        if (slot != -1) {
            clients[slot].socket_fd = new_sock;
            clients[slot].active = true;
            clients[slot].room_id = -1;
            int *arg = malloc(sizeof(int));
            *arg = slot;
            pthread_t tid;
            pthread_create(&tid, NULL, client_handler, arg);
            pthread_detach(tid);
        } else {
            close(new_sock);
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    return 0;
}
