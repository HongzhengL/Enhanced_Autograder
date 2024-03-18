#include "utils.h"

// Run the (executable, parameter) pairs in batches of 8 to avoid timeouts due to 
// having too many child processes running at once
#define PAIRS_BATCH_SIZE 8

typedef struct {
    char *executable_path;
    int parameter;
    int status;
} pairs_t;

// Store the pairs tested by this worker and the results
pairs_t *pairs;

// Information about the child processes and their results
pid_t *pids;
int *child_status;     // Contains status of child processes (-1 for done, 1 for still running)

int curr_batch_size;   // At most PAIRS_BATCH_SIZE (executable, parameter) pairs will be run at once
long worker_id;        // Used for sending/receiving messages from the message queue


// TODO: Timeout handler for alarm signal - should be the same as the one in autograder.c
void timeout_handler(int signum) {
    for (int j = 0; j < curr_batch_size; j++) {
        if (child_status[j] == 1) {  // Checks if child is still running
            if (kill(pids[j], SIGKILL) == -1) {  // does check on kill signal to see if successful
                perror("Kill Failed");
                exit(EXIT_FAILURE);
            }
        }
    }
}


// Execute the student's executable using exec()
void execute_solution(char *executable_path, int param, int batch_idx) {
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);

        // TODO: Redirect STDOUT to output/<executable>.<input> file
        int path_len = strlen("output/") + strlen(executable_name) + MAX_INT_CHARS + 2;
        // int path_len = PATH_MAX;
        char *output_file = malloc(path_len);
        if (output_file == NULL) {
            fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
        snprintf(output_file, path_len, "output/%s.%d", executable_name, param);
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        free(output_file);
        if (fd == -1) {
            fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 3);
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            fprintf(stderr, "Error occured at line %d: dup2 failed\n", __LINE__ - 1);
            exit(EXIT_FAILURE);
        }
        if (close(fd) == -1) {
            fprintf(stderr, "Error occured at line %d: close failed\n", __LINE__ - 1);
            exit(EXIT_FAILURE);
        }
        // TODO: Input to child program can be handled as in the EXEC case (see template.c)
        char param_str[MAX_INT_CHARS + 1];
        snprintf(param_str, MAX_INT_CHARS, "%d", param);
        execl(executable_path, executable_name, param_str, NULL);
        perror("Failed to execute program in worker");
        exit(1);
    }
    // Parent process
    else if (pid > 0) {
        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int finished) {
    // Keep track of finished processes for alarm handler
    child_status = malloc(curr_batch_size * sizeof(int));
    if (child_status == NULL) {
        fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
        exit(EXIT_FAILURE);
    }
    for (int j = 0; j < curr_batch_size; j++) {
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < curr_batch_size; j++) {
        char *current_exe_path = pairs[finished + j].executable_path;
        int current_param = pairs[finished + j].parameter;

        int status;

        // TODO: What if waitpid is interrupted by a signal?
        // TODO: ERROR CHECK WAITPID
        pid_t pid;
        do {
            pid = waitpid(pids[j], &status, 0);
            if (pid == -1 && errno != EINTR) {
                perror("waitpid failed");
                exit(EXIT_FAILURE);
            }
        } while (pid == -1 && errno == EINTR);

        // int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);

        // TODO: Check if the process finished normally, segfaulted, or timed out and update the 
        //       pairs array with the results. Use the macros defined in the enum in utils.h for 
        //       the status field of the pairs_t struct (e.g. CORRECT, INCORRECT, SEGFAULT, etc.)
        //       This should be the same as the evaluation in autograder.c, just updating `pairs` 
        //       instead of `results`.
        int final_status;
        if (signaled) {
            if (WTERMSIG(status) == SIGSEGV) {
                final_status = SEGFAULT;
            } else {
                final_status = STUCK_OR_INFINITE;
            }
        } else if (exited) {
            char *executable_name = get_exe_name(current_exe_path);
            int length_output_path = strlen("output/") + strlen(executable_name) + MAX_INT_CHARS + 2;  // +2 for the null terminator and the dot
            char *output_path = malloc(length_output_path);    // +2 for the null terminator and the dot
            if (output_path == NULL) {
                fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
                exit(EXIT_FAILURE);
            }
            snprintf(output_path, length_output_path, "output/%s.%d", executable_name, current_param);

            int fd;
            if ((fd = open(output_path, O_RDONLY)) == -1) {
                free(output_path);
                fprintf(stderr, "Error occured at line %d: open failed\n", __LINE__ - 3);
                exit(EXIT_FAILURE);
            }
            free(output_path);

            int bytes_read;
            char output[MAX_INT_CHARS + 1];  // +1 for the null terminator
            if ((bytes_read = read(fd, output, MAX_INT_CHARS)) == -1) {
                perror("Read Failed");
                exit(EXIT_FAILURE);
            }
            if (close(fd) == -1) {
                perror("close failed");
                exit(EXIT_FAILURE);
            }
            output[bytes_read] = '\0';
            if (atoi(output) == 0) {
                final_status = CORRECT;
            } else if (atoi(output) == 1) {
                final_status = INCORRECT;
            } else {
                perror("Invalid output");
                exit(EXIT_FAILURE);
            }
        }
        pairs[finished + j].status = final_status;
        // printf("final_status: %d\n", final_status);
        // Mark the process as finished
        child_status[j] = -1;
    }

    free(child_status);
}


