#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 20
#define MAX_COMMANDS 20
#define MAX_HISTORY 100

// checks if all characters entered are digits
int allDigits(const char *);

//reads the parsed user input and returns a number that corresponds to a switch statement that executes the command
int commandResult(char *, char *[]);

//takes the user input and gets the nth element from history and returns the user input from that element
char *executeHistory(char *[], int, char *[]);

int main(void) {
  size_t inputSize = 0; //used to store the buffer size for getline()
  char *userInput = NULL; //used to store the user input
  char *args[MAX_COMMANDS][MAX_ARGS]; //used to store n arguments and separates them if a pipe is entered
  int argNum[MAX_COMMANDS]; //stores the number of arguments for each command
	int cmdNum = 0; //stores the number of pipes entered
	int histExecution = 0; //makeshift boolean variable that stores 0 if user input if from getline() or 1 if user input is from the history
  char delim = ' '; //stores the only delimiter for the parser
  char *execCmd; //stores the return string from executeHistory()
  int newestHistory = 0; //Stores the value for the newestHistory
  char *history[MAX_HISTORY]; //saves the user inputs, a max of 100 can be stored

  while (1) {
		//Enters this loop when the program is not executing from history
    if (histExecution == 0) {
      cmdNum = 0; //reset the number of commands before entering the parse
      argNum[cmdNum] = 0; //reset the number of arguments before entering the parse
			
      // Create the prompt
      printf("sish> ");

      // Read the input
      ssize_t read = getline(&userInput, &inputSize, stdin);
      if (read == -1) {
        perror("read error");
        exit(EXIT_FAILURE);
      }
      int size = strlen(userInput);
      userInput[size - 1] = '\0'; // Remove newline character

      // save history
      int index = newestHistory % MAX_HISTORY;
      history[index] = strdup(userInput);
      newestHistory++;

      // Parse the command
      char *token = strtok(userInput, &delim);
      while (token != NULL) {
        args[cmdNum][argNum[cmdNum]] = token;
        argNum[cmdNum]++;
        token = strtok(NULL, &delim);

				//if a | is entered, store NULL as the last argument of the first command and store the new command in a different column of the array
        if (strcmp(args[cmdNum][argNum[cmdNum] - 1], "|") == 0) {
          args[cmdNum][argNum[cmdNum] - 1] = NULL;
          argNum[cmdNum]--;
          cmdNum++;
          argNum[cmdNum] = 0;
        }
      }
      args[cmdNum][argNum[cmdNum]] = NULL;
    }

		
    histExecution = 0; //if an input is executed from history, change the value of histExecution to prevent an error

    // Read the code
    int cmdResult = commandResult(args[0][0], args[0]);

		//if cmdResult == 1, exit the program
    if (cmdResult == 1) {
      printf("Goodbye!\n");
      free(userInput);
      break;
    }

    // Execute the code
    switch (cmdResult) {
    case 2: //case 2 is cd with no arguments
      if (chdir(getenv("HOME")) != 0) {
        perror("Cannot change to home directory");
      }
      break;
    case 3: //case 3 is cd with 1 argument
      if (chdir(args[0][1]) != 0) {
        perror("Cannot change to that directory");
      }
      break;
    case 4: //case 4 is history with no arguments
      printf("Command History:\n");
      for (int i = 0; i < newestHistory; i++) {
        printf("%d: %s\n", i, history[i]);
      }
      break;
    case 5: //case 5 is history with -c as the arguemnt
      newestHistory = 0;
      for (int i = 0; i < MAX_HISTORY; i++) {
        history[i] = NULL;
      }
      break;
    case 6: //case 6 is history [#], if the executeHistory returns NULL, the index entered causes an error
      execCmd = executeHistory(args[0], newestHistory, history);
      if (execCmd == NULL) {
        perror("Error: Invalid history index");
        break;
      }

      free(userInput);             // Free the previous user input
      userInput = strdup(execCmd); // copy the command from history to the userInput

      // Parse the replaced command
      argNum[cmdNum] = 0; // Reset argument count
      char *token = strtok(userInput, &delim);
      while (token != NULL) {
        args[cmdNum][argNum[cmdNum]] = token;
        argNum[cmdNum]++;
        token = strtok(NULL, &delim);
        if (strcmp(args[cmdNum][argNum[cmdNum] - 1], "|") == 0) {
          args[cmdNum][argNum[cmdNum] - 1] = NULL;
          argNum[cmdNum]--;
          cmdNum++;
          argNum[cmdNum] = 0;
        }
      }
      args[cmdNum][argNum[cmdNum]] = NULL; //add null to the end of the last command's argument list
      histExecution = 1; //change histExecution to true so the program skips the prompt, read, and parse
      break;

			case 7: //Case 7 is any command that is not Built-in, even if it is not an actual command

			//if only one command is read, enter this block
      if (cmdNum == 0) {
        pid_t cpid = fork(); //create a child process to execute the command

        if (cpid == 0) { // inside the child command

					//execute the command
          if (execvp(args[cmdNum][0], args[cmdNum]) < 0) {
            perror("Execution Failed");
            return EXIT_FAILURE;
          }
        } else { // inside the parent process
          wait(NULL);
        }
      } else if (cmdNum == 1) {//if only 2 commands are read, enter this block
        int fd[2]; //stores the file descriptors for the pipe
        pid_t cpid1, cpid2; //stores the child process ids

				//create the pipe
        if (pipe(fd) == -1) {
          perror("Pipe creation error");
          return EXIT_FAILURE;
        }

				//create the first child to execute command 1
        cpid1 = fork();

        if (cpid1 < 0) {
          perror("fork cpid1 failed");
          return EXIT_FAILURE;
        }
        if (cpid1 == 0) {//inside child 1
					
          close(fd[0]);//close read, child 1 doesnt need it

					//redirect the output from standard to the pipe
          if (dup2(fd[1], STDOUT_FILENO) == -1) {
            perror("dup2 failed in child 1");
            return EXIT_FAILURE;
						continue;
          }
          close(fd[1]); //close write, child 1 doesnt need it

					//execute the first argument in child 1
          if (execvp(args[0][0], args[0]) < 0) {
            perror("execvp failed in child 1");
            return EXIT_FAILURE;
						continue;
          }
        }//exit child 1

				//create the second child to execute command 2
        cpid2 = fork();
		
        if (cpid2 < 0) {
          perror("fork cpid2 failed");
          return EXIT_FAILURE;
					continue;
        }
        if (cpid2 == 0) {//inside child 2
          close(fd[1]);//close write

					//redirect standard input to the pipe
          if (dup2(fd[0], STDIN_FILENO) == -1) {
            perror("dup2 in child 2 failed");
            return EXIT_FAILURE;
          }
          close(fd[0]);//close read

					//execute command 2
          if (execvp(args[1][0], args[1]) < 0) {
            perror("execvp failed in child 2");
            return EXIT_FAILURE;
          }
        }//exit child 2
        close(fd[0]);//close read in parent
        close(fd[1]);//close write in parent

        waitpid(cpid1, NULL, 0); //wait for process 1 to finish
        waitpid(cpid2, NULL, 0); //wait for process 2 to finish
      } else if (cmdNum >= 2) {
        pid_t cpid;
        int prevPipe[2]; //stores the file descriptors for the previous pipe

        for (int i = 0; i < cmdNum; i++) {
          int currPipe[2]; //stores the file desriptors for the current pipe

          // if this is not the last iteration of the loop, create currpipe
          if (i != cmdNum - 1) {
            if (pipe(currPipe) == -1) {
              perror("Pipe creation error");
              return EXIT_FAILURE;
            }
          }

					//create the child process
          cpid = fork();
					
          if (cpid < 0) {
            perror("Fork failed");
            return EXIT_FAILURE;
						break;
          } else if (cpid == 0) { // inside the child process
						
            //if this is not the first command, Redirect input
            if (i != 0) {
              if (dup2(prevPipe[0], STDIN_FILENO) == -1) {
                perror("dup2 stdin failed");
                return EXIT_FAILURE;
								break;
              }
              close(prevPipe[0]);//close read for the previous pipe
              close(prevPipe[1]);//close write for the previous pipe
            }

            // if this is not the last command, redirect output
            if (i != cmdNum - 1) {
              if (dup2(currPipe[1], STDOUT_FILENO) == -1) {
                perror("dup2 stdout failed");
                return EXIT_FAILURE;
								break;
              }
              close(currPipe[0]); //close read for the current pipe
              close(currPipe[1]); //close write for the current pipe
            }

            // Execute the current command
            if (execvp(args[i][0], args[i]) == -1) {
              perror("execvp failed");
              return EXIT_FAILURE;
							break;
            }
          } else { // Parent process
            // if this is not the first command, close previous pipe ends
            if (i != 0) {
              close(prevPipe[0]);
              close(prevPipe[1]);
            }

            // if this is not the last command, save current pipe for the next iteration
            if (i != cmdNum - 1) {
              prevPipe[0] = currPipe[0];
              prevPipe[1] = currPipe[1];
            }
          }
        }

        // Wait for all child processes to complete
        while (wait(NULL) > 0);
      }
      break;

			default://default, in case no value is returned, print an error
			 perror("Error: invalid argument");
    }
  }

  return 0;
}

int commandResult(char *command, char *arguments[]) {
  if (strcmp(command, "exit") == 0) {
    printf("Exiting...\n");
    return 1; //exit
  } else if (strcmp(command, "cd") == 0) {
    if (arguments[1] == NULL) {
      printf("No arguments\n");
      return 2; //cd
    } else {
      return 3; //cd [dir]
    }
  } else if (strcmp(command, "history") == 0) {
    if (arguments[1] == NULL) {
      return 4; //history
    } else if (strcmp(arguments[1], "-c") == 0) {
      return 5; //history -c
    } else if (!allDigits(arguments[1])) {
      return 8; //incorrect arguemnt
    } else if (allDigits(arguments[1])) {
      return 6; //history [offset]
    }
  }
  return 7; //executable command
}

int allDigits(const char *argument) {
  while (*argument) {
    if (!isdigit(*argument)) {
      return 0; //return false if any character is not a digit
    }
    argument++;
  }
  return 1; //return true if all character are digits
}

char *executeHistory(char *arg[], int newHist, char *hist[]) { //returns NULL
  int histIndex = atoi(arg[1]);
  if (histIndex < 0 || histIndex >= MAX_HISTORY || histIndex > newHist) {
    return NULL;
  }
  return hist[histIndex];
}