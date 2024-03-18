#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


#include "utils.h"

// Flags to identify which signal was raised
static volatile sig_atomic_t sigsegv_raised = 0;
static volatile sig_atomic_t sigint_raised = 0;

// Custom handler for SIGSEGV
static void handle_sigsegv(int sig) {
    sigsegv_raised = 1;
}

// Custom handler for SIGINT
static void handle_sigint(int sig) {
    sigint_raised = 1;
}

void infinite_loop() {
    while(1){};    // Simulating a infinite loop
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <msgid>\n", argv[0]);
        return 1;
    }

    unsigned int seed = 0;

    for (int i = 0; argv[0][i] != '\0'; i++) {
        seed += (unsigned char)argv[0][i]; 
    }

    // TODO: Read input params from message queue until END message is received
    int msgid;
    if ((msgid = msgget(atoi(argv[1]), 0666)) == -1) {
        perror("Failed to create message queue");
        exit(1);
    }

    while (1) {

        // Read from message queue
        msgbuf_t msg;
        if (msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0) == -1) {
            perror("Failed to receive message");
            exit(EXIT_FAILURE);
        }

        if (strcmp(msg.mtext, "END") == 0) {
            break;
        }



        int param = atoi(msg.mtext);  // TODO: Get this value from the message queue

        seed += param;
        srandom(seed);

        int mode = random() % 15 + 1;
        pid_t pid = getpid();


        // Open output file (output/<executable>.<input>) for writing
        size_t output_file_len = strlen("output/") + strlen(argv[0]) + strlen(msg.mtext) + 2;  // +2 for null terminator and '.'
        char *output_file = malloc(output_file_len);
        snprintf(output_file, output_file_len, "output/%s.%s", argv[0], msg.mtext);
        int fd;
        if ((fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
            perror("Failed to open output file");
            exit(EXIT_FAILURE);
        }


        // Using weighted random to avoid every process getting stuck/infinite/crashed
        switch (mode) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                fprintf(stderr, "Program: %s, PID: %d, Mode: 1 - Exiting with status 0 (Correct answer)\n", argv[0], pid);
                // TODO: Write the result to the output file
                write(fd, "0", 1);
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
                fprintf(stderr, "Program: %s, PID: %d, Mode: 2 - Exiting with status 1 (Incorrect answer)\n", argv[0], pid);
                // TODO: Write the result to the output file
                write(fd, "1", 1);
                break;
            case 13:
                fprintf(stderr, "Program: %s, PID: %d, Mode: 3 - Triggering a segmentation fault\n", argv[0], pid);
                raise(SIGSEGV);  // Trigger a segmentation fault
                break;
            case 14:
                fprintf(stderr, "Program: %s, PID: %d, Mode: 4 - Entering an infinite loop\n", argv[0], pid);
                infinite_loop();
                break;
            case 15:
                fprintf(stderr, "Program: %s, PID: %d, Mode: 5 - Simulating being stuck/blocked\n", argv[0], pid);
                pause();  // Simulate being stuck/blocked
                break;
            default:
                break;
        }
        close(fd);
    }

    return 0;
}
