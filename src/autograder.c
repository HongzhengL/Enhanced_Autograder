#include "utils.h"

// Batch size is determined at runtime now
pid_t *pids;

// Stores the results of the autograder (see utils.h for details)
autograder_results_t *results;

int num_executables;      // Number of executables in test directory
int curr_batch_size;      // At most batch_size executables will be run at once
int total_params;         // Total number of parameters to test - (argc - 2)

// Contains status of child processes (-1 for done, 1 for still running)
int *child_status;


// TODO (Change 3): Timeout handler for alarm signal - kill remaining running child processes
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
void execute_solution(char *executable_path, char *input, int batch_idx) {
    #ifdef PIPE

        // TODO: Setup pipe
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            fprintf(stderr, "Error occured at line %d: pipe failed", __LINE__ - 1);
            exit(EXIT_FAILURE);
        }
    #endif

    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);

        // TODO (Change 1): Redirect STDOUT to output/<executable>.<input> file

        int len_output_path = strlen("output/") + strlen(executable_name) + strlen(input) + 2;  // +2 for the null terminator and the dot
        char *output_path = malloc(len_output_path);
        if (output_path == NULL) {
            fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
        snprintf(output_path, len_output_path, "output/%s.%s", executable_name, input);

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

        // TODO (Change 2): Handle different cases for input source
        #ifdef EXEC

            execl(executable_path, executable_name, input, NULL);
            
        #elif REDIR

            // TODO: Redirect STDIN to input/<input>.in file
            int len_input_path = strlen("input/") + strlen(input) + strlen(".in") + 1;  // +1 for the null terminator
            char *input_path = malloc(len_input_path);    // +1 for the null terminator
            if (input_path == NULL) {
                fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
                exit(EXIT_FAILURE);
            }
            snprintf(input_path, len_input_path, "input/%s.in", input);

            int child_fd = open(input_path, O_RDONLY);
            free(input_path);
            if (child_fd == -1) {
                fprintf(stderr, "Error occured at line %d: open failed", __LINE__ - 3);
                exit(EXIT_FAILURE);
            }
            if (dup2(child_fd, STDIN_FILENO) == -1) {
                fprintf(stderr, "Error occured at line %d: dup2 failed\n", __LINE__ - 1);
                exit(EXIT_FAILURE);
            }
            if (close(child_fd) == -1) {
                fprintf(stderr, "Error occured at line %d: close failed\n", __LINE__ - 1);
                exit(EXIT_FAILURE);
            }
            execl(executable_path, executable_name, NULL);

        #elif PIPE

            // TODO: Pass read end of pipe to child process
            if (close(pipefd[1]) == -1) {
                fprintf(stderr, "Error occured at line %d: close failed", __LINE__ - 1);
                exit(EXIT_FAILURE);
            }
            if (dup2(pipefd[0], STDIN_FILENO) == -1) {
                fprintf(stderr, "Error occured at line %d: dup2 failed", __LINE__ - 1);
                exit(EXIT_FAILURE);
            }
            char string_of_pipefd[MAX_INT_CHARS + 1];
            snprintf(string_of_pipefd, sizeof(string_of_pipefd), "%d", pipefd[0]);
            execl(executable_path, executable_name, string_of_pipefd, NULL);
        #endif

        // If exec fails
        perror("Failed to execute program");
        exit(1);
    } else if (pid > 0) {  // Parent process
        #ifdef PIPE
            // TODO: Send input to child process via pipe
            if (close(pipefd[0]) == -1) {
                fprintf(stderr, "Error occured at line %d: close failed", __LINE__ - 1);
                exit(EXIT_FAILURE);
            }
            if (write(pipefd[1], input, strlen(input)) == -1) {
                fprintf(stderr, "Error occured at line %d: write failed", __LINE__ - 1);
                exit(EXIT_FAILURE);
            }
            if (close(pipefd[1]) == -1) {
                fprintf(stderr, "Error occured at line %d: close failed", __LINE__ - 1);
                exit(EXIT_FAILURE);
            }
        #endif

        pids[batch_idx] = pid;
    } else {  // Fork failed
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int tested, char *param, int param_idx) {
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
        int status;
        errno = 0;
        // TODO: What if waitpid is interrupted by a signal?
        pid_t pid;
        do {
            pid = waitpid(pids[j], &status, 0);
            if (pid == -1 && errno != EINTR) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }
        } while (pid == -1 && errno == EINTR);

        // TODO: Determine if the child process finished normally, segfaulted, or timed out
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);
        int final_status;
        if (signaled) {
            if (WTERMSIG(status) == SIGSEGV) {
                final_status = SEGFAULT;
            } else {
                final_status = STUCK_OR_INFINITE;
            }
        } else if (exited) {
            char *executable_name = get_exe_name(results[tested - curr_batch_size + j].exe_path);
            int length_output_path = strlen("output/") + strlen(executable_name) + strlen(param) + 2;  // +2 for the null terminator and the dot
            char *output_path = malloc(length_output_path);    // +2 for the null terminator and the dot
            if (output_path == NULL) {
                fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
                exit(EXIT_FAILURE);
            }
            snprintf(output_path, length_output_path, "output/%s.%s", executable_name, param);

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

        // TODO: Also, update the results struct with the status of the child process
        results[tested - curr_batch_size + j].status[param_idx] = final_status;

        // NOTE: Make sure you are using the output/<executable>.<input> file to determine the status
        //       of the child process, NOT the exit status like in Project 1.

        // Adding tested parameter to results struct
        results[tested - curr_batch_size + j].params_tested[param_idx] = atoi(param);

        // Mark the process as finished
        child_status[j] = -1;
    }

    free(child_status);
}



