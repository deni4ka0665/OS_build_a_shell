/**
  * Shell framework
  * course Operating Systems
  * Radboud University
  * v02.10.22

  Student names:
  - Denitsa Pavlova
  - Bezayite Amenu
*/

/**
 * Hint: in most IDEs (Visual Studio Code, Qt Creator, neovim) you can:
 * - Control-click on a function name to go to the definition
 * - Ctrl-space to auto complete functions and variables
 */

// function/class definitions you are going to use
#include <iostream>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <future>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>
#include <list>
#include <optional>

// although it is good habit, you don't have to type 'std' before many objects by including this line
using namespace std;

struct Command
{
  vector<string> parts = {};
};

struct Expression
{
  vector<Command> commands;
  string inputFromFile;
  string outputToFile;
  bool background = false;
};

// Parses a string to form a vector of arguments. The separator is a space char (' ').
vector<string> split_string(const string &str, char delimiter = ' ')
{
  vector<string> retval;
  for (size_t pos = 0; pos < str.length();)
  {
    // look for the next space
    size_t found = str.find(delimiter, pos);
    // if no space was found, this is the last word
    if (found == string::npos)
    {
      retval.push_back(str.substr(pos));
      break;
    }
    // filter out consequetive spaces
    if (found != pos)
      retval.push_back(str.substr(pos, found - pos));
    pos = found + 1;
  }
  return retval;
}

// wrapper around the C execvp so it can be called with C++ strings (easier to work with)
// always start with the command itself
// DO NOT CHANGE THIS FUNCTION UNDER ANY CIRCUMSTANCE
int execvp(const vector<string> &args)
{
  // build argument list
  const char **c_args = new const char *[args.size() + 1];
  for (size_t i = 0; i < args.size(); ++i)
  {
    c_args[i] = args[i].c_str();
  }
  c_args[args.size()] = nullptr;
  // replace current process with new process as specified
  int rc = ::execvp(c_args[0], const_cast<char **>(c_args));
  // if we got this far, there must be an error
  int error = errno;
  // in case of failure, clean up memory (this won't overwrite errno normally, but let's be sure)
  delete[] c_args;
  errno = error;
  return rc;
}

// Executes a command with arguments. In case of failure, returns error code.
int execute_command(const Command &cmd)
{
  auto &parts = cmd.parts;
  if (parts.size() == 0)
    return EINVAL;

  // execute external commands
  int retval = execvp(parts);
  return retval;
}

void display_prompt()
{
  char buffer[512];
  char *dir = getcwd(buffer, sizeof(buffer));
  if (dir)
  {
    cout << "\e[32m" << dir << "\e[39m"; // the strings starting with '\e' are escape codes, that the terminal application interpets in this case as "set color to green"/"set color to default"
  }
  cout << "$ ";
  flush(cout);
}

string request_command_line(bool showPrompt)
{
  if (showPrompt)
  {
    display_prompt();
  }
  string retval;
  getline(cin, retval);
  return retval;
}

// note: For such a simple shell, there is little need for a full-blown parser (as in an LL or LR capable parser).
// Here, the user input can be parsed using the following approach.
// First, divide the input into the distinct commands (as they can be chained, separated by `|`).
// Next, these commands are parsed separately. The first command is checked for the `<` operator, and the last command for the `>` operator.
Expression parse_command_line(string commandLine)
{
  Expression expression;
  vector<string> commands = split_string(commandLine, '|');
  for (size_t i = 0; i < commands.size(); ++i)
  {
    string &line = commands[i];
    vector<string> args = split_string(line, ' ');
    if (i == commands.size() - 1 && args.size() > 1 && args[args.size() - 1] == "&")
    {
      expression.background = true;
      args.resize(args.size() - 1);
    }
    if (i == commands.size() - 1 && args.size() > 2 && args[args.size() - 2] == ">")
    {
      expression.outputToFile = args[args.size() - 1];
      args.resize(args.size() - 2);
    }
    if (i == 0 && args.size() > 2 && args[args.size() - 2] == "<")
    {
      expression.inputFromFile = args[args.size() - 1];
      args.resize(args.size() - 2);
    }
    expression.commands.push_back({args});
  }
  return expression;
}

int execute_expression(Expression &expression)
{
  if (expression.commands.size() == 0)
    return EINVAL;

  if (expression.commands[0].parts.size() > 0)
  {
    // if expression is cd to change directories.
    if (expression.commands[0].parts[0] == "cd")
    {
      string newDirectory = expression.commands[0].parts[1];

      int rc = chdir(newDirectory.c_str());

      if (rc < 0)
      {
        cout << "Something went wrong changing the current directory." << endl;
        return 0;
      }

      cout << "Current working directory changed to: " << newDirectory << endl;
      return rc;
    }
    else if (expression.commands[0].parts[0] == "exit")
    {
      return -2;
    }

    else
    {
      int rc = execute_command(expression.commands[0]);
      if (rc == -1)
      {
        cout << "Not a command." << endl;
        return 0;
      }

      return rc;
    }
    return 0;
  }
}

