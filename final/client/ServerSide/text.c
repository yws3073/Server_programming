#include "ServerSide.h"
#include <time.h>

static void stop(int signal);
static char *size_human(long long int value);
static char *time_human(int value);
static void print_commas(long long int bytes_done);
static void print_alternate_output(ServerSide_t *ServerSide);
static void print_help();
static void print_version();
static void print_messages(ServerSide_t *ServerSide);

int run = 1;

#ifdef NOGETOPTLONG
#define getopt_long( a, b, c, d, e ) getopt( a, b, c )
#else
static struct option ServerSide_options[] =
{
	/* name			has_arg	flag	val */
	{ "max-speed",		1,	NULL,	's' },
	{ "num-connections",	1,	NULL,	'n' },
	{ "output",		1,	NULL,	'o' },
	{ "search",		2,	NULL,	'S' },
	{ "no-proxy",		0,	NULL,	'N' },
	{ "quiet",		0,	NULL,	'q' },
	{ "verbose",		0,	NULL,	'v' },
	{ "help",		0,	NULL,	'h' },
	{ "version",		0,	NULL,	'V' },
	{ "alternate",		0,	NULL,	'a' },
	{ "header",		1,	NULL,	'H' },
	{ "user-agent",		1,	NULL,	'U' },
	{ NULL,			0,	NULL,	0 }
};
#endif

/* For returning string values from functions				*/
static char string[MAX_STRING];


int main(int argc, char *argv[])
{
	char fn[MAX_STRING] = "";
	int do_search = 0;
	search_t *search;
	conf_t conf[1];
	ServerSide_t *ServerSide;
	int i, j, cur_head = 0;
	char *s;

#ifdef I18N
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALE);
	textdomain(PACKAGE);
