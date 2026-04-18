#include <socket.h>


#define USE_SOCKETS			1
#define MESSAGE_SIZE		1472
const char message[MESSAGE_SIZE] = { [0 ... ( MESSAGE_SIZE - 1 )] = 1 };
char recv_message[MESSAGE_SIZE];

//Define Server IP
#define ETH_SERVER_IP		"192.168.0.1"
#define ETH_SERVER_PORT		55151


static inline void udp_tx_benchmark()
{
	/* Init UDP connection */
#if USE_SOCKETS == 0
	ip_addr_t PC_IPADDR;
	IP_ADDR4( &PC_IPADDR , 192 , 168 , 0 , 1 ); // TODO: stop doing this hardcoded

	struct udp_pcb* my_udp = udp_new();
	udp_connect( my_udp , &PC_IPADDR , ETH_SERVER_PORT );

#else
	int sock = socket( AF_INET , SOCK_DGRAM , 0 );

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons( ETH_SERVER_PORT );
	addr.sin_addr.s_addr = inet_addr( ETH_SERVER_IP );
#endif

	osDelay( 1000 );
	printf( "IP: %s\n"    , ipaddr_ntoa( &gnetif.ip_addr ) );
	printf( "Mask: %s\n"  , ipaddr_ntoa( &gnetif.netmask ) );
	printf( "GW: %s\n"    , ipaddr_ntoa( &gnetif.gw ) );
	printf( "netif: %d\n" , netif_is_up( &gnetif ) );
	printf( "Link: %d\n"  , netif_is_link_up( &gnetif ) );

	for( ; ; )
	{
#if USE_SOCKETS == 0
		struct pbuf* udp_buffer = udp_buffer = pbuf_alloc( PBUF_TRANSPORT , MESSAGE_SIZE , PBUF_RAM );

		if( udp_buffer != NULL )
		{
			memcpy( udp_buffer->payload , message , MESSAGE_SIZE );
			LOCK_TCPIP_CORE();
			udp_send( my_udp , udp_buffer );
			UNLOCK_TCPIP_CORE();
			pbuf_free( udp_buffer );
		}
#else
		sendto( sock , message , MESSAGE_SIZE , 0 , (struct sockaddr* )&addr , sizeof( addr ) );
#endif
	}
}


static inline void tcp_loopback()
{

#if USE_SOCKETS == 0

#else
	struct sockaddr_in addr;
	memset( &addr , 0 , sizeof( addr ) );

	addr.sin_family = AF_INET;
	addr.sin_port = htons( ETH_SERVER_PORT );
	addr.sin_addr.s_addr = inet_addr( ETH_SERVER_IP );

	int sockfd = lwip_socket( AF_INET, SOCK_STREAM , IPPROTO_TCP );
	if( sockfd == -1 )
		printf( "failed to create socket, errno = %d\n" , errno );

	while( !netif_is_up( &gnetif ) || !netif_is_link_up(&gnetif ) )
	    osDelay( 100 );

	int ret = lwip_connect( sockfd , (const struct sockaddr*)&addr , sizeof(addr) );
	if( ret < 0 )
		printf( "failed to connect socket, errno = %d\n" , errno );
#endif

	osDelay( 1000 );
	printf( "IP: %s\n"    , ipaddr_ntoa( &gnetif.ip_addr ) );
	printf( "Mask: %s\n"  , ipaddr_ntoa( &gnetif.netmask ) );
	printf( "GW: %s\n"    , ipaddr_ntoa( &gnetif.gw ) );
	printf( "netif: %d\n" , netif_is_up( &gnetif ) );
	printf( "Link: %d\n"  , netif_is_link_up( &gnetif ) );

	for( ; ; )
	{
#if USE_SOCKETS == 0
#else
		volatile ssize_t read_len = lwip_read( sockfd , recv_message, MESSAGE_SIZE );
		volatile ssize_t write_len = lwip_write(sockfd, recv_message, read_len );
#endif
	}
}
