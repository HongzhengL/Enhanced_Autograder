#include "utils.h"

pid_t *workers;          // Workers determined by batch size
int *worker_done;        // 1 for done, 0 for still running

// Stores the results of the autograder (see utils.h for details)
autograder_results_t *results;

int num_executables;      // Number of executables in test directory
int total_params;         // Total number of parameters to test - (argc - 2)
int num_workers;          // Number of workers to spawn


void launch_worker(int msqid, int pairs_per_worker, int worker_id) {
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        // TODO: exec() the worker program and pass it the message queue id and worker id.
        //       Use ./worker as the path to the worker program.
        char msqid_str[MAX_INT_CHARS + 1];
        char worker_id_str[MAX_INT_CHARS + 1];
        snprintf(msqid_str, MAX_INT_CHARS, "%d", msqid);
        snprintf(worker_id_str, MAX_INT_CHARS, "%d", worker_id);
        execl("./worker", "worker", msqid_str, worker_id_str, NULL);
        perror("Failed to spawn worker");
        exit(1);
    } else if (pid > 0) {  // Parent process
        // TODO: Send the total number of pairs to worker via message queue (mtype = worker_id)
        msgbuf_t msg;
        memset(&msg, 0, sizeof(msgbuf_t));
        msg.mtype = worker_id;
        snprintf(msg.mtext, MESSAGE_SIZE, "%d", pairs_per_worker);
        if (msgsnd(msqid, &msg, sizeof(msg), 0) == -1) {
            perror("Failed to send message to worker");
            exit(1);
        }
        // Store the worker's pid for monitoring
        workers[worker_id - 1] = pid;
    } else {  // Fork failed
        perror("Failed to fork worker");
        exit(1);
    }
}


// TODO: Receive ACK from all workers using message queue (mtype = BROADCAST_MTYPE)
void receive_ack_from_workers(int msqid, int num_workers) {
    int received = 0;
    while (received < num_workers) {
        msgbuf_t msg;
        memset(&msg, 0, sizeof(msgbuf_t));
        // +1 to mtype to receive ACK from workers, avoid worker receiving "ACK" from other workers
        if (msgrcv(msqid, &msg, sizeof(msg), BROADCAST_MTYPE + 1, 0) == -1) {
            perror("Failed to receive message from worker");
            exit(EXIT_FAILURE);
        }
        if (strcmp(msg.mtext, "ACK") == 0) {
            received++;
        }
    }
}


// TODO: Send SYNACK to all workers using message queue (mtype = BROADCAST_MTYPE)
void send_synack_to_workers(int msqid, int num_workers) {
    for (int i = 0; i < num_workers; i++) {
        msgbuf_t msg;
        memset(&msg, 0, sizeof(msgbuf_t));
        msg.mtype = BROADCAST_MTYPE;
        snprintf(msg.mtext, MESSAGE_SIZE, "SYNACK");
        if (msgsnd(msqid, &msg, sizeof(msg), 0) == -1) {
            perror("Failed to send message to worker");
            exit(EXIT_FAILURE);
        }
    }
}


