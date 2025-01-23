
/*
 * CS354: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include "command.h"
#include <fcntl.h>
#include <time.h>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <fstream>

pid_t last_child_pid;


SimpleCommand::SimpleCommand()
{
	// Create available space for 5 arguments
	_numberOfAvailableArguments = 5;
	_numberOfArguments = 0;
	_arguments = (char **) malloc( _numberOfAvailableArguments * sizeof( char * ) );
}

void
SimpleCommand::insertArgument( char * argument )
{
	if ( _numberOfAvailableArguments == _numberOfArguments  + 1 ) {
		// Double the available space
		_numberOfAvailableArguments *= 2;
		_arguments = (char **) realloc( _arguments,
				  _numberOfAvailableArguments * sizeof( char * ) );
	}
	
	_arguments[ _numberOfArguments ] = argument;

	// Add NULL argument at the end
	_arguments[ _numberOfArguments + 1] = NULL;
	
	_numberOfArguments++;
}

Command::Command()
{
	// Create available space for one simple command
	_numberOfAvailableSimpleCommands = 1;
	_simpleCommands = (SimpleCommand **)
		malloc( _numberOfSimpleCommands * sizeof( SimpleCommand * ) );

	_numberOfSimpleCommands = 0;	
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_append = 0;	
	_background = 0;
	_pipes = 0;	
}

void
Command::insertSimpleCommand( SimpleCommand * simpleCommand )
{
        _pipes++;
	if ( _numberOfAvailableSimpleCommands == _numberOfSimpleCommands ) {
		_numberOfAvailableSimpleCommands *= 2;
		_simpleCommands = (SimpleCommand **) realloc( _simpleCommands,
			 _numberOfAvailableSimpleCommands * sizeof( SimpleCommand * ) );
	}
	
	_simpleCommands[ _numberOfSimpleCommands ] = simpleCommand;
	_numberOfSimpleCommands++;
}

void
Command:: clear()
{
	for ( int i = 0; i < _numberOfSimpleCommands; i++ ) {
		for ( int j = 0; j < _simpleCommands[ i ]->_numberOfArguments; j ++ ) {
			free ( _simpleCommands[ i ]->_arguments[ j ] );
		}
		
		free ( _simpleCommands[ i ]->_arguments );
		free ( _simpleCommands[ i ] );
	}

	if ( _outFile ) {
		free( _outFile );
	}

	if ( _inputFile ) {
		free( _inputFile );
	}

	if ( _errFile ) {
		free( _errFile );
	}

	_numberOfSimpleCommands = 0;
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_append = 0;		
	_background = 0;
	_pipes = 0;	
}

void
Command::print()
{
	printf("\n\n");
	printf("              COMMAND TABLE                \n");
	printf("\n");
	printf("  #   Simple Commands\n");
	printf("  --- ----------------------------------------------------------\n");
	
	for ( int i = 0; i < _numberOfSimpleCommands; i++ ) {
		printf("  %-3d ", i );
		for ( int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++ ) {
			printf("\"%s\" \t", _simpleCommands[i]->_arguments[ j ] );
		}
	}

	printf( "\n\n" );
	printf( "  Output       Input        Error        Background\n" );
	printf( "  ------------ ------------ ------------ ------------\n" );
	printf( "  %-12s %-12s %-12s %-12s\n", _outFile?_outFile:"default",
		_inputFile?_inputFile:"default", _errFile?_errFile:"default",
		_background?"YES":"NO");
	printf( "\n\n" );
	
}

void handle_logsig(int sig) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::ofstream log("/home/vboxuser/Desktop/Labs_Done/8164-Lab3/log.txt", std::ios::app);    
    if (pid <= 0) {
        pid = last_child_pid;
        log << "Process " << pid << " terminated @ " 
        << std::put_time(std::localtime(&currentTime), "%Y-%m-%d %H:%M:%S") 
        << ".\n";
    log.close();
    }
    log << "Child " << pid << " terminated @ " 
        << std::put_time(std::localtime(&currentTime), "%Y-%m-%d %H:%M:%S") 
        << ".\n";
    log.close();
}

void setup_signals() {
    signal(SIGCHLD, handle_logsig);
    signal(SIGINT, SIG_IGN);
}


void Command::execute()
{
	// Don't do anything if there are no simple commands
	if (_numberOfSimpleCommands == 0) {
		prompt();
		return;
	}
    print();
	// Handle exit command
	if (strcmp(_simpleCommands[0]->_arguments[0], "exit") == 0) {
		std::cout << "Goodbye!" << std::endl;
		exit(0);
	}

	// Handle cd command
	if (strcmp(_currentSimpleCommand->_arguments[0], "cd") == 0) {
		int target_dir;
		if (_currentSimpleCommand->_numberOfArguments == 1) {
			target_dir = chdir(getenv("HOME"));
		} else {
			target_dir = chdir(_currentSimpleCommand->_arguments[1]);
		}
		if (target_dir == -1) {
			std::cerr << "Directory change failed" << std::endl;
		}
		clear();
		prompt();
		return;
	}		

	int defaultin = dup(0);
	int defaultout = dup(1);
	int defaulterr = dup(2);
	int outfile, inputfile, errfile;
	int fdpipe[_pipes][2];

	// error redirection
	if (_errFile) {
		errfile = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (errfile >= 0) {
            dup2(errfile, 2);
            close(errfile);
        }
	}

	// Set up pipes and redirection
	for (int cmd_idx = 0; cmd_idx < _numberOfSimpleCommands; cmd_idx++) {
		// Create a pipe for each command, if necessary
		if (pipe(fdpipe[cmd_idx]) == -1) {		
			std::cerr << "Pipe creation failed" << std::endl;
			exit(1);
		}

		if (cmd_idx == 0) { // First command
			// Input Redirection
			if (_inputFile)
			{
				inputfile = open(_inputFile, 0666);
				dup2(inputfile, 0);
				close(inputfile);
			} 
			else 
			{
				dup2(defaultin, 0);
			}
			// Output Redirection if there’s a pipeline
			if (_numberOfSimpleCommands > 1) 
			{
				dup2(fdpipe[cmd_idx][1], 1);
				close(fdpipe[cmd_idx][1]);
			}
		} if (cmd_idx == _numberOfSimpleCommands - 1) { // Last command
			// Input Redirection from previous pipe
			if (_numberOfSimpleCommands > 1) {
				dup2(fdpipe[cmd_idx - 1][0], 0);
				close(fdpipe[cmd_idx - 1][0]);
			}
			// Output Redirection
			if (_outFile) {
				if (_append) {
					outfile = open(_outFile, O_WRONLY | O_CREAT | O_APPEND, 0777);
				} else {
					outfile = open(_outFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				}
				dup2(outfile, 1);
				close(outfile);
			} else {
				dup2(defaultout, 1);
			}
		} else { // Middle commands
			dup2(fdpipe[cmd_idx - 1][0], 0);
			dup2(fdpipe[cmd_idx][1], 1);			
			close(fdpipe[cmd_idx - 1][0]);
			close(fdpipe[cmd_idx][1]);
		}

		// Fork the process
		pid_t pid = fork();
		if (pid == 0) { // Execute
			execvp(_simpleCommands[cmd_idx]->_arguments[0], _simpleCommands[cmd_idx]->_arguments);
		} else {	   // Restore defaults for the next command
			dup2(defaultin, 0);
			dup2(defaultout, 1);
			last_child_pid = pid;
			if (!_background) {
				waitpid(pid, nullptr, 0);
			}
		}
	}

	close(defaulterr);	
	close(defaultin);
	close(defaultout);
	clear();
	prompt();
}


void
Command::prompt()
{
	printf("myshell>");
	fflush(stdout);
}

Command Command::_currentCommand;
SimpleCommand * Command::_currentSimpleCommand;

int yyparse(void);

int 
main()
{
        setup_signals();
	Command::_currentCommand.prompt();
	yyparse();
	return 0;
}
