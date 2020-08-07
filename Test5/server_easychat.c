#define	_USE_BSD
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>

#define	QLEN		32	/* maximum connection queue length	*/
#define	BUFSIZE		4096
#define	CLIENTNUM	10

unsigned short	portbase = 0;  

int 		sockarray[CLIENTNUM];
pthread_t	sockid[CLIENTNUM];
int 		socknum = 0;  

int	passivesock(const char *service, const char *transport,int qlen);
int	passiveTCP(const char *service, int qlen);
int	TCP_chatd(int fd);
int	errexit(const char *format, ...);
void reaper(int);

int main(int argc, char *argv[])
{
	pthread_t th;
	pthread_attr_t ta;

	char *service = "echo"; 	/*服务名称或端口号*/
	struct sockaddr_in fsin;	/* 来自客户的地址*/
	unsigned int alen;	/* 客户端地址长度 */
	int	msock;		/* 主服务器套接字 */
	int	ssock;		/* 从服务器套接字 */
	int i;

	switch (argc) {
	case	1:
		break;
	case	2:
		service = argv[1];
		break;
	default:
		errexit("usage: TCPechod [port]\n");
	}

	msock = passiveTCP(service, QLEN);
	printf("server port: %s\n",service);

	(void)pthread_attr_init(&ta);
	(void)pthread_attr_setdetachstate(&ta,PTHREAD_CREATE_DETACHED);

	while (1) {
		alen = sizeof(fsin);
		ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
		
		if (ssock < 0) {
			if (errno == EINTR) 
				continue;
			errexit("accept: %s\n", strerror(errno));
		}
		if(pthread_create(&th,&ta,(void * (*)(void *))TCP_chatd,(void *)ssock) < 0)
			errexit("pthread_create:%s\n",strerror(errno));
		
		else{
			sockarray[socknum] = ssock;
			sockid[socknum]=th;
			socknum++;
		}
	}
}

int TCP_chatd(int fd)
{
	char buf[BUFSIZE];
	int	cc;
	int i;
	
	while(cc = read(fd,buf,sizeof(buf)))
	{
		if(cc < 0)
			errexit("echo read:%s\n",strerror(errno));

		for(i=0;i<socknum;i++)
		{
			if(write(sockarray[i],buf,cc) < 0)
				errexit("echo write:%s\n",strerror(errno));
		}
	}

	return 0;
}

int passivesock(const char *service, const char *transport, int qlen)
{
	struct servent	*pse;	/*指向服务信息条目的指针*/
	struct protoent *ppe;	/*指向协议信息条目的指针*/
	struct sockaddr_in sin;	/*Internet端点地址*/
	int	s, type;	/*套接字描述符和套接字类型*/

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	/*将服务名称映射到端口号*/
	if ( pse = getservbyname(service, transport) )
		sin.sin_port = htons(ntohs((unsigned short)pse->s_port)
			+ portbase);
	else if ((sin.sin_port=htons((unsigned short)atoi(service))) == 0)
		errexit("can't get \"%s\" service entry\n", service);

 	/*将协议名称映射到协议号*/	
	if ( (ppe = getprotobyname(transport)) == 0)
		errexit("can't get \"%s\" protocol entry\n", transport);

    /* 使用协议选择套接字类型*/
	if (strcmp(transport, "udp") == 0)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

    /*分配一个套接字*/
	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0)
		errexit("can't create socket: %s\n", strerror(errno));

    /*绑定套接字*/
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		errexit("can't bind to %s port: %s\n", service,
			strerror(errno));
	if (type == SOCK_STREAM && listen(s, qlen) < 0)
		errexit("can't listen on %s port: %s\n", service,
			strerror(errno));
	return s;
}

int passiveTCP(const char *service, int qlen)
{
	return passivesock(service, "tcp", qlen);
}

void reaper(int sig)
{
	int	status;

	while (wait3(&status, WNOHANG, (struct rusage *)0) >= 0)
		/* empty */;
}

int errexit(const char *format, ...)
{
	va_list	args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(1);
}
