#include "config.h"

#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <netdb.h>
#ifndef	NOGETOPTLONG
#define _GNU_SOURCE
#include <getopt.h>
#endif
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <pthread.h>

/* Internationalization							*/
#ifdef I18N
#define PACKAGE			"ServerSide"
#define _( x )			gettext( x )
#include <libintl.h>
#include <locale.h>
#else
#define _( x )			x
#endif

/* Compiled-in settings							*/
#define MAX_STRING		1024
#define MAX_ADD_HEADERS	10
#define MAX_REDIR		5
#define ServerSide_VERSION_STRING	"2.4"
#define DEFAULT_USER_AGENT	"ServerSide " ServerSide_VERSION_STRING " (" ARCH ")"

typedef struct
{
	void *next;
	char text[MAX_STRING];
} message_t;

typedef message_t url_t;
typedef message_t if_t;

#include "conf.h"
#include "tcp.h"
#include "ftp.h"
#include "http.h"
#include "conn.h"
#include "search.h"

#define min( a, b )		( (a) < (b) ? (a) : (b) )
#define max( a, b )		( (a) > (b) ? (a) : (b) )

typedef struct
{
	conn_t *conn;
	conf_t conf[1];
	char filename[MAX_STRING];
	double start_time;
	int next_state, finish_time;
	long long bytes_done, start_byte, size;
	int bytes_per_second;
	int delay_time;
	int outfd;
	int ready;
	message_t *message;
	url_t *url;
} ServerSide_t;

ServerSide_t *ServerSide_new(conf_t *conf, int count, void *url);
int ServerSide_open(ServerSide_t *ServerSide);
void ServerSide_start(ServerSide_t *ServerSide);
void ServerSide_do(ServerSide_t *ServerSide);
void ServerSide_close(ServerSide_t *ServerSide);

double gettime();