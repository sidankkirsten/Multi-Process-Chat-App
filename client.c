/* CSci4061 F2018 Assignment 2
* section: 011
* date: 11/10/18
* name: Ziwen Song, Kirsten Qi
* id: 5329981,5354225 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "comm.h"
#include "util.h"

/* -------------------------Main function for the client ----------------------*/
void main(int argc, char * argv[]) {
	if(argc < 2) {
		printf("Invalid username: Username cannot be empty.\n");
		exit(0);
	} else {
		int pipe_user_reading_from_server[2], pipe_user_writing_to_server[2];

		// You will need to get user name as a parameter, argv[1].

		if(connect_to_server("YOUR_UNIQUE_ID", argv[1], pipe_user_reading_from_server, pipe_user_writing_to_server) == -1) {
			exit(-1);
		}

		/* -------------- YOUR CODE STARTS HERE -----------------------------------*/
		int nread_from_user, nread_from_server;
		char buf_1[MAX_MSG], buf_2[MAX_MSG];

		fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);		// set non-blocking for stdin
		fcntl(pipe_user_reading_from_server[0], F_SETFL, fcntl(pipe_user_reading_from_server[0], F_GETFL, 0) | O_NONBLOCK);		// set non-blocking for pipe

		print_prompt(argv[1]);

		close(pipe_user_writing_to_server[0]);
		close(pipe_user_reading_from_server[1]);

		while (1) {
			/* Poll stdin (input from the terminal) and send it to server (child process) via pipe */
			nread_from_user = read(STDIN_FILENO, buf_1, MAX_MSG);
			if (nread_from_user == -1) {
				if (errno == EAGAIN) {
					usleep(INTERVAL);
				}
				else {
					perror("read failed");
					exit(EXIT_FAILURE);
				}
			}
			else if (nread_from_user == 0) {
				close(STDIN_FILENO);
			}
			else {
				if (buf_1[nread_from_user-1] == '\n') buf_1[nread_from_user-1] = '\0';		// remove "\n"

				write(pipe_user_writing_to_server[1], buf_1, nread_from_user);

				if (get_command_type(buf_1) == EXIT) {		// \exit from user stdin
					close(pipe_user_writing_to_server[1]);
					exit(EXIT_SUCCESS);
				}

				print_prompt(argv[1]);
			}


			/* poll pipe retrieved and print it to stdout */
			nread_from_server = read(pipe_user_reading_from_server[0], buf_2, MAX_MSG);
			if (nread_from_server == -1) {
				if (errno == EAGAIN) {
					usleep(INTERVAL);
				}
				else {
					perror("read failed");
					exit(EXIT_FAILURE);
				}
			}
			else if (nread_from_server == 0) {
				close(pipe_user_reading_from_server[0]);
			}
			else {
				char message[MAX_MSG];

				switch (get_command_type(buf_2)) {
					case EXIT:	// \exit or \kick from server stdin
						printf("The server process seems dead\n");
						close(pipe_user_writing_to_server[1]);

						exit(EXIT_SUCCESS);
						break;
					default:		// message from pipe
						printf("\n%s\n", buf_2);
						memset(buf_2, '\0', MAX_MSG);

						print_prompt(argv[1]);
				}
			}
		}
	}

	/* -------------- YOUR CODE ENDS HERE -----------------------------------*/
}

/*--------------------------End of main for the client --------------------------*/