#endif
	srand((unsigned)time(NULL));

	if (!conf_init(conf))
	{
		return(1);
	}

	opterr = 0;

	j = -1;
	while (1)
	{
		int option;

		option = getopt_long(argc, argv, "s:n:o:S::NqvhVaH:U:", ServerSide_options, NULL);
		if (option == -1)
			break;

		switch (option)
		{
		case 'U':
			strncpy(conf->user_agent, optarg, MAX_STRING);
			break;
		case 'H':
			strncpy(conf->add_header[cur_head++], optarg, MAX_STRING);
			break;
		case 's':
			if (!sscanf(optarg, "%i", &conf->max_speed))
			{
				print_help();
				return(1);
			}
			break;
		case 'n':
			if (!sscanf(optarg, "%i", &conf->num_connections))
			{
				print_help();
				return(1);
			}
			break;
		case 'o':
			strncpy(fn, optarg, MAX_STRING);
			break;
		case 'S':
			do_search = 1;
			if (optarg != NULL)
				if (!sscanf(optarg, "%i", &conf->search_top))
				{
					print_help();
					return(1);
				}
			break;
		case 'a':
			conf->alternate_output = 1;
			break;
		case 'N':
			*conf->http_proxy = 0;
			break;
		case 'h':
			print_help();
			return(0);
		case 'v':
			if (j == -1)
				j = 1;
			else
				j++;
			break;
		case 'V':
			print_version();
			return(0);
		case 'q':
			close(1);
			conf->verbose = -1;
			if (open("/dev/null", O_WRONLY) != 1)
			{
				fprintf(stderr, _("Can't redirect stdout to /dev/null.\n"));
				return(1);
			}
			break;
		default:
			print_help();
			return(1);
		}
	}
	conf->add_header_count = cur_head;
	if (j > -1)
		conf->verbose = j;

	if (argc - optind == 0)
	{
		print_help();
		return(1);
	}
	else if (strcmp(argv[optind], "-") == 0)
	{
		s = malloc(MAX_STRING);
		if (scanf("%1024[^\n]s", s) != 1) {
			fprintf(stderr, _("Error when trying to read URL (Too long?).\n"));
			return(1);
		}
	}
	else
	{
		s = argv[optind];
		if (strlen(s) > MAX_STRING)
		{
			fprintf(stderr, _("Can't handle URLs of length over %d\n"), MAX_STRING);
			return(1);
		}
	}

	printf(_("Initializing download: %s\n"), s);
	if (do_search)
	{
		search = malloc(sizeof(search_t) * (conf->search_amount + 1));
		memset(search, 0, sizeof(search_t) * (conf->search_amount + 1));
		search[0].conf = conf;
		if (conf->verbose)
			printf(_("Doing search...\n"));
		i = search_makelist(search, s);
		if (i < 0)
		{
			fprintf(stderr, _("File not found\n"));
			return(1);
		}
		if (conf->verbose)
			printf(_("Testing speeds, this can take a while...\n"));
		j = search_getspeeds(search, i);
		search_sortlist(search, i);
		if (conf->verbose)
		{
			printf(_("%i usable servers found, will use these URLs:\n"), j);
			j = min(j, conf->search_top);
			printf("%-60s %15s\n", "URL", "Speed");
			for (i = 0; i < j; i++)
				printf("%-70.70s %5i\n", search[i].url, search[i].speed);
			printf("\n");
		}
		ServerSide = ServerSide_new(conf, j, search);
		free(search);
		if (ServerSide->ready == -1)
		{
			print_messages(ServerSide);
			ServerSide_close(ServerSide);
			return(1);
		}
	}
	else if (argc - optind == 1)
	{
		ServerSide = ServerSide_new(conf, 0, s);
		if (ServerSide->ready == -1)
		{
			print_messages(ServerSide);
			ServerSide_close(ServerSide);
			return(1);
		}
	}
	else
	{
		search = malloc(sizeof(search_t) * (argc - optind));
		memset(search, 0, sizeof(search_t) * (argc - optind));
		for (i = 0; i < (argc - optind); i++)
			strncpy(search[i].url, argv[optind + i], MAX_STRING);
		ServerSide = ServerSide_new(conf, argc - optind, search);
		free(search);
		if (ServerSide->ready == -1)
		{
			print_messages(ServerSide);
			ServerSide_close(ServerSide);
			return(1);
		}
	}
	print_messages(ServerSide);
	if (s != argv[optind])
	{
		free(s);
	}

	if (*fn)
	{
		struct stat buf;

		if (stat(fn, &buf) == 0)
		{
			if (S_ISDIR(buf.st_mode))
			{
				size_t fnlen = strlen(fn);
				size_t ServerSidefnlen = strlen(ServerSide->filename);

				if (fnlen + 1 + ServerSidefnlen + 1 > MAX_STRING) {
					fprintf(stderr, _("Filename too long!\n"));
					return (1);
				}

				fn[fnlen] = '/';
				memcpy(fn + fnlen + 1, ServerSide->filename, ServerSidefnlen);
				fn[fnlen + 1 + ServerSidefnlen] = '\0';
			}
		}
		sprintf(string, "%s.st", fn);
		if (access(fn, F_OK) == 0) if (access(string, F_OK) != 0)
		{
			fprintf(stderr, _("No state file, cannot resume!\n"));
			return(1);
		}
		if (access(string, F_OK) == 0) if (access(fn, F_OK) != 0)
		{
			printf(_("State file found, but no downloaded data. Starting from scratch.\n"));
			unlink(string);
		}
		strcpy(ServerSide->filename, fn);
	}
	else
	{
		/* Local file existence check					*/
		i = 0;
		s = ServerSide->filename + strlen(ServerSide->filename);
		while (1)
		{
			sprintf(string, "%s.st", ServerSide->filename);
			if (access(ServerSide->filename, F_OK) == 0)
			{
				if (ServerSide->conn[0].supported)
				{
					if (access(string, F_OK) == 0)
						break;
				}
			}
			else
			{
				if (access(string, F_OK))
					break;
			}
			sprintf(s, ".%i", i);
			i++;
		}
	}

	if (!ServerSide_open(ServerSide))
	{
		print_messages(ServerSide);
		return(1);
	}
	print_messages(ServerSide);
	ServerSide_start(ServerSide);
	print_messages(ServerSide);

	if (conf->alternate_output)
	{
		putchar('\n');
	}
	else
	{
		if (ServerSide->bytes_done > 0)	/* Print first dots if resuming	*/
		{
			putchar('\n');
			print_commas(ServerSide->bytes_done);
		}
	}
	ServerSide->start_byte = ServerSide->bytes_done;

	/* Install save_state signal handler for resuming support	*/
	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	while (!ServerSide->ready && run)
	{
		long long int prev, done;

		prev = ServerSide->bytes_done;
		ServerSide_do(ServerSide);

		if (conf->alternate_output)
		{
			if (!ServerSide->message && prev != ServerSide->bytes_done)
				print_alternate_output(ServerSide);
		}
		else
		{
			/* The infamous wget-like 'interface'.. ;)		*/
			done = (ServerSide->bytes_done / 1024) - (prev / 1024);
			if (done && conf->verbose > -1)
			{
				for (i = 0; i < done; i++)
				{
					i += (prev / 1024);
					if ((i % 50) == 0)
					{
						if (prev >= 1024)
							printf("  [%6.1fKB/s]\n", (double)ServerSide->bytes_per_second / 1024);
					}
					else if ((i % 10) == 0)
					{
						putchar(' ');
					}
					putchar('.');
					i -= (prev / 1024);
				}
				fflush(stdout);
			}
		}

		if (ServerSide->message)
		{
			if (conf->alternate_output == 1)
			{
				/* clreol-simulation */
				putchar('\r');
				for (i = 0; i < 79; i++) /* linewidth known? */
					putchar(' ');
				putchar('\r');
			}
			else
			{
				putchar('\n');
			}
			print_messages(ServerSide);
			if (!ServerSide->ready)
			{
				if (conf->alternate_output != 1)
					print_commas(ServerSide->bytes_done);
				else
					print_alternate_output(ServerSide);
			}
		}
		else if (ServerSide->ready)
		{
			putchar('\n');
		}
	}

	strcpy(string + MAX_STRING / 2,
		size_human(ServerSide->bytes_done - ServerSide->start_byte));

	printf(_("\nDownloaded %s in %s. (%.2f KB/s)\n"),
		string + MAX_STRING / 2,
		time_human(gettime() - ServerSide->start_time),
		(double)ServerSide->bytes_per_second / 1024);

	i = ServerSide->ready ? 0 : 2;

	ServerSide_close(ServerSide);

	return(i);
}

