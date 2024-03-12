#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h> // For PATH_MAX
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>


#define TIMEOUT_SECS 10    // Timeout threshold for stuck/infinite loop
#define MAX_INT_CHARS 10 // Maximum number of characters in an integer

/************************* ONLY FOR MESSAGE QUEUES *************************/
// Message queue msgtyp for general messages between mq_autograder and worker
#define BROADCAST_MTYPE 4061  

// Size of message queue message -> max size of executable path sent/received
#define MESSAGE_SIZE 100
/************************* ONLY FOR MESSAGE QUEUES *************************/

// Main struct for storing the results of the autograder
typedef struct {
    char *exe_path;       // path to executable
    int *params_tested;   // array of parameters tested
    int *status;          // array of exit status codes for each parameter
} autograder_results_t;


// Message buffer struct for message queue
typedef struct {
    long mtype;
    char mtext[MESSAGE_SIZE];
} msgbuf_t;


// Define an enum for the program execution outcomes
enum {
    CORRECT = 1,            // Corresponds to case 1: Exit with status 0 (correct answer)
    INCORRECT,              // Corresponds to case 2: Exit with status 1 (incorrect answer)
    SEGFAULT,               // Corresponds to case 3: Triggering a segmentation fault
    STUCK_OR_INFINITE       // Corresponds to case 4 and 5: Stuck, or in an infinite loop
};


// Helper function to get executable name from path
// Example: solutions/sol_1 -> sol_1
char *get_exe_name(char *path);

// Function to convert status macro to the corresponding message
// Example: CORRECT -> "correct"
const char* get_status_message(int status);

// Takes in path to solutions directory and integer address for storing the 
// total number of executables in the solutions directory. Returns a malloc'd
// array of strings containing the executable paths.
char **get_student_executables(char *solution_dir, int *num_executables);


// Count the number of times the pattern "processor" occurs in /proc/cpuinfo
int get_batch_size();


// Create the input/<input>.in files for each parameter
void create_input_files(char **argv_params, int num_parameters);


// Setup timer to determine if child processes are stuck
void start_timer(int seconds, void (*timeout_handler)(int));


// Cancel the timer
void cancel_timer();


// Unlink all of the input/<input>.in files
void remove_input_files(char **argv_params, int num_parameters);


// Unlink all of the output/<executable>.<param> files in the current batch
void remove_output_files(autograder_results_t *results, int tested, int current_batch_size, char *param);


/*
Writes autograder_results_t to a file called results.txt

Note: For the format syntax, let <name:width> denote a field with contents "name" and width "width".
Here is the format (this will be useful for implementing the scores() function):

<exe_name:strlen(longest_exe_name)>:<p1:5> (<status1:9>)<p2:5> (<status2:9>)...<pN:5> (<statusN:9>)

where N is the number of parameters tested and all fields are right-aligned except for exe_name.
*/
void write_results_to_file(autograder_results_t *results, int num_executables, int total_params);


/*
Gets the line containing executable_name's results from the results file and 
calculates the percentage of correct answers for the executable. You must use 
a *seek() function to make this efficient. For more information on the format 
of the results file, see utils.c/write_results_to_file(). Hint: each line of 
the file will have the same length. You should use fgets() or getline() AT MOST 
twice in this function - once for the first line and once for the line containing 
the executable's results. 

Example inputs:
    results_file: "results.txt"
    executable_name: "solutions/sol_5"

Example output:
    0.5
*/
double get_score(char *results_file);


/*
This function goes through each executable and calculates the score using scores().
Then it writes the scores to a file called scores.txt. The format of the file is

<exe_name:strlen(longest_exe_name)>: <score:5.3f>

where <exe_name> is the name of the executable and <score> is the score of the executable.
*/
void write_scores_to_file(autograder_results_t *results, int num_executables, char *results_file);

#endif // UTILS_H
