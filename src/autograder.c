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


// TODO: Timeout handler for alarm signal
void timeout_handler(int signum) {

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
        printf("Pipe created: %d, %d\n", pipefd[0], pipefd[1]);
    #endif
    
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);

        // TODO (Change 1): Redirect STDOUT to output/<executable>.<input> file


        // TODO (Change 2): Handle different cases for input source
        #ifdef EXEC
            execl(executable_path, executable_name, input, NULL);

        #elif REDIR
            
            // TODO: Redirect STDIN to input/<input>.in file
            char *input_path = malloc(strlen("input/") + strlen(input) + strlen(".in") + 1);    // +1 for the null terminator
            sprintf(input_path, "input/%s.in", input);

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
            sprintf(string_of_pipefd, "%d", pipefd[0]);
            execl(executable_path, executable_name, string_of_pipefd, NULL);
        #endif

        // If exec fails
        perror("Failed to execute program");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
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
            close(pipefd[1]);
        #endif

        // TODO (Change 3): Setup timer to determine if child process is stuck
        struct sigaction sa;
        struct itimerval interval;

        sa.sa_handler = timeout_handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, NULL) == -1) {
            perror("Failed to set up signal handler");
            exit(1);
        }

        interval.it_interval.tv_sec = 0;
        interval.it_interval.tv_usec = 0;
        interval.it_value.tv_sec = TIMEOUT_SECS;
        interval.it_value.tv_usec = 0;

        if (setitimer(ITIMER_REAL, &interval, NULL) == -1) {
            perror("Failed to set up timer");
            exit(1);
        } 

        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int tested, char *param, int param_idx) {
    // Keep track of finished processes for alarm handler
    child_status = malloc(curr_batch_size * sizeof(int));
    for (int j = 0; j < curr_batch_size; j++) {
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < curr_batch_size; j++) {

        int status;
        pid_t pid = waitpid(pids[j], &status, 0);

        // TODO: Determine if the child process finished normally, segfaulted, or timed out
        int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);
        
        
        // TODO: Also, update the results struct with the status of the child process


        // Adding tested parameter to results struct
        results[tested - curr_batch_size + j].params_tested[param_idx] = atoi(param);

        // Mark the process as finished
        child_status[j] = -1;
    }

    // TODO: Cancel the timer


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
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = malloc((total_params) * sizeof(int));
        results[i].status = malloc((total_params) * sizeof(int));
    }

    #ifdef REDIR
        // TODO: Create the input/<input>.in files        
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
		
            // Execute the programs in batch size chunks
            for (int j = 0; j < curr_batch_size; j++) {
                execute_solution(executable_paths[tested], argv[i], j);
		        tested++;
            }

            // Wait for the batch to finish and check results
            monitor_and_evaluate_solutions(tested, argv[i], i - 2);

            // TODO Unlink all output files in current batch (output/<executable>.<input>)
            remove_output_files(results, tested, curr_batch_size, argv[i]);  // Implement this function (src/utils.c)
            

            // Adjust the remaining count after the batch has finished
            remaining -= curr_batch_size;
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

    free(pids);
    
    return 0;
}     