// Send results for the current batch back to the autograder
void send_results(int msqid, long mtype, int finished) {
    // Format of message should be ("%s %d %d", executable_path, parameter, status)
    for (int i = 0; i < curr_batch_size; ++i) {
        msgbuf_t msg;
        msg.mtype = mtype;
        snprintf(msg.mtext, MESSAGE_SIZE, "%s %d %d", pairs[finished + i].executable_path, pairs[finished + i].parameter, pairs[finished + i].status);
        if (msgsnd(msqid, &msg, sizeof(msg), 0) == -1) {
            perror("Failed to send results to autograder");
            exit(EXIT_FAILURE);
        }
    }
}


// Send DONE message to autograder to indicate that the worker has finished testing
void send_done_msg(int msqid, long mtype) {
    msgbuf_t msg;
    msg.mtype = mtype;
    snprintf(msg.mtext, MESSAGE_SIZE, "DONE");
    printf("Worker %ld sending DONE\n", mtype);
    if (msgsnd(msqid, &msg, sizeof(msg), 0) == -1) {
        perror("Failed to send DONE message to autograder");
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <msqid> <worker_id>\n", argv[0]);
        return 1;
    }

    int msqid = atoi(argv[1]);
    worker_id = atoi(argv[2]);
    printf("Worker %ld started\n", worker_id);

    // TODO: Receive initial message from autograder specifying the number of (executable, parameter) 
    // pairs that the worker will test (should just be an integer in the message body). (mtype = worker_id)
    msgbuf_t msg;
    if (msgrcv(msqid, &msg, sizeof(msg), worker_id, 0) == -1) {
        perror("Failed to receive message from autograder");
        exit(EXIT_FAILURE);
    }

    // TODO: Parse message and set up pairs_t array
    int pairs_to_test = atoi(msg.mtext);
    pairs = malloc(pairs_to_test * sizeof(pairs_t));
    if (pairs == NULL) {
        fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < pairs_to_test; i++) {
        pairs[i].executable_path = malloc(PATH_MAX);
        if (pairs[i].executable_path == NULL) {
            fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
    }

    // TODO: Receive (executable, parameter) pairs from autograder and store them in pairs_t array.
    //       Messages will have the format ("%s %d", executable_path, parameter). (mtype = worker_id)
    for (int i = 0; i < pairs_to_test; i++) {
        if (msgrcv(msqid, &msg, sizeof(msg), worker_id, 0) == -1) {
            perror("Failed to receive message from autograder");
            exit(EXIT_FAILURE);
        }
        char *executable_path = strtok(msg.mtext, " ");
        int parameter = atoi(strtok(NULL, " "));
        strcpy(pairs[i].executable_path, executable_path);
        // pairs[i].executable_path = executable_path;
        pairs[i].parameter = parameter;
        printf("Worker %ld received: %s %d i: %d\n", worker_id, pairs[i].executable_path, pairs[i].parameter, i);
    }

    // TODO: Send ACK message to mq_autograder after all pairs received (mtype = BROADCAST_MTYPE)
    msg.mtype = BROADCAST_MTYPE + 1;
    snprintf(msg.mtext, MESSAGE_SIZE, "ACK");
    printf("Sending ACK to autograder\n");
    printf("Worker %ld sending ACK\n", worker_id);
    if (msgsnd(msqid, &msg, sizeof(msg), 0) == -1) {
        perror("Failed to send message to autograder");
        exit(EXIT_FAILURE);
    }
    printf("Pairs[%d].executable_path: %s\n", 0, pairs[0].executable_path);
    // TODO: Wait for SYNACK from autograder to start testing (mtype = BROADCAST_MTYPE).
    //       Be careful to account for the possibility of receiving ACK messages just sent.
    int received = 0;
    printf("Waiting for SYNACK\n");
    while (received < 1) {
        if (msgrcv(msqid, &msg, sizeof(msg), BROADCAST_MTYPE, 0) == -1) {
            perror("Failed to receive message from autograder");
            exit(EXIT_FAILURE);
        }
        if (strcmp(msg.mtext, "SYNACK") == 0) {
            received++;
        }
        printf("msg.mtext: %s\n", msg.mtext);
        printf("worker %ld received: %d / 1\n", worker_id, received);
    }
    printf("Received SYNACK\n");
    // Run the pairs in batches of 8 and send results back to autograder
    for (int i = 0; i < pairs_to_test; i+= PAIRS_BATCH_SIZE) {
        int remaining = pairs_to_test - i;
        curr_batch_size = remaining < PAIRS_BATCH_SIZE ? remaining : PAIRS_BATCH_SIZE;
        pids = malloc(curr_batch_size * sizeof(pid_t));

        for (int j = 0; j < curr_batch_size; j++) {
            // TODO: Execute the student executable
            printf("i + j: %d\n", i + j);
            printf("executable_path: %s\n", pairs[i + j].executable_path);
            printf("parameter: %d\n", pairs[i + j].parameter);
            execute_solution(pairs[i + j].executable_path, pairs[i + j].parameter, j);
        }
        printf("Executed batch %d\n", i);
        // TODO: Setup timer to determine if child process is stuck
        start_timer(TIMEOUT_SECS, timeout_handler);  // Implement this function (src/utils.c)

        // TODO: Wait for the batch to finish and check results
        monitor_and_evaluate_solutions(i);

        // TODO: Cancel the timer if all child processes have finished
        if (child_status == NULL) {
            cancel_timer();
        }

        // TODO: Send batch results (intermediate results) back to autograder
        send_results(msqid, worker_id, i);

        free(pids);
    }

    // TODO: Send DONE message to autograder to indicate that the worker has finished testing
    send_done_msg(msqid, worker_id);

    // Free the pairs_t array
    for (int i = 0; i < pairs_to_test; i++) {
        free(pairs[i].executable_path);
    }
    free(pairs);

}
