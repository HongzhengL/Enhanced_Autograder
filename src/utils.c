#include "utils.h"

#define ALIGNMENT 9     // Number of characters to align the status messages

const char* get_status_message(int status) {
    switch (status) {
        case CORRECT: return "correct";
        case INCORRECT: return "incorrect";
        case SEGFAULT: return "crash";
        case STUCK_OR_INFINITE: return "stuck/inf";
        default: return "unknown";
    }
}


char *get_exe_name(char *path) {
    return strrchr(path, '/') + 1;
}


char **get_student_executables(char *solution_dir, int *num_executables) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;

    // Open the directory
    dir = opendir(solution_dir);
    if (!dir) {
        perror("Failed to open directory");
        exit(EXIT_FAILURE);
    }

    // Count the number of executables
    *num_executables = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", solution_dir, entry->d_name);

        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode) && entry->d_name[0] != '.')
                (*num_executables)++;
        } else {
            perror("Failed to get file status");
            exit(EXIT_FAILURE);
        }
    }

    // Allocate memory for the array of strings
    char **executables = (char **) malloc(*num_executables * sizeof(char *));
    if (!executables) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Reset the directory stream
    rewinddir(dir);

    // Read the file names
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", solution_dir, entry->d_name);

        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode) && entry->d_name[0] != '.') {
                int len_executable = strlen(solution_dir) + strlen(entry->d_name) + 2;
                executables[i] = (char *) malloc((len_executable) * sizeof(char));
                if (!executables[i]) {
                    perror("Failed to allocate memory");
                    exit(EXIT_FAILURE);
                }
                snprintf(executables[i], len_executable, "%s/%s", solution_dir, entry->d_name);
                i++;
            }
        }
    }

    // Close the directory
    closedir(dir);

    // Return the array of strings (remember to free the memory later)
    return executables;
}


