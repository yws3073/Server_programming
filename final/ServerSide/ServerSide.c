#include "ServerSide.h"

/* ServerSide */
static void save_state(ServerSide_t *ServerSide);
static void *setup_thread(void *);
static void ServerSide_message(ServerSide_t *ServerSide, char *format, ...);
static void ServerSide_divide(ServerSide_t *ServerSide);

static char *buffer = NULL;

/* Create a new ServerSide_t structure					*/
ServerSide_t *ServerSide_new(conf_t *conf, int count, void *url)
{
	search_t *res;
	ServerSide_t *ServerSide;
	url_t *u;
	char *s;
	int i;

	ServerSide = malloc(sizeof(ServerSide_t));
	memset(ServerSide, 0, sizeof(ServerSide_t));
	*ServerSide->conf = *conf;
	ServerSide->conn = malloc(sizeof(conn_t) * ServerSide->conf->num_connections);
	memset(ServerSide->conn, 0, sizeof(conn_t) * ServerSide->conf->num_connections);
	if (ServerSide->conf->max_speed > 0)
	{
		if ((float)ServerSide->conf->max_speed / ServerSide->conf->buffer_size < 0.5)
		{
			if (ServerSide->conf->verbose >= 2)
				ServerSide_message(ServerSide, _("Buffer resized for this speed."));
			ServerSide->conf->buffer_size = ServerSide->conf->max_speed;
		}
		ServerSide->delay_time = (int)((float)1000000 / ServerSide->conf->max_speed * ServerSide->conf->buffer_size * ServerSide->conf->num_connections);
	}
	if (buffer == NULL)
		buffer = malloc(max(MAX_STRING, ServerSide->conf->buffer_size));

	if (count == 0)
	{
		ServerSide->url = malloc(sizeof(url_t));
		ServerSide->url->next = ServerSide->url;
		strncpy(ServerSide->url->text, (char *)url, MAX_STRING);
	}
	else
	{
		res = (search_t *)url;
		u = ServerSide->url = malloc(sizeof(url_t));
		for (i = 0; i < count; i++)
		{
			strncpy(u->text, res[i].url, MAX_STRING);
			if (i < count - 1)
			{
				u->next = malloc(sizeof(url_t));
				u = u->next;
			}
			else
			{
				u->next = ServerSide->url;
			}
		}
	}

	ServerSide->conn[0].conf = ServerSide->conf;
	if (!conn_set(&ServerSide->conn[0], ServerSide->url->text))
	{
		ServerSide_message(ServerSide, _("Could not parse URL.\n"));
		ServerSide->ready = -1;
		return(ServerSide);
	}

	ServerSide->conn[0].local_if = ServerSide->conf->interfaces->text;
	ServerSide->conf->interfaces = ServerSide->conf->interfaces->next;

	strncpy(ServerSide->filename, ServerSide->conn[0].file, MAX_STRING);
	http_decode(ServerSide->filename);
	if (*ServerSide->filename == 0)	/* Index page == no fn		*/
		strncpy(ServerSide->filename, ServerSide->conf->default_filename, MAX_STRING);
	if ((s = strchr(ServerSide->filename, '?')) != NULL && ServerSide->conf->strip_cgi_parameters)
		*s = 0;		/* Get rid of CGI parameters		*/

	if (!conn_init(&ServerSide->conn[0]))
	{
		ServerSide_message(ServerSide, ServerSide->conn[0].message);
		ServerSide->ready = -1;
		return(ServerSide);
	}

	/* This does more than just checking the file size, it all depends
	on the protocol used.					*/
	if (!conn_info(&ServerSide->conn[0]))
	{
		ServerSide_message(ServerSide, ServerSide->conn[0].message);
		ServerSide->ready = -1;
		return(ServerSide);
	}
	s = conn_url(ServerSide->conn);
	strncpy(ServerSide->url->text, s, MAX_STRING);
	if ((ServerSide->size = ServerSide->conn[0].size) != INT_MAX)
	{
		if (ServerSide->conf->verbose > 0)
			ServerSide_message(ServerSide, _("File size: %lld bytes"), ServerSide->size);
	}

	/* Wildcards in URL --> Get complete filename			*/
	if (strchr(ServerSide->filename, '*') || strchr(ServerSide->filename, '?'))
		strncpy(ServerSide->filename, ServerSide->conn[0].file, MAX_STRING);

	return(ServerSide);
}

