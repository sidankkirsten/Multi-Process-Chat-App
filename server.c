/* CSci4061 F2018 Assignment 2
* section: 011
* date: 11/10/18
* name: Ziwen Song, Kirsten Qi
* id: 5329981,5354225 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "comm.h"
#include "util.h"

/* -----------Functions that implement server functionality -------------------------*/

/*
 * Returns the empty slot on success, or -1 on failure
 */
int find_empty_slot(USER * user_list) {
	// iterate through the user_list and check m_status to see if any slot is EMPTY
	// return the index of the empty slot
    int i = 0;
	for(i=0;i<MAX_USER;i++) {
    	if(user_list[i].m_status == SLOT_EMPTY) {
			return i;
		}
	}
	return -1;
}

/*
 * list the existing users on the server shell
 */
int list_users(int idx, USER * user_list)
{
	// iterate through the user list
	// if you find any slot which is not empty, print that m_user_id
	// if every slot is empty, print "<no users>""
	// If the function is called by the server (that is, idx is -1), then printf the list
	// If the function is called by the user, then send the list to the user using write() and passing m_fd_to_user
	// return 0 on success
	int i, flag = 0;
	char buf[MAX_MSG], *s = NULL;

	/* construct a list of user names */
	s = buf;
	strncpy(s, "---connecetd user list---\n", strlen("---connecetd user list---\n"));
	s += strlen("---connecetd user list---\n");
	for (i = 0; i < MAX_USER; i++) {
		if (user_list[i].m_status == SLOT_EMPTY)
			continue;
		flag = 1;
		strncpy(s, user_list[i].m_user_id, strlen(user_list[i].m_user_id));
		s = s + strlen(user_list[i].m_user_id);
		strncpy(s, "\n", 1);
		s++;
	}
	if (flag == 0) {
		strcpy(buf, "<no users>\n");
	} else {
		s--;
		strncpy(s, "\0", 1);
	}

	if(idx < 0) {
		printf("%s", buf);
		printf("\n");
	} else {
		/* write to the given pipe fd */
		if (write(user_list[idx].m_fd_to_user, buf, strlen(buf) + 1) < 0)
			perror("writing to server shell");
	}

	return 0;
}

/*
 * add a new user
 */
int add_user(int idx, USER * user_list, int pid, char * user_id, int pipe_to_child, int pipe_to_parent)
{
	// populate the user_list structure with the arguments passed to this function
	// return the index of user added
	user_list[idx].m_pid = pid;
	strcpy(user_list[idx].m_user_id, user_id);
	user_list[idx].m_fd_to_user = pipe_to_child;
	user_list[idx].m_fd_to_server = pipe_to_parent;
	user_list[idx].m_status = SLOT_FULL;

	return idx;
}

/*
 * Kill a user
 */
void kill_user(int idx, USER * user_list) {
	// kill a user (specified by idx) by using the systemcall kill()
	// then call waitpid on the user
	int pid = user_list[idx].m_pid;
	int status, ret;

	while (1) {
		ret = waitpid(pid, &status, WNOHANG);

		if (ret == pid) break;

		kill(pid, SIGQUIT);
	}
}

/*
 * Perform cleanup actions after the used has been killed
 */
void cleanup_user(int idx, USER * user_list)
{
	// m_pid should be set back to -1
	// m_user_id should be set to zero, using memset()
	// close all the fd
	// set the value of all fd back to -1
	// set the status back to empty
	user_list[idx].m_pid = -1;
	memset(user_list[idx].m_user_id, '\0', MAX_USER_ID);
	close(user_list[idx].m_fd_to_user);
	close(user_list[idx].m_fd_to_server);
	user_list[idx].m_fd_to_user = -1;
	user_list[idx].m_fd_to_server = -1;
	user_list[idx].m_status = SLOT_EMPTY;
}

/*
 * Kills the user and performs cleanup
 */
void kick_user(int idx, USER * user_list) {
	// should kill_user()
	// then perform cleanup_user()
	kill_user(idx, user_list);
	cleanup_user(idx, user_list);
}

/*
 * broadcast message to all users
 */
