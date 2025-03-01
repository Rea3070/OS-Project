#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 64
#define PROMPT "wish> "
#define MAX_PATHS 64
static char *manypath[MAX_PATHS] = { "\bin", NULL}; //path declaration for later use
char error_message[30] = "An error has occurred\n";

// Function prototypes
void parse_input(char *input, char **args);
int parse_input_with_redirection(char *input, char **args, char **output_file);
void execute_command(char *input);
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
                write(STDERR_FILENO, error_message, strlen(error_message)); //error message
            }
        }

        // Remove newline character
        input[strcspn(input, "\n")] = '\0';

        // Execute command(s)
        execute_command(input);
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

//This is the new redirection part VVV
int parse_input_with_redirection(char *input, char **args, char **output_file) 
{
    int i = 0;
    int redirection_found = 0;
    *output_file = NULL;
    
    char *token = strtok(input, " ");
    while (token != NULL && i < MAX_ARGS - 1) 
    {
        if (strcmp(token, ">") == 0) 
        {
            redirection_found = 1;
            //Get output file
            token = strtok(NULL, " ");
            if (token == NULL) 
            {
                //No filename specified
                return 0;
            }
            *output_file = token;
            
            // Check for more tokens
            if (strtok(NULL, " ") != NULL) 
            {
                //Multiple filenames
                return 0;
            }
            break;
        } 
        else 
        {
            args[i++] = token;
        }
        token = strtok(NULL, " ");
    }
    
    args[i] = NULL;
    
    //Redirection found no output fil
    if (redirection_found && *output_file == NULL) {
        return 0;
    }
    
    return 1;
}

// Execute a command or multiple commands in parallel
void execute_command(char *input) {
    char *commands[MAX_ARGS];
    int num_commands = 0;
    char input_copy[1024]; // Buffer for input copy
    
    //Make a copy of the input
    strcpy(input_copy, input);

    // Split input by '&' to get individual commands
    char *command = strtok(input_copy, "&");
    while (command != NULL && num_commands < MAX_ARGS - 1) {
        commands[num_commands++] = command;
        command = strtok(NULL, "&");
    }
    commands[num_commands] = NULL;

    pid_t pids[MAX_ARGS];
    int num_pids = 0;

    // Execute each command in parallel
    for (int i = 0; i < num_commands; i++) {
        char *args[MAX_ARGS];
        char *output_file = NULL;
        char command_copy[1024]; // Buffer for command copy
        
        //Make a copy of the command
        strcpy(command_copy, commands[i]);
        
        //Parse command redirection
        int parse_result = parse_input_with_redirection(command_copy, args, &output_file);
        
        //Parsing error
        if (!parse_result) 
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }

        // Handle empty command
        if (args[0] == NULL) {
            continue;
        }

        // Handle built-in commands
        if (is_builtin_command(args)) {
            continue;
        }

        // Fork a child process
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            //Handle redirection
            if (output_file != NULL) 
            {
                //Open output file
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) 
                {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
                //Redirect stdout to file
                if (dup2(fd, STDOUT_FILENO) < 0) 
                {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
                //Redirect stderr to file
                if (dup2(fd, STDERR_FILENO) < 0) 
                {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
                //Close file
                close(fd);
            }
            
            // Child process: Execute command
            execvp(args[0], args);
            // If execvp returns, there was an error
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        } else {
            // Parent process: store child PID
            pids[num_pids++] = pid;
        }
    }

    // Wait for all child processes to complete
    for (int i = 0; i < num_pids; i++) {
        waitpid(pids[i], NULL, 0);
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