/* Open a local file to store the downloaded data			*/
int ServerSide_open(ServerSide_t *ServerSide)
{
	int i, fd;
	long long int j;

	if (ServerSide->conf->verbose > 0)
		ServerSide_message(ServerSide, _("Opening output file %s"), ServerSide->filename);
	snprintf(buffer, MAX_STRING, "%s.st", ServerSide->filename);

	ServerSide->outfd = -1;

	/* Check whether server knows about RESTart and switch back to
	single connection download if necessary			*/
	if (!ServerSide->conn[0].supported)
	{
		ServerSide_message(ServerSide, _("Server unsupported, "
			"starting from scratch with one connection."));
		ServerSide->conf->num_connections = 1;
		ServerSide->conn = realloc(ServerSide->conn, sizeof(conn_t));
		ServerSide_divide(ServerSide);
	}
	else if ((fd = open(buffer, O_RDONLY)) != -1)
	{
		read(fd, &ServerSide->conf->num_connections, sizeof(ServerSide->conf->num_connections));

		ServerSide->conn = realloc(ServerSide->conn, sizeof(conn_t) * ServerSide->conf->num_connections);
		memset(ServerSide->conn + 1, 0, sizeof(conn_t) * (ServerSide->conf->num_connections - 1));

		ServerSide_divide(ServerSide);

		read(fd, &ServerSide->bytes_done, sizeof(ServerSide->bytes_done));
		for (i = 0; i < ServerSide->conf->num_connections; i++)
			read(fd, &ServerSide->conn[i].currentbyte, sizeof(ServerSide->conn[i].currentbyte));

		ServerSide_message(ServerSide, _("State file found: %lld bytes downloaded, %lld to go."),
			ServerSide->bytes_done, ServerSide->size - ServerSide->bytes_done);

		close(fd);

		if ((ServerSide->outfd = open(ServerSide->filename, O_WRONLY, 0666)) == -1)
		{
			ServerSide_message(ServerSide, _("Error opening local file"));
			return(0);
		}
	}

	/* If outfd == -1 we have to start from scrath now		*/
	if (ServerSide->outfd == -1)
	{
		ServerSide_divide(ServerSide);

		if ((ServerSide->outfd = open(ServerSide->filename, O_CREAT | O_WRONLY, 0666)) == -1)
		{
			ServerSide_message(ServerSide, _("Error opening local file"));
			return(0);
		}

		/* And check whether the filesystem can handle seeks to
		past-EOF areas.. Speeds things up. :) AFAIK this
		should just not happen:				*/
		if (lseek(ServerSide->outfd, ServerSide->size, SEEK_SET) == -1 && ServerSide->conf->num_connections > 1)
		{
			/* But if the OS/fs does not allow to seek behind
			EOF, we have to fill the file with zeroes before
			starting. Slow..				*/
			ServerSide_message(ServerSide, _("Crappy filesystem/OS.. Working around. :-("));
			lseek(ServerSide->outfd, 0, SEEK_SET);
			memset(buffer, 0, ServerSide->conf->buffer_size);
			j = ServerSide->size;
			while (j > 0)
			{
				write(ServerSide->outfd, buffer, min(j, ServerSide->conf->buffer_size));
				j -= ServerSide->conf->buffer_size;
			}
		}
	}

	return(1);
}

