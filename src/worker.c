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

        // TODO: Redirect STDOUT to output/<executable>.<param> file
        char param_str[20];
        sprintf(param_str, "%d", param);
        int len_output_path = strlen("output/") + strlen(executable_name) + strlen(param_str) + 2;  // +2 for the null terminator and the dot
        char *output_path = malloc(len_output_path);
        if (output_path == NULL) {
            fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
        snprintf(output_path, len_output_path, "output/%s.%s", executable_name, param_str);

        int fd;
        if ((fd = open(output_path, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1) {
            free(output_path);
            fprintf(stderr, "Error occured at line %d: open failed\n", __LINE__ - 2);
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
        free(output_path);
        
        // TODO: Input to child program can be handled as in the EXEC case (see template.c)
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
    for (int j = 0; j < curr_batch_size; j++) {
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < curr_batch_size; j++) {
        char *current_exe_path = pairs[finished + j].executable_path;
        int current_param = pairs[finished + j].parameter;

        int status;
        pid_t pid;
        // TODO: What if waitpid is interrupted by a signal?
        do {
            pid = waitpid(pids[j], &status, 0);
            if (pid == -1 && errno != EINTR) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }
        } while (pid == -1 && errno == EINTR);

        int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);

        // TODO: Check if the process finished normally, segfaulted, or timed out and update the 
        //       pairs array with the results. Use the macros defined in the enum in utils.h for 
        //       the status field of the pairs_t struct (e.g. CORRECT, INCORRECT, SEGFAULT, etc.)
        //       This should be the same as the evaluation in autograder.c, just updating `pairs` 
        //       instead of `results`.
        int final_status = NULL;
        if (signaled) {
            if (WTERMSIG(status) == SIGSEGV) {
                final_status = SEGFAULT;
            } else {
                final_status = STUCK_OR_INFINITE;
            }
        } else if (exited) {
            char *executable_name = get_exe_name(current_exe_path);
            char param_str[20];
            sprintf(param_str, "%d", current_param);
            int length_output_path = strlen("output/") + strlen(executable_name) + strlen(param_str) + 2;  // +2 for the null terminator and the dot
            char *output_path = malloc(length_output_path);    // +2 for the null terminator and the dot

            if (output_path == NULL) {
                fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
                exit(EXIT_FAILURE);
            }
            snprintf(output_path, length_output_path, "output/%s.%s", executable_name, param_str);

            int fd;
            if ((fd = open(output_path, O_RDONLY)) == -1) {
                free(output_path);
                fprintf(stderr, "Error occured at line %d: open failed\n", __LINE__ - 3);
                exit(EXIT_FAILURE);
            }
            free(output_path);
            free(executable_name);

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
        if (final_status == NULL) {
            perror("No final status received");
            exit(EXIT_FAILURE);
        }
        pairs[finished + j].status = final_status;

        // Mark the process as finished
        child_status[j] = -1;
        free(current_exe_path);
    }
    
    free(child_status);
}


// Send results for the current batch back to the autograder
void send_results(int msqid, long mtype, int finished) {
    // Format of message should be ("%s %d %d", executable_path, parameter, status)
    for (int i = 0; i < curr_batch_size; i++) {
        //Locally declaring executable_path, parameter, & status, for simplicity.
        char *executable_path = pairs[finished - curr_batch_size + i].executable_path;
        int parameter = pairs[finished - curr_batch_size + i].parameter;
        int status = pairs[finished - curr_batch_size + i].status;
        
        // Setting up message. 
        msgbuf_t message;
        message.mtype = mtype;

        //Setting message text
        int length_msg = strlen(executable_path) + parameter + status + 3;  // +3 for the null terminator and the 2 spaces
        char *message_text = malloc(length_msg);

        if (message_text == NULL) {
            perror("malloc of message_text failed");
            exit(EXIT_FAILURE);
        }

        snprintf(message_text, length_msg, "%s %d %d", executable_path, parameter, status);

        strncpy(message.mtext, message_text, MESSAGE_SIZE - 1);
        message.mtext[MESSAGE_SIZE - 1] = '\0';
        
        if (msgsnd(msqid, &message, sizeof(message), 0) == -1) {
            free(message_text);
            free(executable_path);
            perror("Failed to send results");
            exit(EXIT_FAILURE);
        }
        free(message_text);
        free(executable_path);
    }
}


// Send DONE message to autograder to indicate that the worker has finished testing
void send_done_msg(int msqid, long mtype) {
    msgbuf_t message;
    message.mtype = mtype;
    char message_to_send[] = "DONE";
    strcpy(message.mtext, message_to_send);

    if (msgsnd(msqid, &message, sizeof(message), 0) == -1) {
        perror("Failed to send DONE");
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

    // TODO: Receive initial message from autograder specifying the number of (executable, parameter) 
    // pairs that the worker will test (should just be an integer in the message body). (mtype = worker_id)
    msgbuf_t init_msg;
    if (msgrcv(msqid, &init_msg, sizeof(init_msg), worker_id, 0) == -1) {
        perror("Initial setup message from master receive failed");
        exit(EXIT_FAILURE);
    }

    // TODO: Parse message and set up pairs_t array

    int pairs_to_test = atoi(init_msg.mtext);
    pairs = malloc(pairs_to_test);

    // TODO: Receive (executable, parameter) pairs from autograder and store them in pairs_t array.
    //       Messages will have the format ("%s %d", executable_path, parameter). (mtype = worker_id)
    for (int i = 0; i < pairs_to_test; i++) {
        msgbuf_t pair;
        if (msgrcv(msqid, &pair, sizeof(pair), worker_id, 0) == -1) {
            perror("Pair retrieval failed");
            exit(EXIT_FAILURE);
        }

        char message_received[MESSAGE_SIZE] = pair.mtext;
        
        char *pair_part = strtok(message_received, " ");
        pairs[i].executable_path = malloc(strlen(pair_part) + 1); //1 for null terminator.
        
        // Retrieving executable_path
        strcpy(pairs[i].executable_path, pair_part);
        
        // Retrieving parameter
        pair_part = strtok(NULL, " ");
        if (pair_part != NULL) {
            pairs[i].parameter = atoi(pair_part);
        } else {
            perror("Parameter missing");
            exit(EXIT_FAILURE);
        }
        free(pair_part);
    }

    // TODO: Send ACK message to mq_autograder after all pairs received (mtype = BROADCAST_MTYPE)
    msgbuf_t ack;
    ack.mtype = BROADCAST_MTYPE;
    char ack_message[] = "ACK";
    strcpy(ack.mtext, ack_message);
    if (msgsnd(msqid, &ack, sizeof(ack), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }


    // TODO: Wait for SYNACK from autograder to start testing (mtype = BROADCAST_MTYPE).
    //       Be careful to account for the possibility of receiving ACK messages just sent.
    

    // Run the pairs in batches of 8 and send results back to autograder
    for (int i = 0; i < pairs_to_test; i+= PAIRS_BATCH_SIZE) {
        int remaining = pairs_to_test - i;
        curr_batch_size = remaining < PAIRS_BATCH_SIZE ? remaining : PAIRS_BATCH_SIZE;
        pids = malloc(curr_batch_size * sizeof(pid_t));

        for (int j = 0; j < curr_batch_size; j++) {
            // TODO: Execute the student executable
            execute_solution(pairs[i + j].executable_path, pairs[i + j].parameter, j);
        }

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

    free(pids);
}
