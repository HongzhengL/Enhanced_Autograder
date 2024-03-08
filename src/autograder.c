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
        // TODO: Setup PIPE
    int pipe_fd[2];
    
    if(pipe(pipe_fd) == -1){ // check pipe
        perror("Pipe Failed");
        exit(EXIT_FAILURE);
    }
    #endif
    
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);
            
        // TODO (Change 1): Redirect STDOUT to output/<executable>.<input> file
        char output_path[64];
        sprintf(output_path, "./output/%s.%d", executable_name, input);
        int output_file  = open(output_path, O_CREAT | O_RDWR);
        if(output_file == NULL){ // check if output_file was properly opened
            perror("File Open Failed");
            exit(EXIT_FAILURE);
        }
        if(dup2(output_file, 1) == -1){ // check dup2, setting stdout to redirect to output_file
            fprintf(stderr, "Error on line %d: failed to dup2", __LINE__ - 1);
            exit(EXIT_FAILURE);
        }

        // TODO (Change 2): Handle different cases for input source
        #ifdef EXEC


        #elif REDIR
            
            // TODO: Redirect STDIN to input/<input>.in file
            

        #elif PIPE
            
            // TODO: Pass read end of pipe to child process

        #endif

        // If exec fails
        perror("Failed to execute program");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
        #ifdef PIPE
            // TODO: Send input to child process via pipe
            
        #endif

        // TODO (Change 3): Setup timer to determine if child process is stuck
        

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

int get_batch_size(){
    int nCore = 0;
    int sz; //bytes_read
    int pipe_fd[2]; //create a pipe for outputs

    if(pipe(pipe_fd) == -1){
        perror("Pipe Failed");
        exit(EXIT_FAILURE);
    }

    pid_t grepproc = fork();

    if(grepproc == 0){ //child process

        if(close(pipe_fd[0]) == -1){ //Closing read end, won't need it.
            perror("Close Failed");
            exit(EXIT_FAILURE);
        }

        if(dup2(pipe_fd[1], 1) == -1){//redirecting stdout to pipe
            fprintf(stderr, "Error occured at line %d: dup2 failed\n", __LINE__ - 1);
            exit(EXIT_FAILURE);               
        }

        execlp("/bin/grep", "grep", "-c", "^processor", "/proc/cpuinfo", NULL);
        fprintf(stderr, "Error occured at line %d: Failed to run Process",__LINE__ - 1);
        exit(EXIT_FAILURE);
    }else if(grepproc > 0){ //Parent process
        
        if(close(pipe_fd[1]) == -1){ // close off write
            perror("Close Failed");
            exit(EXIT_FAILURE);
        }
        if(dup2(pipe_fd[0], 0) == -1){ //redirect stdin to pipe.
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }
        char reader[32];
        if((sz = read(pipe_fd[0], reader, 10)) == -1){ //read from pipe
            perror("read failed");
            exit(EXIT_FAILURE);
        }
        reader[sz] = '\0'; // adding string term character to end of batch_size;
        if(close(pipe_fd[0]) == -1){ //closing read end of pipe.
            perror("close failed");
            exit(EXIT_FAILURE);
        }
        wait(NULL);
        nCore = atoi(reader);
        return nCore;
    }else{
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
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