/* SIGINT/SIGTERM handler						*/
void stop(int signal)
{
	run = 0;
}

/* Convert a number of bytes to a human-readable form			*/
char *size_human(long long int value)
{
	if (value == 1)
		sprintf(string, _("%lld byte"), value);
	else if (value < 1024)
		sprintf(string, _("%lld bytes"), value);
	else if (value < 10485760)
		sprintf(string, _("%.1f kilobytes"), (float)value / 1024);
	else
		sprintf(string, _("%.1f megabytes"), (float)value / 1048576);

	return(string);
}

/* Convert a number of seconds to a human-readable form			*/
char *time_human(int value)
{
	if (value == 1)
		sprintf(string, _("%i second"), value);
	else if (value < 60)
		sprintf(string, _("%i seconds"), value);
	else if (value < 3600)
		sprintf(string, _("%i:%02i seconds"), value / 60, value % 60);
	else
		sprintf(string, _("%i:%02i:%02i seconds"), value / 3600, (value / 60) % 60, value % 60);

	return(string);
}

/* Part of the infamous wget-like interface. Just put it in a function
because I need it quite often..					*/
void print_commas(long long int bytes_done)
{
	int i, j;

	printf("       ");
	j = (bytes_done / 1024) % 50;
	if (j == 0) j = 50;
	for (i = 0; i < j; i++)
	{
		if ((i % 10) == 0)
			putchar(' ');
		putchar(',');
	}
	fflush(stdout);
}