// framework for executing "date | tail -c 5" using raw commands
// two processes are created, and connected to each other
int step1(bool showPrompt)
{
  // create communication channel shared between the two processes
  // ...

  pid_t child1 = fork();
  if (child1 == 0)
  {
    // redirect standard output (STDOUT_FILENO) to the input of the shared communication channel
    // free non used resources (why?)
    Command cmd = {{string("date")}};
    execute_command(cmd);
    // display nice warning that the executable could not be found
    abort(); // if the executable is not found, we should abort. (why?)
  }

  pid_t child2 = fork();
  if (child2 == 0)
  {
    // redirect the output of the shared communication channel to the standard input (STDIN_FILENO).
    // free non used resources (why?)
    Command cmd = {{string("tail"), string("-c"), string("5")}};
    execute_command(cmd);
    abort(); // if the executable is not found, we should abort. (why?)
  }

  // free non used resources (why?)
  // wait on child processes to finish (why both?)
  waitpid(child1, nullptr, 0);
  waitpid(child2, nullptr, 0);
  return 0;
}

#define READ_END 0
#define WRITE_END 1
#define ENDS 2
int shell(bool showPrompt)
{
  int status = 1; // as long as status code is equal to 1 the command will keep running
  while (cin.good())
  {
    if (status > 0)
    {
      int fd[ENDS];
      int file;
      if (pipe(fd) == -1) // create pipe
      {
        cout << "An error has occured" << endl;
      }

      string commandLine = request_command_line(showPrompt);
      Expression expression = parse_command_line(commandLine);

      pid_t pid0 = fork();
      if (pid0 == 0)
      {
        if (expression.commands.size() > 1 || expression.outputToFile != "")
        {
          dup2(fd[WRITE_END], STDOUT_FILENO); // write the output of the command
          close(fd[READ_END]);                // close all ends of pipe
          close(fd[WRITE_END]);
          execute_command(expression.commands[0]);
        }
        else
        {
          int rc = execute_expression(expression);
          if (rc == -2) // when rc = -2; exit command has been entered
          {
            status = 0; // when status is equal to 0, shell will exit
          }
          else if (rc < 0)
          {
            cout << "Something went wrong!" << endl;
          }
        }
      }

      if (expression.background)
      {
        pid_t pid2 = fork();
        if (pid2 == 0)
        {
          setpgid(0, 0); // change the group id to enable the process running background.
          execute_expression(expression);
        }
      }
      else if (expression.outputToFile != "")
      {
        pid_t pid1 = fork();
        if (pid1 == 0)
        {
          if (dup2(fd[READ_END], STDIN_FILENO) == -1)
          {
            cout << "Failed to redirect stdin of writing to file" << endl;
            return 1;
          }
          else if ((close(fd[READ_END]) == -1) || (close(fd[WRITE_END]) == -1))
          {
            cout << "Failed to close pipe file descriptors" << endl;
            return 1;
          }
          else
          {
            file = open(expression.outputToFile.c_str(), O_WRONLY | O_TRUNC | O_CREAT);

            char buffer[4096]; // create a buffer - hard-coded number.

            read(STDIN_FILENO, buffer, 4096); // read and write to the buffer.

            write(file, buffer, 4096); // write to the file.
            close(fd[READ_END]);       // close end of pipe
            close(fd[WRITE_END]);
            close(file);
          }
        }

        close(fd[READ_END]); // close both ends of pipe - code run by the parent process
        close(fd[WRITE_END]);

        waitpid(pid1, NULL, 0); // waiting for the curr process to finish.
      }
      else if (expression.commands.size() > 1)
      {
        pid_t pid1 = fork();
        if (pid1 == 0)
        {
          dup2(fd[READ_END], STDIN_FILENO);
          close(fd[READ_END]); // close all ends of pipe
          close(fd[WRITE_END]);
          execute_command(expression.commands[1]);
        }

        close(fd[READ_END]); // close both ends of pipe - code run by the parent process
        close(fd[WRITE_END]);

        // wait for change in state to execute the processes
        waitpid(pid0, NULL, 0);
        waitpid(pid1, NULL, 0);
      }
      else
      {
        waitpid(pid0, NULL, 0); // else always late for change in the state of the child process to fork again and show terminal.
      }
    }
    else
    {
      kill(getppid(), 1); // when status code is set to -2 the shell will stop execution.
    }
  }
  return 0;
}