int broadcast_msg(USER * user_list, char *buf, char *sender)
{
	//iterate over the user_list and if a slot is full, and the user is not the sender itself,
	//then send the message to that user
	//return zero on success
	int i;
	for (i = 0; i < MAX_USER; i++) {
		if (user_list[i].m_status == SLOT_FULL) {
			char message[MAX_MSG];
			if (strcmp(sender, "admin") == 0) {
				sprintf(message, "admin-notice: %s", buf);
			}
			else {
				if (strcmp(sender, user_list[i].m_user_id) == 0) continue;
				strcpy(message, buf);
			}

			if (write(user_list[i].m_fd_to_user, message, MAX_MSG) == -1) {
				perror("write failed");
				return -1;
			}
		}
	}

	return 0;
}

/*
 * Cleanup user chat boxes
 */
void cleanup_users(USER * user_list)
{
	// go over the user list and check for any empty slots
	// call cleanup user for each of those users.
	int i;
	for (i = 0; i < MAX_USER; i++) {
		if (user_list[i].m_status == SLOT_FULL) {
			cleanup_user(i, user_list);
		}
	}
}

/*
 * find user index for given user name
 */
int find_user_index(USER * user_list, char * user_id)
{
	// go over the  user list to return the index of the user which matches the argument user_id
	// return -1 if not found
	int i, user_idx = -1;

	if (user_id == NULL) {
		fprintf(stderr, "NULL name passed.\n");
		return user_idx;
	}
	for (i=0;i<MAX_USER;i++) {
		if (user_list[i].m_status == SLOT_EMPTY)
			continue;
		if (strcmp(user_list[i].m_user_id, user_id) == 0) {
			return i;
		}
	}

	return -1;
}

/*
 * given a command's input buffer, extract name
 */
int extract_name(char * buf, char * user_name)
{
	char inbuf[MAX_MSG];
    char * tokens[16];
    strcpy(inbuf, buf);

    int token_cnt = parse_line(inbuf, tokens, " ");

    if(token_cnt >= 2) {
        strcpy(user_name, tokens[1]);
        return 0;
    }

    return -1;
}

int extract_text(char *buf, char * text)
{
    char inbuf[MAX_MSG];
    char * tokens[16];
    char * s = NULL;
    strcpy(inbuf, buf);

    int token_cnt = parse_line(inbuf, tokens, " ");

    if(token_cnt >= 3) {
        //Find " "
        s = strchr(buf, ' ');
        s = strchr(s+1, ' ');

        strcpy(text, s+1);
        return 0;
    }

    return -1;
}

/*
 * send personal message
 */