static void print_alternate_output(ServerSide_t *ServerSide)
{
	long long int done = ServerSide->bytes_done;
	long long int total = ServerSide->size;
	int i, j = 0;
	double now = gettime();

	printf("\r[%3ld%%] [", min(100, (long)(done*100. / total + .5)));

	for (i = 0; i<ServerSide->conf->num_connections; i++)
	{
		for (; j<((double)ServerSide->conn[i].currentbyte / (total + 1) * 50) - 1; j++)
			putchar('.');

		if (ServerSide->conn[i].currentbyte<ServerSide->conn[i].lastbyte)
		{
			if (now <= ServerSide->conn[i].last_transfer + ServerSide->conf->connection_timeout / 2)
				putchar(i + '0');
			else
				putchar('#');
		}
		else
			putchar('.');

		j++;

		for (; j<((double)ServerSide->conn[i].lastbyte / (total + 1) * 50); j++)
			putchar(' ');
	}

	if (ServerSide->bytes_per_second > 1048576)
		printf("] [%6.1fMB/s]", (double)ServerSide->bytes_per_second / (1024 * 1024));
	else if (ServerSide->bytes_per_second > 1024)
		printf("] [%6.1fKB/s]", (double)ServerSide->bytes_per_second / 1024);
	else
		printf("] [%6.1fB/s]", (double)ServerSide->bytes_per_second);

	if (done<total)
	{
		int seconds, minutes, hours, days;
		seconds = ServerSide->finish_time - now;
		minutes = seconds / 60; seconds -= minutes * 60;
		hours = minutes / 60; minutes -= hours * 60;
		days = hours / 24; hours -= days * 24;
		if (days)
			printf(" [%2dd%2d]", days, hours);
		else if (hours)
			printf(" [%2dh%02d]", hours, minutes);
		else
			printf(" [%02d:%02d]", minutes, seconds);
	}

	fflush(stdout);
}

void print_help()
{
#ifdef NOGETOPTLONG
	printf(_("Usage: ServerSide [options] url1 [url2] [url...]\n"
		"\n"
		"-s x\tSpecify maximum speed (bytes per second)\n"
		"-n x\tSpecify maximum number of connections\n"
		"-o f\tSpecify local output file\n"
		"-S [x]\tSearch for mirrors and download from x servers\n"
		"-H x\tAdd header string\n"
		"-U x\tSet user agent\n"
		"-N\tJust don't use any proxy server\n"
		"-q\tLeave stdout alone\n"
		"-v\tMore status information\n"
		"-a\tAlternate progress indicator\n"
		"-h\tThis information\n"
		"-V\tVersion information\n"
		"\n"
		"If you find a error, then mail me. yws3073@naver.com\n"));
#else
	printf(_("Usage: ServerSide [options] url1 [url2] [url...]\n"
		"\n"
		"--max-speed=x\t\t-s x\tSpecify maximum speed (bytes per second)\n"
		"--num-connections=x\t-n x\tSpecify maximum number of connections\n"
		"--output=f\t\t-o f\tSpecify local output file\n"
		"--search[=x]\t\t-S [x]\tSearch for mirrors and download from x servers\n"
		"--header=x\t\t-H x\tAdd header string\n"
		"--user-agent=x\t\t-U x\tSet user agent\n"
		"--no-proxy\t\t-N\tJust don't use any proxy server\n"
		"--quiet\t\t\t-q\tLeave stdout alone\n"
		"--verbose\t\t-v\tMore status information\n"
		"--alternate\t\t-a\tAlternate progress indicator\n"
		"--help\t\t\t-h\tThis information\n"
		"--version\t\t-V\tVersion information\n"
		"\n"
		"If you find a error, then mail me. yws3073@naver.com\n"));
#endif
}

void print_version()
{
	printf(_("ServerSide version %s (%s)\n"), ServerSide_VERSION_STRING, ARCH);
	printf("\nGachon univ. Wonseok Ryu \n");
}

/* Print any message in the ServerSide structure				*/
void print_messages(ServerSide_t *ServerSide)
{
	message_t *m;

	while (ServerSide->message)
	{
		printf("%s\n", ServerSide->message->text);
		m = ServerSide->message;
		ServerSide->message = ServerSide->message->next;
		free(m);
	}
}