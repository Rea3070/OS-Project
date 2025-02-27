#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 64
#define PROMPT "wish> "
#define MAX_PATHS 64
static char *manypath[MAX_PATHS] = { "\bin", NULL}; //path declaration for later use
char error_message[30] = "An error has occurred\n";

// Function prototypes
void parse_input(char *input, char **args);
void execute_command(char **args);
int is_builtin_command(char **args);

int main(int argc, char *argv[]) {
    char *input = NULL;
    size_t input_size = 0;
    FILE *input_stream = stdin;

    // Batch mode: read from file
    if (argc == 2) {
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message)); //error message
            exit(1);
        }
    } else if (argc > 2) {
        fprintf(stderr, "Usage: %s [batch_file]\n", argv[0]);
        exit(1);
    }

    // Main shell loop
    while (1) {
        // Print prompt in interactive mode
        if (input_stream == stdin) {
            printf(PROMPT);
        }

        // Read input
        if (getline(&input, &input_size, input_stream) == -1) {
            if (feof(input_stream)) {
                // End of file (EOF) reached
                break;
            } else {
                pwrite(STDERR_FILENO, error_message, strlen(error_message)); //error message
            }
        }

        // Remove newline character
        input[strcspn(input, "\n")] = '\0';

        // Parse input into arguments
        char *args[MAX_ARGS];
        parse_input(input, args);

        // Handle empty input
        if (args[0] == NULL) {
            continue;
        }

        // Execute command
        execute_command(args);
    }

    // Cleanup
    free(input);
    if (input_stream != stdin) {
        fclose(input_stream);
    }

    return 0;
}

// Parse input into arguments
void parse_input(char *input, char **args) {
    int i = 0;
    char *token = strtok(input, " ");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL; // Null-terminate the argument list
}

// Execute a command
void execute_command(char **args) {
    char *commands[MAX_ARGS];
    int num_commands = 0;

    char *command = strtok(args[0], "&");
    while (command != NULL && num_commands < MAX_ARGS - 1) {
        commands[num_commands++] = command;
        command = strtok(NULL, "&");
    }
    commands[num_commands] = NULL; // Null-terminate the command list

    pid_t pid[MAX_ARGS];
    int num_pids = 0;

    for(int i = 0; i < num_commands; i++) {
        char *command_args[MAX_ARGS];
        parse_input(commands[i], command_args);

        if(command_args[0] == NULL) {
            continue; // Skip empty commands
        }

        // Check for built-in commands
        if (is_builtin_command(command_args)) {
            continue;
        }

        // Fork a child process
        pid[num_pids] = fork();
        if (pid[num_pids] == 0) { // Child process
            // Construct the full path for the command
            char full_path[256];
            int found = 0;
            for (int j = 0; manypath[j] != NULL; j++) {
                snprintf(full_path, sizeof(full_path), "%s/%s", manypath[j], command_args[0]);
                if (access(full_path, X_OK) == 0) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                write(STDERR_FILENO, error_message, strlen(error_message)); //error message
                exit(1);
            }

            // Execute the command
            execv(full_path, command_args);
            write(STDERR_FILENO, error_message, strlen(error_message)); //error message
            exit(1);
        } else if (pid[num_pids] < 0) { // Fork failed
            write(STDERR_FILENO, error_message, strlen(error_message)); //error message
        } else { // Parent process
            num_pids++;
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < num_pids; i++) {
        waitpid(pid[i], NULL, 0);
    }
}

// Check for built-in commands
int is_builtin_command(char **args) {

    int argnum = 0;
    
    //counts number of arguments without passing argv[]
    while (args[argnum] != NULL) {
        argnum++;
    }

    if (strcmp(args[0], "exit") == 0) {
        if (argnum > 1) {
            write(STDERR_FILENO, error_message, strlen(error_message));  
            return 1;
        }
        exit(0);
    }
    // Add more built-in commands here 

    if (strcmp(args[0], "cd") == 0) {
        if (argnum != 2) {
            write(STDERR_FILENO, error_message, strlen(error_message));  
            return 1;
        }
        
        if (chdir(args[1]) != 0) { // runs chdir, if it doesn't return 0, it failed
            write(STDERR_FILENO, error_message, strlen(error_message)); //error message
            return 1;
        }

        return 1;
    }

    if (strcmp(args[0], "path") == 0) {
        for (int i = 0; i < MAX_PATHS; i++) {
            manypath[i] = NULL; //makes sure that each time path is called it resets whatever was last stored in manypath
        }

        for (int i = 1; args[i] != NULL && i < MAX_PATHS; i++) {
            manypath[i - 1] = strdup(args[i]); //store manypath
        }

        manypath[MAX_PATHS - 1] = NULL; // cleanup

        return 1;
    }

    return 0;
}