// Wait for all workers to finish and collect their results from message queue
void wait_for_workers(int msqid, int pairs_to_test, char **argv_params) {
    int received = 0;
    worker_done = (int *) malloc(num_workers * sizeof(int));
    if (worker_done == NULL) {
        fprintf(stderr, "Error occurred at line %d in %s: malloc failed\n", __LINE__, __FILE__);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_workers; i++) {
        worker_done[i] = 0;
    }

    while (received < pairs_to_test) {
        for (int i = 0; i < num_workers; i++) {
            if (worker_done[i] == 1) {
                continue;
            }

            // Check if worker has finished
            pid_t retpid = waitpid(workers[i], NULL, WNOHANG);

            int msgflg;
            if (retpid > 0)
                // Worker has finished and still has messages to receive
                msgflg = 0;
            else if (retpid == 0)
                // Worker is still running -> receive intermediate results
                msgflg = IPC_NOWAIT;
            else {
                // Error
                perror("Failed to wait for child process");
                exit(1);
            }

            // TODO: Receive results from worker and store them in the results struct.
            //       If message is "DONE", set worker_done[i] to 1 and break out of loop.
            //       Messages will have the format ("%s %d %d", executable_path, parameter, status)
            //       so consider using sscanf() to parse the message.
            while (1) {
                msgbuf_t msg;
                memset(&msg, 0, sizeof(msgbuf_t));
                if (msgrcv(msqid, &msg, sizeof(msg), i + 1, msgflg) == -1) {
                    if (errno == ENOMSG) {  // No message, break
                        break;
                    }
                    perror("Failed to receive message from worker");
                    exit(1);
                }

                // if `DONE`, set worker_done[i] to 1 and break
                if (strcmp(msg.mtext, "DONE") == 0) {
                    worker_done[i] = 1;
                    break;
                }

                // parse message
                char exe_path[MESSAGE_SIZE];
                int param, status;
                sscanf(msg.mtext, "%s %d %d", exe_path, &param, &status);
                for (int j = 0; j < num_executables; j++) {
                    if (strcmp(results[j].exe_path, exe_path) == 0) {
                        for (int k = 0; k < total_params; k++) {
                            if (results[j].params_tested[k] == param) {
                                results[j].status[k] = status;
                                break;
                            }
                        }
                        break;
                    }
                }
                received++;
            }
        }
    }

    free(worker_done);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <testdir> <p1> <p2> ... <pn>\n", argv[0]);
        return 1;
    }

    char *testdir = argv[1];
    total_params = argc - 2;

    char **executable_paths = get_student_executables(testdir, &num_executables);

    // Construct summary struct
    results = (autograder_results_t *) malloc(num_executables * sizeof(autograder_results_t));
    if (results == NULL) {
        fprintf(stderr, "Error occurred at line %d in %s: malloc failed\n", __LINE__, __FILE__);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = (int *) malloc((total_params) * sizeof(int));
        if (results[i].params_tested == NULL) {
            fprintf(stderr, "Error occurred at line %d in file %s: malloc failed\n", __LINE__, __FILE__);
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < total_params; j++) {
            results[i].params_tested[j] = atoi(argv[j + 2]);
        }
        results[i].status = (int *) malloc((total_params) * sizeof(int));
        if (results[i].status == NULL) {
            fprintf(stderr, "Error occurred at line %d in file %s: malloc failed\n", __LINE__, __FILE__);
            exit(EXIT_FAILURE);
        }
    }

    num_workers = get_batch_size();
    // Check if some workers won't be used -> don't spawn them
    if (num_workers > num_executables * total_params) {
        num_workers = num_executables * total_params;
    }
    workers = (pid_t *) malloc(num_workers * sizeof(pid_t));
    if (workers == NULL) {
        fprintf(stderr, "Error occurred at line %d in %s: malloc failed\n", __LINE__, __FILE__);
        exit(EXIT_FAILURE);
    }

    // Create a unique key for message queue
    key_t key = IPC_PRIVATE;

    // TODO: Create a message queue
    int msqid;
    if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("Failed to create message queue");
        exit(EXIT_FAILURE);
    }

    int num_pairs_to_test = num_executables * total_params;

    // Spawn workers and send them the total number of (executable, parameter) pairs they will test
    for (int i = 0; i < num_workers; i++) {
        int leftover = num_pairs_to_test % num_workers - i > 0 ? 1 : 0;
        int pairs_per_worker = num_pairs_to_test / num_workers + leftover;

        // TODO: Spawn worker and send it the number of pairs it will test via message queue
        launch_worker(msqid, pairs_per_worker, i + 1);
    }

    // Send (executable, parameter) pairs to workers
    int sent = 0;
    for (int i = 0; i < total_params; i++) {
        for (int j = 0; j < num_executables; j++) {
            msgbuf_t msg;
            memset(&msg, 0, sizeof(msgbuf_t));
            long worker_id = sent % num_workers + 1;

            // TODO: Send (executable, parameter) pair to worker via message queue (mtype = worker_id)
            msg.mtype = worker_id;
            snprintf(msg.mtext, MESSAGE_SIZE, "%s %s", executable_paths[j], argv[i + 2]);
            if (msgsnd(msqid, &msg, sizeof(msg), 0) == -1) {
                perror("Failed to send message to worker");
                exit(EXIT_FAILURE);
            }
            sent++;
        }
    }

    // TODO: Wait for ACK from workers to tell all workers to start testing (synchronization)
    receive_ack_from_workers(msqid, num_workers);

    // TODO: Send message to workers to allow them to start testing
    send_synack_to_workers(msqid, num_workers);

    // TODO: Wait for all workers to finish and collect their results from message queue
    wait_for_workers(msqid, num_pairs_to_test, argv + 2);


    // TODO: Remove ALL output files (output/<executable>.<input>)
    for (int i = 0; i < num_executables; i++) {
        for (int j = 0; j < total_params; j++) {
            char output_path[PATH_MAX];
            snprintf(output_path, MESSAGE_SIZE, "output/%s.%s", get_exe_name(results[i].exe_path), argv[j + 2]);
            if (unlink(output_path) == -1) {
                perror("Failed to remove output file");
                exit(EXIT_FAILURE);
            }
        }
    }

    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug your scores function
    // get_score("results.txt", results[0].exe_path);

    // Print each score to scores.txt
    write_scores_to_file(results, num_executables, "results.txt");

    // TODO: Remove the message queue
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("Failed to remove message queue");
        exit(1);
    }

    // Free the results struct and its fields
    for (int i = 0; i < num_executables; i++) {
        free(results[i].exe_path);
        free(results[i].params_tested);
        free(results[i].status);
    }

    free(results);
    free(executable_paths);
    free(workers);

    return 0;
}
