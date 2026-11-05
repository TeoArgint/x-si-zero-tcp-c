#include "common.h"

static int sock_fd = -1;
static bool my_turn = false;

void draw_board(char b[9]) {
    printf("\n");
    for(int i = 0; i < 9; i++) {
        if(b[i] == 'X') printf(COLOR_RED " X " COLOR_RESET);
        else if(b[i] == 'O') printf(COLOR_BLUE " O " COLOR_RESET);
        else printf(" %d ", i+1); // afiseaza numarul pt input

        if(i % 3 != 2) printf("|");
        if(i % 3 == 2 && i != 8) printf("\n---|---|---\n");
    }
    printf("\n\n");
}

void *receive_handler(void *arg) {
    Packet pkt;
    while (recv(sock_fd, &pkt, sizeof(Packet), 0) > 0) {
        switch (pkt.type) {
            case MSG_CONNECT_RESP:
                printf(COLOR_GREEN "%s" COLOR_RESET "\n", pkt.text);
                break;
            case MSG_JOIN_ROOM_RESP:
                printf(COLOR_CYAN "%s" COLOR_RESET "\n", pkt.text);
                break;
            case MSG_GAME_START:
                printf("\n" COLOR_YELLOW "=========================================\n  %s\n=========================================" COLOR_RESET "\n", pkt.text);
                break;
            case MSG_STATE_UPDATE:
                draw_board(pkt.board);
                break;
            case MSG_TURN_NOTIFY:
                my_turn = (pkt.status_code == 1);
                if (my_turn) {
                    printf(COLOR_GREEN ">>> %s <<< " COLOR_RESET, pkt.text);
                    fflush(stdout);
                } else {
                    printf(COLOR_CYAN "%s" COLOR_RESET "\n", pkt.text);
                }
                break;
            case MSG_MOVE_RESULT:
                printf(COLOR_RED "%s " COLOR_RESET, pkt.text);
                fflush(stdout);
                break;
            case MSG_GAME_OVER:
                printf("\n" COLOR_YELLOW "*****************************************\n  %s\n*****************************************" COLOR_RESET "\n\n", pkt.text);
                my_turn = false;
                break;
            case MSG_CHAT_MSG:
                printf(COLOR_MAGENTA "[%s]: %s" COLOR_RESET "\n", pkt.sender, pkt.text);
                break;
            case MSG_ERROR:
                printf(COLOR_RED "[EROARE]: %s" COLOR_RESET "\n", pkt.text);
                break;
            default: break;
        }
    }
    printf(COLOR_RED "\nDeconectat de la server." COLOR_RESET "\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    char ip[32] = "127.0.0.1";
    int port = DEFAULT_PORT;
    if (argc > 1) strcpy(ip, argv[1]);
    if (argc > 2) port = atoi(argv[2]);

    printf(COLOR_CYAN "=== JOC X SI ZERO (TCP Client) ===\n" COLOR_RESET);
    char username[MAX_NAME_LEN];
    printf("Numele tau: ");
    fgets(username, MAX_NAME_LEN, stdin);
    username[strcspn(username, "\r\n")] = 0;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in s_addr = {0};
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &s_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) {
        perror("Eroare conectare"); return 1;
    }

    Packet auth = {.type = MSG_CONNECT_REQ};
    strcpy(auth.sender, username);
    send(sock_fd, &auth, sizeof(Packet), 0);

    pthread_t tid;
    pthread_create(&tid, NULL, receive_handler, NULL);
    usleep(200000);

    printf("Introdu ID-ul camerei (0-19): ");
    char buf[BUFFER_SIZE];
    bool joined = false;

    while (fgets(buf, BUFFER_SIZE, stdin)) {
        buf[strcspn(buf, "\r\n")] = 0;
        if (strlen(buf) == 0) continue;

        if (!joined) {
            Packet j = {.type = MSG_JOIN_ROOM_REQ, .extra_val = atoi(buf)};
            send(sock_fd, &j, sizeof(Packet), 0);
            joined = true;
            continue;
        }

        if (buf[0] == '/') {
            Packet c = {.type = MSG_CHAT_MSG};
            strcpy(c.text, buf + 1);
            send(sock_fd, &c, sizeof(Packet), 0);
        } else if (my_turn) {
            Packet m = {.type = MSG_MOVE_SUBMIT, .extra_val = atoi(buf)};
            send(sock_fd, &m, sizeof(Packet), 0);
        }
    }
    close(sock_fd);
    return 0;
}