int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <testdir> <p1> <p2> ... <pn>\n", argv[0]);
        return 1;
    }

    char *testdir = argv[1];
    total_params = argc - 2;

    // TODO (Change 0): Implement get_batch_size() function
    int batch_size = get_batch_size();

    char **executable_paths = get_student_executables(testdir, &num_executables);

    // Construct summary struct
    results = malloc(num_executables * sizeof(autograder_results_t));
    if (results == NULL) {
        fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = malloc((total_params) * sizeof(int));
        if (results[i].params_tested == NULL) {
            fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
        results[i].status = malloc((total_params) * sizeof(int));
        if (results[i].status == NULL) {
            fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
    }

    #ifdef REDIR
        // TODO: Create the input/<input>.in files and write the parameters to them
        create_input_files(argv + 2, total_params);  // Implement this function (src/utils.c)
    #endif

    // MAIN LOOP: For each parameter, run all executables in batch size chunks
    for (int i = 2; i < argc; i++) {
        int remaining = num_executables;
        int tested = 0;

        // Test the parameter on each executable
        while (remaining > 0) {
            // Determine current batch size - min(remaining, batch_size)
            curr_batch_size = remaining < batch_size ? remaining : batch_size;
            pids = malloc(curr_batch_size * sizeof(pid_t));
            if (pids == NULL) {
                fprintf(stderr, "Error occured at line %d: malloc failed\n", __LINE__ - 2);
                exit(EXIT_FAILURE);
            }

            // TODO: Execute the programs in batch size chunks
            for (int j = 0; j < curr_batch_size; j++) {
                execute_solution(executable_paths[tested], argv[i], j);
                tested++;
            }

            // TODO (Change 3): Setup timer to determine if child process is stuck
            start_timer(TIMEOUT_SECS, timeout_handler);  // Implement this function (src/utils.c)

            // TODO: Wait for the batch to finish and check results
            monitor_and_evaluate_solutions(tested, argv[i], i - 2);

            // TODO: Cancel the timer if all child processes have finished
            if (child_status == NULL) {
                cancel_timer();  // Implement this function (src/utils.c)
            }

            // TODO Unlink all output files in current batch (output/<executable>.<input>)
            remove_output_files(results, tested, curr_batch_size, argv[i]);  // Implement this function (src/utils.c)


            // Adjust the remaining count after the batch has finished
            remaining -= curr_batch_size;

            free(pids);
        }
    }

    #ifdef REDIR
        // TODO: Unlink all input files for REDIR case (<input>.in)
        remove_input_files(argv + 2, total_params);  // Implement this function (src/utils.c)
    #endif

    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug your scores function
    // get_score("results.txt", results[0].exe_path);

    // Print each score to scores.txt
    write_scores_to_file(results, num_executables, "results.txt");

    // Free the results struct and its fields
    for (int i = 0; i < num_executables; i++) {
        free(results[i].exe_path);
        free(results[i].params_tested);
        free(results[i].status);
    }

    free(results);
    free(executable_paths);

    return 0;
}