void send_p2p_msg(int idx, USER * user_list, char *buf)
{
	// get the target user by name using extract_name() function
	// find the user id using find_user_index()
	// if user not found, write back to the original user "User not found", using the write() function on pipes.
	// if the user is found then write the message that the user wants to send to that user.
	char target_user[MAX_USER_ID], message[MAX_MSG];

	if (extract_name(buf, target_user) == -1) {		// extract the user id and it was not given
		sprintf(message, "User was not given");
		if (write(user_list[idx].m_fd_to_user, message, MAX_MSG) == -1) {
			perror("write failed");
			exit(EXIT_FAILURE);
		}
	}
	else {		// the user id was given
		int target_idx;
		if ((target_idx = find_user_index(user_list, target_user)) == -1) {		// user not found
			sprintf(message, "User not found");
			if (write(user_list[idx].m_fd_to_user, message, MAX_MSG) == -1) {
				perror("write failed");
				exit(EXIT_FAILURE);
			}
		}
		else {		// user found
			char text[MAX_MSG];
			if (extract_text(buf, text) == -1) {	// message is not given
				sprintf(message, "Message is not given");
				if (write(user_list[idx].m_fd_to_user, message, MAX_MSG) == -1) {
					perror("write failed");
					exit(EXIT_FAILURE);
				}
			}
			else {		// message is given and send it to the target user
				sprintf(message, "%s: %s", user_list[idx].m_user_id, text);
				if (write(user_list[target_idx].m_fd_to_user, message, MAX_MSG) == -1) {
					perror("write failed");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
}


/*
 * Populates the user list initially
 */
void init_user_list(USER * user_list)
{
	//iterate over the MAX_USER
	//memset() all m_user_id to zero
	//set all fd to -1
	//set the status to be EMPTY
	int i=0;
	for(i=0;i<MAX_USER;i++) {
		user_list[i].m_pid = -1;
		memset(user_list[i].m_user_id, '\0', MAX_USER_ID);
		user_list[i].m_fd_to_user = -1;
		user_list[i].m_fd_to_server = -1;
		user_list[i].m_status = SLOT_EMPTY;
	}
}

/*
 * Send exit command to user
 */
void send_exit_message(int fd_to_user)
{
	char buf[MAX_MSG];
	sprintf(buf, "\\exit");
	if (write(fd_to_user, buf, MAX_MSG) == -1) {
		perror("write failed");
		exit(EXIT_FAILURE);
	}
}


void server_handle_commands(USER *user_list)
{
	int nread_from_stdin, nread_from_user;
	char buf_1[MAX_MSG], buf_2[MAX_MSG];

	int i;

	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);		// set non-blocking for stdin

	/* handle server commands */
	nread_from_stdin = read(STDIN_FILENO, buf_1, MAX_MSG);
	if (nread_from_stdin == -1) {
		if (errno == EAGAIN) {
			usleep(INTERVAL);
		}
	}
	else if (nread_from_stdin == 0) {
		// close(STDIN_FILENO);
	}
	else {
		if (buf_1[nread_from_stdin-1] == '\n') buf_1[nread_from_stdin-1] = '\0';		// remove "\n"

		char kicked_user_id[MAX_USER_ID];
		extract_name(buf_1, kicked_user_id);	// get the username
		int kicked_idx = find_user_index(user_list, kicked_user_id);	// get the index of specific user

		switch (get_command_type(buf_1)) {		// server process commands
			case LIST:		// \list
				list_users(-1, user_list);
				break;
			case KICK:		// \kick username
				if (kicked_idx == -1) {   // kicked_idx is not in user list
					printf("Cannot find user: %s\n", kicked_user_id);
				}
				else {
					send_exit_message(user_list[kicked_idx].m_fd_to_user);
					usleep(INTERVAL*3);
					kick_user(kicked_idx, user_list);
				}
				break;
			case EXIT:		// \exit
				for (i = 0; i < MAX_USER; i++) {
					if (user_list[i].m_status == SLOT_EMPTY) continue;

					send_exit_message(user_list[i].m_fd_to_user);
          usleep(INTERVAL*3);
          kick_user(i, user_list);
				}

				exit(EXIT_SUCCESS);
				break;
			default:		// any-other-text
				broadcast_msg(user_list, buf_1, "admin");
		}

		print_prompt("admin");
	}


	/* handle user commands */
	for (i = 0; i < MAX_USER; i++) {
		if (user_list[i].m_status == SLOT_EMPTY) continue;

		fcntl(user_list[i].m_fd_to_server, F_SETFL, fcntl(user_list[i].m_fd_to_server, F_GETFL, 0) | O_NONBLOCK);	// set non-blocking

		nread_from_user = read(user_list[i].m_fd_to_server, buf_2, MAX_MSG);
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
			close(user_list[i].m_fd_to_server);
		}
		else {
			switch (get_command_type(buf_2)) {		// user process commands
				case LIST:		// \list
					list_users(i, user_list);
          printf("List\n");
					print_prompt("admin");
					break;
				case P2P:		// \p2p
					send_p2p_msg(i, user_list, buf_2);
					break;
				case EXIT:		// \exit
          printf("\nThe user: %s seems to be terminated.\n", user_list[i].m_user_id);
					kick_user(i, user_list);
					print_prompt("admin");
					break;
				default:		// any-other-text
					broadcast_msg(user_list, buf_2, user_list[i].m_user_id);
			}
		}
	}

}


// poll child processes and handle user commands
void child_handle_message(int pipe_child_reading_from_user[2], int pipe_child_writing_to_user[2], int pipe_SERVER_reading_from_child[2], int pipe_SERVER_writing_to_child[2])
{
	int nread_from_server, nread_from_user;
	char buf_1[MAX_MSG], buf_2[MAX_MSG];

	fcntl(pipe_SERVER_writing_to_child[0], F_SETFL, fcntl(pipe_SERVER_writing_to_child[0], F_GETFL, 0) | O_NONBLOCK);	// set non-blocking for pipe
	fcntl(pipe_child_reading_from_user[0], F_SETFL, fcntl(pipe_child_reading_from_user[0], F_GETFL, 0) | O_NONBLOCK);	// set non-blocking	for pipe

	close(pipe_SERVER_writing_to_child[1]);
	close(pipe_SERVER_reading_from_child[0]);
	close(pipe_child_writing_to_user[0]);
	close(pipe_child_reading_from_user[1]);

	while (1) {
		// forward message from SERVER to user
		nread_from_server = read(pipe_SERVER_writing_to_child[0], buf_1, MAX_MSG);    // read message from server
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
			close(pipe_SERVER_writing_to_child[0]);
			break;
		}
		else {
			write(pipe_child_writing_to_user[1], buf_1, nread_from_server);    // write message to user
		}

		/* forward message from user to SERVER */
		nread_from_user = read(pipe_child_reading_from_user[0], buf_2, MAX_MSG);    // read message from user
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
			close(pipe_child_reading_from_user[0]);
			break;
		}
		else {
			write(pipe_SERVER_reading_from_child[1], buf_2, nread_from_user);    // write message to server
		}

	}

	close(pipe_child_writing_to_user[1]);
	close(pipe_SERVER_reading_from_child[1]);
}