// TODO: Implement this function
int get_batch_size() {
    int batch_size = 0;
    int bytes_read;  // bytes_read
    int pipe_fd[2];  // create a pipe for outputs

    if (pipe(pipe_fd) == -1) {
        fprintf(stderr, "Error occurred at line %d: pipe failed\n", __LINE__ - 1);
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid == 0) {  // child process
        if (close(pipe_fd[0]) == -1) {  // Closing read end, won't need it.
            fprintf(stderr, "Error occurred at line %d: close failed\n", __LINE__ - 1);
            exit(EXIT_FAILURE);
        }
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {  // redirecting stdout to pipe
            fprintf(stderr, "Error occured at line %d: dup2 failed\n", __LINE__ - 1);
            exit(EXIT_FAILURE);
        }
        execlp("grep", "grep", "-c", "^processor", "/proc/cpuinfo", NULL);
        fprintf(stderr, "Error occured at line %d: Failed to run execlp", __LINE__ - 1);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {  // Parent process
        if (close(pipe_fd[1]) == -1) {  // close off write
            perror("Close Failed");
            exit(EXIT_FAILURE);
        }
        char reader[BUFSIZ];
        if ((bytes_read = read(pipe_fd[0], reader, BUFSIZ)) == -1) {  // read from pipe
            perror("read failed");
            exit(EXIT_FAILURE);
        }
        reader[bytes_read] = '\0';  // adding string term character to end of batch_size;
        if (close(pipe_fd[0]) == -1) {  // closing read end of pipe.
            perror("close failed");
            exit(EXIT_FAILURE);
        }

        pid_t wait_result;
        do {
            wait_result = wait(NULL);
            if (wait_result == -1 && errno != EINTR) {
                perror("wait failed");
                exit(EXIT_FAILURE);
            }
        } while (wait_result == -1 && errno == EINTR);

        batch_size = atoi(reader);
    } else {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    return batch_size;
}


// TODO: Implement this function
void create_input_files(char **argv_params, int num_parameters) {
    for (int i = 0; i < num_parameters; ++i) {
        size_t len_input_file = strlen("input/") + strlen(argv_params[i]) + strlen(".in") + 1;  // +1 for the null terminator
        char *input_file = malloc(len_input_file);  // +1 for the null terminator
        if (input_file == NULL) {
            fprintf(stderr, "Error occurred at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
        snprintf(input_file, len_input_file, "input/%s.in", argv_params[i]);
        FILE *file = fopen(input_file, "w");
        if (!file) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }
        fprintf(file, "%s", argv_params[i]);
        fclose(file);
        free(input_file);
    }
}


// TODO: Implement this function
void start_timer(int seconds, void (*timeout_handler)(int)) {
    struct sigaction sa;
    struct itimerval interval;

    sa.sa_handler = timeout_handler;
    sa.sa_flags = 0;
    if (sigemptyset(&sa.sa_mask) == -1) { 
        perror("Failed to empty sig set");
        exit(EXIT_FAILURE);
    }
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
}


// TODO: Implement this function
void cancel_timer() {
    struct itimerval interval;
    interval.it_interval.tv_sec = 0;
    interval.it_interval.tv_usec = 0;
    interval.it_value.tv_sec = 0;  // Stopping Timer.
    interval.it_value.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &interval, 0)) {
        perror("setitimer");
        exit(EXIT_FAILURE);
    }
}


// TODO: Implement this function
void remove_input_files(char **argv_params, int num_parameters) {
    for (int i = 0; i < num_parameters; ++i) {
        size_t len_input_file = strlen("input/") + strlen(argv_params[i]) + strlen(".in") + 1;  // +1 for the null terminator
        char *input_file = malloc(len_input_file);  // +1 for the null terminator
        if (input_file == NULL) {
            fprintf(stderr, "Error occurred at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
        snprintf(input_file, len_input_file, "input/%s.in", argv_params[i]);
        if (unlink(input_file) == -1) {
            perror("Failed to unlink file");
            free(input_file);
            exit(EXIT_FAILURE);
        }
        free(input_file);
    }
}


// TODO: Implement this function
void remove_output_files(autograder_results_t *results, int tested, int current_batch_size, char *param) {
    for (int i = 0; i < current_batch_size; ++i) {
        char *exe_name = get_exe_name(results[tested - current_batch_size + i].exe_path);
        size_t len_output_file = strlen("output/") + strlen(exe_name) + strlen(param) + 2;  // +1 for the null terminator
        char *output_file = malloc(len_output_file);
        if (output_file == NULL) {
            fprintf(stderr, "Error occurred at line %d: malloc failed\n", __LINE__ - 2);
            exit(EXIT_FAILURE);
        }
        snprintf(output_file, len_output_file, "output/%s.%s", exe_name, param);

        if (unlink(output_file) == -1) {
            perror("Failed to unlink file");
            free(output_file);
            exit(EXIT_FAILURE);
        }

        free(output_file);
    }
}


int get_longest_len_executable(autograder_results_t *results, int num_executables) {
    int longest_len = 0;
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);
        int len = strlen(exe_name);
        if (len > longest_len) {
            longest_len = len;
        }
    }
    return longest_len;
}


void write_results_to_file(autograder_results_t *results, int num_executables, int total_params) {
    FILE *file = fopen("results.txt", "w");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Find the longest executable name (for formatting purposes)
    int longest_len = 0;
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);
        int len = strlen(exe_name);
        if (len > longest_len) {
            longest_len = len;
        }
    }

    // Sort the results data structure by executable name (specifically number at the end)
    for (int i = 0; i < num_executables; i++) {
        for (int j = i + 1; j < num_executables; j++) {
            char *exe_name_i = get_exe_name(results[i].exe_path);
            int num_i = atoi(strrchr(exe_name_i, '_') + 1);
            char *exe_name_j = get_exe_name(results[j].exe_path);
            int num_j = atoi(strrchr(exe_name_j, '_') + 1);
            if (num_i > num_j) {
                autograder_results_t temp = results[i];
                results[i] = results[j];
                results[j] = temp;
            }
        }
    }

    // Write results to file
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);

        char format[20];
        snprintf(format, sizeof(format), "%%-%ds:", longest_len);
        fprintf(file, format, exe_name);  // Write the program path
        for (int j = 0; j < total_params; j++) {
            fprintf(file, "%5d (", results[i].params_tested[j]);  // Write the pi value for the program
            const char* message = get_status_message(results[i].status[j]);
            fprintf(file, "%9s) ", message);  // Write each status
        }
        fprintf(file, "\n");
    }

    fclose(file);
}


// TODO: Implement this function
double get_score(char *result_line) {
    int correct = 0;
    int total = 0;
    char *token = strstr(result_line, "(");
    while (token != NULL) {
        char result[ALIGNMENT + 1];     // +1 for the null terminator
        strncpy(result, token + 1, ALIGNMENT);
        result[9] = '\0';
        if (strcmp(result, "  correct") == 0) {
            ++correct;
        }
        ++total;
        token = strstr(token + 1, "(");
    }
    return (double) correct / total * 1.0;
}


void write_scores_to_file(autograder_results_t *results, int num_executables, char *results_file) {
    FILE *file = fopen(results_file, "r");
    for (int i = 0; i < num_executables; i++) {
        char *line = NULL;
        size_t len = 0;
        getline(&line, &len, file);
        double student_score = get_score(line);

        char *student_exe = get_exe_name(results[i].exe_path);

        char score_file[] = "scores.txt";

        FILE *score_fp;
        if (i == 0)
            score_fp = fopen(score_file, "w");
        else
            score_fp = fopen(score_file, "a");

        if (!score_fp) {
            perror("Failed to open score file");
            exit(1);
        }

        int longest_len = get_longest_len_executable(results, num_executables);

        char format[20];
        snprintf(format, sizeof(format), "%%-%ds: ", longest_len);
        fprintf(score_fp, format, student_exe);
        fprintf(score_fp, "%5.3f\n", student_score);

        fclose(score_fp);
        free(line);
    }
    fclose(file);
}