/* Start downloading							*/
void ServerSide_start(ServerSide_t *ServerSide)
{
	int i;

	/* HTTP might've redirected and FTP handles wildcards, so
	re-scan the URL for every conn				*/
	for (i = 0; i < ServerSide->conf->num_connections; i++)
	{
		conn_set(&ServerSide->conn[i], ServerSide->url->text);
		ServerSide->url = ServerSide->url->next;
		ServerSide->conn[i].local_if = ServerSide->conf->interfaces->text;
		ServerSide->conf->interfaces = ServerSide->conf->interfaces->next;
		ServerSide->conn[i].conf = ServerSide->conf;
		if (i) ServerSide->conn[i].supported = 1;
	}

	if (ServerSide->conf->verbose > 0)
		ServerSide_message(ServerSide, _("Starting download"));

	for (i = 0; i < ServerSide->conf->num_connections; i++)
		if (ServerSide->conn[i].currentbyte <= ServerSide->conn[i].lastbyte)
		{
			if (ServerSide->conf->verbose >= 2)
			{
				ServerSide_message(ServerSide, _("Connection %i downloading from %s:%i using interface %s"),
					i, ServerSide->conn[i].host, ServerSide->conn[i].port, ServerSide->conn[i].local_if);
			}

			ServerSide->conn[i].state = 1;
			if (pthread_create(ServerSide->conn[i].setup_thread, NULL, setup_thread, &ServerSide->conn[i]) != 0)
			{
				ServerSide_message(ServerSide, _("pthread error!!!"));
				ServerSide->ready = -1;
			}
			else
			{
				ServerSide->conn[i].last_transfer = gettime();
			}
		}

	/* The real downloading will start now, so let's start counting	*/
	ServerSide->start_time = gettime();
	ServerSide->ready = 0;
}