/* ---------------------End of the functions that implementServer functionality -----------------*/


/* ---------------------Start of the Main function ----------------------------------------------*/
int main(int argc, char * argv[])
{
	int nbytes;
	setup_connection("YOUR_UNIQUE_ID"); // Specifies the connection point as argument.

	USER user_list[MAX_USER];
	init_user_list(user_list);   // Initialize user list

	char buf[MAX_MSG];
	fcntl(0, F_SETFL, fcntl(0, F_GETFL)| O_NONBLOCK);
	print_prompt("admin");

	int idx = -1;

	while(1) {
		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/
		int pipe_SERVER_reading_from_child[2];
		int pipe_SERVER_writing_to_child[2];
		int pipe_child_writing_to_user[2];
		int pipe_child_reading_from_user[2];
		char user_id[MAX_USER_ID];

		// Handling a new connection using get_connection
		if (get_connection(user_id, pipe_child_writing_to_user, pipe_child_reading_from_user) != -1) {		// there is new user connection
			if ((idx = find_user_index(user_list, user_id)) == -1) {	// check user id, user is not in the user list
				if ((idx = find_empty_slot(user_list)) != -1) {		// check max user, there is a empty slot

					// the SERVER create two pipes for bidirectional communication with a child process
					if (pipe(pipe_SERVER_writing_to_child) < 0 || pipe(pipe_SERVER_reading_from_child) < 0) {
						perror("Failed to create pipe");
						exit(EXIT_FAILURE);
					}

					pid_t child_pid;
					if ((child_pid = fork()) == 0) { 	// Child process: poll users and SERVER; forward message bewteen SERVER and user
						child_handle_message(pipe_child_reading_from_user, pipe_child_writing_to_user, pipe_SERVER_reading_from_child, pipe_SERVER_writing_to_child);
					}
					else if (child_pid > 0) {	// Server process: Add a new user information into an empty slot; poll stdin (input from the terminal) and handle admnistrative command
						add_user(idx, user_list, child_pid, user_id, pipe_SERVER_writing_to_child[1], pipe_SERVER_reading_from_child[0]);
						printf("\nA new user: %s connected. slot: %d\n", user_id, idx);

						print_prompt("admin");

						server_handle_commands(user_list);
					}
					else {
						perror("fork failed");
						exit(EXIT_FAILURE);
					}

				}
				else {	// there is no empty slot
					fprintf(stderr, "\nNo empty slot.\n");
					send_exit_message(pipe_child_writing_to_user[1]);

					print_prompt("admin");
				}
			}
			else {		// user is in the user list
				fprintf(stderr, "\nUser id: %s already taken.\n", user_list[idx].m_user_id);
				send_exit_message(pipe_child_writing_to_user[1]);

				print_prompt("admin");
			}
		}
		else {
			server_handle_commands(user_list);
		}

		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/
	}
}

/* --------------------End of the main function ----------------------------------------*/
