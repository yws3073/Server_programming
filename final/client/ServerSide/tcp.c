#include "ServerSide.h"

/* Get a TCP connection */
int tcp_connect(char *hostname, int port, char *local_if)
{
	struct hostent *host = NULL;
	struct sockaddr_in addr;
	struct sockaddr_in local;
	int fd;

#ifdef DEBUG
	socklen_t i = sizeof(local);

	fprintf(stderr, "tcp_connect( %s, %i ) = ", hostname, port);
#endif

	/* Why this loop? Because the call might return an empty record.
	At least it very rarely does, on my system...		*/
	for (fd = 0; fd < 5; fd++)
	{
		if ((host = gethostbyname(hostname)) == NULL)
			return(-1);
		if (*host->h_name) break;
	}
	if (!host || !host->h_name || !*host->h_name)
		return(-1);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return(-1);

	if (local_if && *local_if)
	{
		local.sin_family = AF_INET;
		local.sin_port = 0;
		local.sin_addr.s_addr = inet_addr(local_if);
		if (bind(fd, (struct sockaddr *) &local, sizeof(struct sockaddr_in)) == -1)
		{
			close(fd);
			return(-1);
		}
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr = *((struct in_addr *) host->h_addr);

	if (connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) == -1)
	{
		close(fd);
		return(-1);
	}

#ifdef DEBUG
	getsockname(fd, &local, &i);
	fprintf(stderr, "%i\n", ntohs(local.sin_port));
#endif

	return(fd);
}

int get_if_ip(char *iface, char *ip)
{
	struct ifreq ifr;
	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	memset(&ifr, 0, sizeof(struct ifreq));

	strcpy(ifr.ifr_name, iface);
	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(fd, SIOCGIFADDR, &ifr) == 0)
	{
		struct sockaddr_in *x = (struct sockaddr_in *) &ifr.ifr_addr;
		strcpy(ip, inet_ntoa(x->sin_addr));
		return(1);
	}
	else
	{
		return(0);
	}
}