/* Main 'loop'								*/
void ServerSide_do(ServerSide_t *ServerSide)
{
	if(rand() % 20 != 1){
		fd_set fds[1];
		int hifd, i;
		long long int remaining, size;
		struct timeval timeval[1];

		/* Create statefile if necessary				*/
		if (gettime() > ServerSide->next_state)
		{
			save_state(ServerSide);
			ServerSide->next_state = gettime() + ServerSide->conf->save_state_interval;
		}

		/* Wait for data on (one of) the connections			*/
		FD_ZERO(fds);
		hifd = 0;
		for (i = 0; i < ServerSide->conf->num_connections; i++)
		{
			if (ServerSide->conn[i].enabled)
				FD_SET(ServerSide->conn[i].fd, fds);
			hifd = max(hifd, ServerSide->conn[i].fd);
		}
		if (hifd == 0)
		{
			/* No connections yet. Wait...				*/
			usleep(100000);
			goto conn_check;
		}
		else
		{
			timeval->tv_sec = 0;
			timeval->tv_usec = 100000;
			/* A select() error probably means it was interrupted
			by a signal, or that something else's very wrong...	*/
			if (select(hifd + 1, fds, NULL, NULL, timeval) == -1)
			{
				ServerSide->ready = -1;
				return;
			}
		}

		/* Handle connections which need attention			*/
		for (i = 0; i < ServerSide->conf->num_connections; i++)
			if (ServerSide->conn[i].enabled) {
				if (FD_ISSET(ServerSide->conn[i].fd, fds))
				{
					ServerSide->conn[i].last_transfer = gettime();
					size = read(ServerSide->conn[i].fd, buffer, ServerSide->conf->buffer_size);
					if (size == -1)
					{
						if (ServerSide->conf->verbose)
						{
							ServerSide_message(ServerSide, _("Error on connection %i! "
								"Connection closed"), i);
						}
						ServerSide->conn[i].enabled = 0;
						conn_disconnect(&ServerSide->conn[i]);
						continue;
					}
					else if (size == 0)
					{
						if (ServerSide->conf->verbose)
						{
							/* Only abnormal behaviour if:		*/
							if (ServerSide->conn[i].currentbyte < ServerSide->conn[i].lastbyte && ServerSide->size != INT_MAX)
							{
								ServerSide_message(ServerSide, _("Connection %i unexpectedly closed"), i);
							}
							else
							{
								ServerSide_message(ServerSide, _("Connection %i finished"), i);
							}
						}
						if (!ServerSide->conn[0].supported)
						{
							ServerSide->ready = 1;
						}
						ServerSide->conn[i].enabled = 0;
						conn_disconnect(&ServerSide->conn[i]);
						continue;
					}
					/* remaining == Bytes to go					*/
					remaining = ServerSide->conn[i].lastbyte - ServerSide->conn[i].currentbyte + 1;
					if (remaining < size)
					{
						if (ServerSide->conf->verbose)
						{
							ServerSide_message(ServerSide, _("Connection %i finished"), i);
						}
						ServerSide->conn[i].enabled = 0;
						conn_disconnect(&ServerSide->conn[i]);
						size = remaining;
						/* Don't terminate, still stuff to write!	*/
					}
					/* This should always succeed..				*/
					lseek(ServerSide->outfd, ServerSide->conn[i].currentbyte, SEEK_SET);
					if (write(ServerSide->outfd, buffer, size) != size)
					{

						ServerSide_message(ServerSide, _("Write error!"));
						ServerSide->ready = -1;
						return;
					}
					ServerSide->conn[i].currentbyte += size;
					ServerSide->bytes_done += size;
				}
				else
				{
					if (gettime() > ServerSide->conn[i].last_transfer + ServerSide->conf->connection_timeout)
					{
						if (ServerSide->conf->verbose)
							ServerSide_message(ServerSide, _("Connection %i timed out"), i);
						conn_disconnect(&ServerSide->conn[i]);
						ServerSide->conn[i].enabled = 0;
					}
				}
			}

		if (ServerSide->ready)
			return;

	conn_check:
		/* Look for aborted connections and attempt to restart them.	*/
		for (i = 0; i < ServerSide->conf->num_connections; i++)
		{
			if (!ServerSide->conn[i].enabled && ServerSide->conn[i].currentbyte < ServerSide->conn[i].lastbyte)
			{
				if (ServerSide->conn[i].state == 0)
				{
					// Wait for termination of this thread
					pthread_join(*(ServerSide->conn[i].setup_thread), NULL);

					conn_set(&ServerSide->conn[i], ServerSide->url->text);
					ServerSide->url = ServerSide->url->next;
					/* ServerSide->conn[i].local_if = ServerSide->conf->interfaces->text;
					ServerSide->conf->interfaces = ServerSide->conf->interfaces->next; */
					if (ServerSide->conf->verbose >= 2)
						ServerSide_message(ServerSide, _("Connection %i downloading from %s:%i using interface %s"),
							i, ServerSide->conn[i].host, ServerSide->conn[i].port, ServerSide->conn[i].local_if);

					ServerSide->conn[i].state = 1;
					if (pthread_create(ServerSide->conn[i].setup_thread, NULL, setup_thread, &ServerSide->conn[i]) == 0)
					{
						ServerSide->conn[i].last_transfer = gettime();
					}
					else
					{
						ServerSide_message(ServerSide, _("pthread error!!!"));
						ServerSide->ready = -1;
					}
				}
				else
				{
					if (gettime() > ServerSide->conn[i].last_transfer + ServerSide->conf->reconnect_delay)
					{
						pthread_cancel(*ServerSide->conn[i].setup_thread);
						ServerSide->conn[i].state = 0;
					}
				}
			}
		}

		/* Calculate current average speed and finish_time		*/
		ServerSide->bytes_per_second = (int)((double)(ServerSide->bytes_done - ServerSide->start_byte) / (gettime() - ServerSide->start_time));
		ServerSide->finish_time = (int)(ServerSide->start_time + (double)(ServerSide->size - ServerSide->start_byte) / ServerSide->bytes_per_second);

		/* Ready?							*/
		if (ServerSide->bytes_done == ServerSide->size)
			ServerSide->ready = 1;
	}
}

