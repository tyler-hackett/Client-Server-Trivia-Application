// Author: Tyler Hackett
// Pledge: I pledge my honor that I have abided by the Stevens Honor System.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <errno.h>
#include <sys/select.h>
#define MAX_PLAYERS 2
#define MAX_QUESTIONS 50
#define BUFFER_SIZE 1024

struct Question {
    char prompt[1024];
    char options[3][50];
    int correct_option;
};

struct Player {
    int fd;
    int score;
    char name[128];
};

void send_to_client(int fd, const char* message) {
    if (write(fd, message, strlen(message)) < 0) {
        perror("write failed");
    }
}

int load_questions(struct Question* questions, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open question file");
        return -1;
    }
    char line[BUFFER_SIZE];
    int count = 0;
    while (count < MAX_QUESTIONS && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;
        strncpy(questions[count].prompt, line, sizeof(questions[count].prompt));
        if (!fgets(line, sizeof(line), file)) break;
        line[strcspn(line, "\n")] = '\0';
        char* token = strtok(line, " ");
        for (int i = 0; i < 3 && token; i++) {
            for (char* p = token; *p; p++) {
                if (*p == '_') *p = ' ';
            }
            strncpy(questions[count].options[i], token, sizeof(questions[count].options[i]));
            token = strtok(NULL, " ");
        }
        if (!fgets(line, sizeof(line), file)) break;
        line[strcspn(line, "\n")] = '\0';
        for (int i = 0; i < 3; i++) {
            if (strcmp(questions[count].options[i], line) == 0) {
                questions[count].correct_option = i;
                break;
            }
        }
        count++;
    }
    fclose(file);
    return count;
}

int main(int argc, char* argv[]) {
    char* question_file = "qshort.txt";
    char* ip_address = "127.0.0.1";
    int port = 25555;
    int opt;
    while ((opt = getopt(argc, argv, "f:i:p:h")) != -1) {
        switch (opt) {
            case 'f':
                question_file = optarg;
                break;
            case 'i':
                ip_address = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n", argv[0]);
                printf("-f question_file\tDefault: questions.txt\n");
                printf("-i IP_address\t\tDefault: 127.0.0.1\n");
                printf("-p port_number\t\tDefault: 25555\n");
                printf("-h\t\t\tShow this help info.\n");
                return 0;
            case '?':
                fprintf(stderr, "Error: Unknown option '%c' received\n", optopt);
                return 1;
        }
    }
    struct Question questions[MAX_QUESTIONS];
    int question_count = load_questions(questions, question_file);
    if (question_count <= 0) {
        fprintf(stderr, "Failed to load questions\n");
        return 1;
    }
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(ip_address)
    };
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }
    if (listen(server_fd, MAX_PLAYERS) < 0) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }
    printf("Welcome to 392 Trivia!\n");
    struct Player players[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++) {
        players[i].fd = -1;
        players[i].score = 0;
        players[i].name[0] = '\0';
    }
    int active_players = 0;
    fd_set read_fds;
    int max_fd = server_fd;
    while (active_players < MAX_PLAYERS) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) {
            perror("select error");
            break;
        }
        if (FD_ISSET(server_fd, &read_fds)) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd < 0) {
                perror("accept failed");
                continue;
            }
            if (active_players >= MAX_PLAYERS) {
                printf("Max connections reached!\n");
                close(client_fd);
                continue;
            }
            printf("New connection detected!\n");
            int slot = -1;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].fd == -1) {
                    slot = i;
                    break;
                }
            }
            if (slot == -1) {
                close(client_fd);
                continue;
            }
            players[slot].fd = client_fd;
            active_players++;
            send_to_client(client_fd, "Please type your name: ");
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
        }
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (players[i].fd != -1 && players[i].name[0] == '\0') {
                FD_SET(players[i].fd, &read_fds);
            }
        }
        ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) {
            perror("select error");
            break;
        }
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (players[i].fd != -1 && players[i].name[0] == '\0' && FD_ISSET(players[i].fd, &read_fds)) {
                char name[128];
                int bytes = read(players[i].fd, name, sizeof(name) - 1);
                if (bytes <= 0) {
                    printf("Lost connection!\n");
                    close(players[i].fd);
                    players[i].fd = -1;
                    active_players--;
                    continue;
                }
                name[bytes] = '\0';
                char* newline = strchr(name, '\n');
                if (newline) *newline = '\0';
                strncpy(players[i].name, name, sizeof(players[i].name));
                printf("Hi %s!\n", players[i].name);
            }
        }
    }
    printf("The game starts now!\n");
    for (int q = 0; q < question_count; q++) {
        printf("Question %d: %s\n", q + 1, questions[q].prompt);
        for (int i = 0; i < 3; i++) {
            printf("%d: %s\n", i + 1, questions[q].options[i]);
        }
        char question_msg[BUFFER_SIZE];
        snprintf(question_msg, sizeof(question_msg), 
                "Question %d: %s\nPress 1: %s\nPress 2: %s\nPress 3: %s\n",
                q + 1, questions[q].prompt,
                questions[q].options[0],
                questions[q].options[1],
                questions[q].options[2]);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (players[i].fd != -1) {
                send_to_client(players[i].fd, question_msg);
            }
        }
        int answered = 0;
        while (!answered) {
            FD_ZERO(&read_fds);
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].fd != -1) {
                    FD_SET(players[i].fd, &read_fds);
                }
            }
            int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
            if (activity < 0) {
                perror("select error");
                break;
            }
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].fd != -1 && FD_ISSET(players[i].fd, &read_fds)) {
                    char answer;
                    int bytes = read(players[i].fd, &answer, 1);
                    if (bytes <= 0) {
                        printf("%s disconnected!\n", players[i].name);
                        close(players[i].fd);
                        players[i].fd = -1;
                        continue;
                    }
                    if (answer >= '1' && answer <= '3') {
                        int chosen = answer - '1';
                        char result_msg[BUFFER_SIZE];
                        if (chosen == questions[q].correct_option) {
                            players[i].score++;
                            snprintf(result_msg, sizeof(result_msg),
                                    "Correct! %s gains 1 point (Total: %d)\n", players[i].name, players[i].score);
                        } else {
                            players[i].score--;
                            snprintf(result_msg, sizeof(result_msg),
                                    "Wrong! %s loses 1 point (Total: %d)\n", players[i].name, players[i].score);
                        }
                        printf("%s", result_msg);
                        char correct_msg[BUFFER_SIZE];
                        snprintf(correct_msg, sizeof(correct_msg),
                                "The correct answer was: %s\n", questions[q].options[questions[q].correct_option]);
                        printf("%s", correct_msg);
                        for (int j = 0; j < MAX_PLAYERS; j++) {
                            if (players[j].fd != -1) {
                                send_to_client(players[j].fd, result_msg);
                                send_to_client(players[j].fd, correct_msg);
                            }
                        }
                        answered = 1;
                        break;
                    }
                }
            }
        }
    }
    int max_score = -1;
    char winner[128] = "No one";
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].fd != -1 && players[i].score > max_score) {
            max_score = players[i].score;
            strncpy(winner, players[i].name, sizeof(winner));
        }
    }
    printf("Congrats, %s!\n", winner);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].fd != -1) {
            close(players[i].fd);
        }
    }
    close(server_fd);
    return 0;
}