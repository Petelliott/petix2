#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    // fake shell

    char *args[32];
    char buff[1024];
    while (!feof(stdin)) {
        printf("$ ");
        fgets(buff, sizeof(buff), stdin);
        char *save;

        size_t i = 0;
        for (char *c = strtok_r(buff, " \t\n", &save); c != NULL;
             c = strtok_r(NULL, " \t\n", &save)) {

            args[i++] = c;
        }
        args[i] = NULL;


        pid_t pid = fork();
        if (pid == -1) {
            perror("fork(2)");
        } else if (pid == 0) {
            execve(args[0], args, NULL);
            // if we are here, there must be an error
            perror("execve(2)");

            return 1;
        } else {
            int wstatus;
            waitpid(pid, &wstatus, 0);
        }

        /*
        for (size_t j = 0; j < i; ++j) {
            printf("'%s'\n", args[j]);
        }
        */
    }

    return 0;
}