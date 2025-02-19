#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 64
#define PROMPT "wish> "

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
            fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
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
                perror("getline");
                exit(1);
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
    // Handle built-in commands
    if (is_builtin_command(args)) {
        return;
    }

    // Fork a child process
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        // Child process: execute command
        execvp(args[0], args);
        // If execvp returns, there was an error
        fprintf(stderr, "Error: Command not found: %s\n", args[0]);
        exit(1);
    } else {
        // Parent process: wait for child to finish
        wait(NULL);
    }
}

// Check for built-in commands
int is_builtin_command(char **args) {
    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }
    // Add more built-in commands here 
    return 0;
}