#define MAX_QUERY	2048		/* Should not grow larger..	*/

typedef struct
{
	char host[MAX_STRING];
	char auth[MAX_STRING];
	char request[MAX_QUERY];
	char headers[MAX_QUERY];
	int proto;			/* FTP through HTTP proxies	*/
	int proxy;
	long long int firstbyte;
	long long int lastbyte;
	int status;
	int fd;
	char *local_if;
} http_t;

int http_connect(http_t *conn, int proto, char *proxy, char *host, int port, char *user, char *pass);
void http_disconnect(http_t *conn);
void http_get(http_t *conn, char *lurl);
void http_addheader(http_t *conn, char *format, ...);
int http_exec(http_t *conn);
char *http_header(http_t *conn, char *header);
long long int http_size(http_t *conn);
void http_encode(char *s);
void http_decode(char *s);