/* Close an ServerSide connection						*/
void ServerSide_close(ServerSide_t *ServerSide)
{
	int i;
	message_t *m;

	/* Terminate any thread still running				*/
	for (i = 0; i < ServerSide->conf->num_connections; i++)
		/* don't try to kill non existing thread */
		if (*ServerSide->conn[i].setup_thread != 0)
			pthread_cancel(*ServerSide->conn[i].setup_thread);

	/* Delete state file if necessary				*/
	if (ServerSide->ready == 1)
	{
		snprintf(buffer, MAX_STRING, "%s.st", ServerSide->filename);
		unlink(buffer);
	}
	/* Else: Create it.. 						*/
	else if (ServerSide->bytes_done > 0)
	{
		save_state(ServerSide);
	}

	/* Delete any message not processed yet				*/
	while (ServerSide->message)
	{
		m = ServerSide->message;
		ServerSide->message = ServerSide->message->next;
		free(m);
	}

	/* Close all connections and local file				*/
	close(ServerSide->outfd);
	for (i = 0; i < ServerSide->conf->num_connections; i++)
		conn_disconnect(&ServerSide->conn[i]);

	free(ServerSide->conn);
	free(ServerSide);
}

/* time() with more precision						*/
double gettime()
{
	struct timeval time[1];

	gettimeofday(time, 0);
	return((double)time->tv_sec + (double)time->tv_usec / 1000000);
}

/* Save the state of the current download				*/
void save_state(ServerSide_t *ServerSide)
{
	int fd, i;
	char fn[MAX_STRING + 4];

	/* No use for such a file if the server doesn't support
	resuming anyway..						*/
	if (!ServerSide->conn[0].supported)
		return;

	snprintf(fn, MAX_STRING, "%s.st", ServerSide->filename);
	if ((fd = open(fn, O_CREAT | O_TRUNC | O_WRONLY, 0666)) == -1)
	{
		return;		/* Not 100% fatal..			*/
	}
	write(fd, &ServerSide->conf->num_connections, sizeof(ServerSide->conf->num_connections));
	write(fd, &ServerSide->bytes_done, sizeof(ServerSide->bytes_done));
	for (i = 0; i < ServerSide->conf->num_connections; i++)
	{
		write(fd, &ServerSide->conn[i].currentbyte, sizeof(ServerSide->conn[i].currentbyte));
	}
	close(fd);
}

/* Thread used to set up a connection					*/
void *setup_thread(void *c)
{
	conn_t *conn = c;
	int oldstate;

	/* Allow this thread to be killed at any time.			*/
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);

	if (conn_setup(conn))
	{
		conn->last_transfer = gettime();
		if (conn_exec(conn))
		{
			conn->last_transfer = gettime();
			conn->enabled = 1;
			conn->state = 0;
			return(NULL);
		}
	}

	conn_disconnect(conn);
	conn->state = 0;
	return(NULL);
}

/* Add a message to the ServerSide->message structure				*/
static void ServerSide_message(ServerSide_t *ServerSide, char *format, ...)
{
	message_t *m = malloc(sizeof(message_t)), *n = ServerSide->message;
	va_list params;

	memset(m, 0, sizeof(message_t));
	va_start(params, format);
	vsnprintf(m->text, MAX_STRING, format, params);
	va_end(params);

	if (ServerSide->message == NULL)
	{
		ServerSide->message = m;
	}
	else
	{
		while (n->next != NULL)
			n = n->next;
		n->next = m;
	}
}

/* Divide the file and set the locations for each connection		*/
static void ServerSide_divide(ServerSide_t *ServerSide)
{
	int i;

	ServerSide->conn[0].currentbyte = 0;
	ServerSide->conn[0].lastbyte = ServerSide->size / ServerSide->conf->num_connections - 1;
}