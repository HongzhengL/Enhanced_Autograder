#include "../include/utils.h"

int get_batch_size(){
    int pipe_fd[2]; //create a pipe for outputs

    pipe(pipe_fd);

    pid_t grepproc = fork();

    if(grepproc == 0){ //child process
        close(pipe_fd[0]); //Closing read end, won't need it.
        dup2(pipe_fd[1], 1); //redirecting stdout to pipe
        execl("/bin/grep", "grep", "-c", "^processor", "/proc/cpuinfo", NULL);
        perror("Failed to run Process");
        exit(1);
    }else if(grepproc > 0){ //Parent process
        close(pipe_fd[1]); // close off write
        dup2(pipe_fd[0], 0); //redirect stdin to pipe.    
        int sz;
        char batch_size[32];
        sz = read(0, batch_size, 10);
        batch_size[sz] = '\0';
        close(pipe_fd[0]);
        wait(NULL);
        return atoi(batch_size);
    }else{
        perror("fork failed");
        exit(-1);
    }
}

int main(){

    printf("Core Count = %d\n", get_batch_size());

    return 0;
}

