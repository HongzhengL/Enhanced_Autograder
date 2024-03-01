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
void execute_solution(char *executable_path, char **argv_params, int msgid, int batch_idx) {
    
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);

        // TODO: exec() the student's executable and pass msgid as an argument
        
        perror("Failed to execute program");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
        // TOOD: Send all of the input parameters to the child process via the message queue.
        //       Remember to use get_tag() to determine the message type.

        // TODO: Send END message to child process to indicate that all parameters have been sent

        // TODO: Setup timer to determine if child process is stuck
       
        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int tested, char **argv_params, int msgid) {
    // Keep track of finished processes for alarm handler
    child_status = malloc(curr_batch_size * sizeof(int));
    for (int i = 0; i < curr_batch_size; i++) {
        child_status[i] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int i = 0; i < curr_batch_size; i++) {

        int status;
        pid_t pid = waitpid(pids[i], &status, 0);

        // TODO: Determine if the child process finished normally, segfaulted, or timed out
        int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);

        
        // This part is very similar to the autograder.c version, the main difference is now you 
        // have to check multiple output files (for each parameter) for each executable at a time.
        
        // Adding tested parameters (all of them) to results struct
        for (int j = 0; j < total_params; j++) {
            results[tested - curr_batch_size + i].params_tested[j] = atoi(argv_params[j]);
        }

        // Mark the process as finished
        child_status[i] = -1;
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

    int batch_size = get_batch_size();

    char **executable_paths = get_student_executables(testdir, &num_executables);


    // Construct summary struct
    results = malloc(num_executables * sizeof(autograder_results_t));
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = malloc((total_params) * sizeof(int));
        results[i].status = malloc((total_params) * sizeof(int));
    }

    // TODO: Create a unique key for message queue (CHANGE src/mq_autograder.c TO SOME UNIQUE FILENAME).
    //       For example: create a file with your x500 and use that as the first argument to ftok.
    key_t key = ftok("src/mq_autograder.c", 777); 
    if (key == -1) {
        perror("Failed to create key");
        exit(1);
    }

    // TODO: Create a message queue
    int msgid;
    
    // MAIN LOOP: For a batch of executables at a time, test all parameters
    int finished = 0;
    int tested = 0;
    while (finished < num_executables) {
        // Determine the number of executables to test in this batch
        curr_batch_size = num_executables - finished > batch_size ? batch_size : num_executables - finished;
        pids = malloc(curr_batch_size * sizeof(pid_t));

        // Execute the executables in the batch
        for (int i = 0; i < curr_batch_size; i++) {
            execute_solution(executable_paths[finished + i], argv + 2, msgid, i);
            tested++;
        }

        sleep(2);  // Give the child processes time to start

        // Wait for the batch to finish and check results
        monitor_and_evaluate_solutions(tested, argv + 2, msgid);

        // TODO: Unlink all output files in current batch (output/<executable>.<input>)


        finished += curr_batch_size;

    }

    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug
    // get_score("results.txt", results[0].exe_path);

    write_scores_to_file(results, num_executables, "results.txt");

    // TODO: Remove the message queue


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
