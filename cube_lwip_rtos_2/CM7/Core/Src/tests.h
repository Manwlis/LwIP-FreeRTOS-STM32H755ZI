/* Includes ------------------------------------------------------------------*/
#include "settings.h"

#if LWIP_IMPLEMENTATION == RAW_API
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#else
#include <socket.h>
#endif

#include <stdio.h>

/* Defines ------------------------------------------------------------*/
#define MESSAGE_SIZE	1460
#define ETH_SERVER_PORT	55151

#if LWIP_IMPLEMENTATION == RAW_API
#define ETH_SERVER_IP_1	192
#define ETH_SERVER_IP_2	168
#define ETH_SERVER_IP_3	0
#define ETH_SERVER_IP_4	1
#else
#define ETH_SERVER_IP	"192.168.0.1"
#endif

/* Typedef -----------------------------------------------------------*/
#if LWIP_IMPLEMENTATION == RAW_API
struct tcp_client_ctx
{
	struct tcp_pcb* pcb;

	struct pbuf* rx_head;
	struct pbuf* rx_tail;

	uint16_t rx_offset;   // offset inside head pbuf
};
#endif

/* Variables ---------------------------------------------------------*/
const char message[MESSAGE_SIZE] = { [0 ... ( MESSAGE_SIZE - 1 )] = 1 };
char recv_message[MESSAGE_SIZE];

#if LWIP_IMPLEMENTATION == RAW_API
static struct tcp_pcb* client_pcb;
#endif
/* Function prototypes -----------------------------------------------*/
#if LWIP_IMPLEMENTATION == RAW_API
static err_t tcp_client_connected( void* arg , struct tcp_pcb* tpcb , err_t err );
static err_t tcp_client_recv( void* arg , struct tcp_pcb* tpcb , struct pbuf* p , err_t err );
static err_t tcp_client_sent( void* arg , struct tcp_pcb* tpcb , u16_t len );
static void tcp_client_err( void* arg , err_t err );
#endif

/* Functions ---------------------------------------------------------*/
/**
 * @brief  UDP transmit test.
 * @retval None
 */
static inline void udp_tx_benchmark()
{
	/* Init UDP connection */
#if LWIP_IMPLEMENTATION == RAW_API
	ip_addr_t PC_IPADDR;
	IP4_ADDR( &PC_IPADDR , ETH_SERVER_IP_1 , ETH_SERVER_IP_2 , ETH_SERVER_IP_3 , ETH_SERVER_IP_4 );

	struct udp_pcb* my_udp = udp_new();
	udp_connect( my_udp , &PC_IPADDR , ETH_SERVER_PORT );

#else
	int sock = socket( AF_INET , SOCK_DGRAM , 0 );

	struct sockaddr_in addr;
	memset( &addr , 0 , sizeof( addr ) );

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
#if LWIP_IMPLEMENTATION == RAW_API
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

/**
 * @brief  TCP loopback test.
 * @retval None
 */
static inline void tcp_loopback()
{

#if LWIP_IMPLEMENTATION == RAW_API

	ip_addr_t server_ip;

	IP4_ADDR( &server_ip , ETH_SERVER_IP_1 , ETH_SERVER_IP_2 , ETH_SERVER_IP_3 , ETH_SERVER_IP_4 );

	/* Create PCB */
	client_pcb = tcp_new();
	if( client_pcb == NULL )
	{
		printf( "Failed to create PCB\n" );
		return;
	}

	/* Register callbacks */
	tcp_arg( client_pcb , NULL );
	tcp_err( client_pcb , tcp_client_err );

	while( !netif_is_up( &gnetif ) || !netif_is_link_up( &gnetif ) )
		osDelay( 100 );

	err_t err = tcp_connect( client_pcb , &server_ip , ETH_SERVER_PORT , tcp_client_connected );
	if( err != ERR_OK )
	{
		printf( "Connect failed: %d\n" , err );
		tcp_abort( client_pcb );
	}

#else
	struct sockaddr_in addr;
	memset( &addr , 0 , sizeof( addr ) );

	addr.sin_family = AF_INET;
	addr.sin_port = htons( ETH_SERVER_PORT );
	addr.sin_addr.s_addr = inet_addr( ETH_SERVER_IP );

	int sockfd = lwip_socket( AF_INET , SOCK_STREAM , IPPROTO_TCP );
	if( sockfd == -1 )
		printf( "failed to create socket, errno = %d\n" , errno );

	while( !netif_is_up( &gnetif ) || !netif_is_link_up( &gnetif ) )
		osDelay( 100 );

	int ret = lwip_connect( sockfd , (const struct sockaddr*) &addr , sizeof( addr ) );
	if( ret < 0 )
		printf( "failed to connect socket, errno = %d\n" , errno );
#endif

//	osDelay( 1000 );
//	printf( "IP: %s\n" , ipaddr_ntoa( &gnetif.ip_addr ) );
//	printf( "Mask: %s\n" , ipaddr_ntoa( &gnetif.netmask ) );
//	printf( "GW: %s\n" , ipaddr_ntoa( &gnetif.gw ) );
//	printf( "netif: %d\n" , netif_is_up( &gnetif ) );
//	printf( "Link: %d\n" , netif_is_link_up( &gnetif ) );

	for( ; ; )
	{
		volatile ssize_t read_len = lwip_read( sockfd , recv_message , MESSAGE_SIZE );
		volatile ssize_t write_len = lwip_write( sockfd , recv_message , read_len );
	}
}

#if LWIP_IMPLEMENTATION == RAW_API
static void enqueue_pbuf( struct tcp_client_ctx* ctx , struct pbuf* p )
{
	p->next = NULL;

	if( ctx->rx_tail )
		ctx->rx_tail->next = p;
	else
		ctx->rx_head = p;

	ctx->rx_tail = p;
}

static struct pbuf* dequeue_pbuf( struct tcp_client_ctx* ctx )
{
	struct pbuf* p = ctx->rx_head;
	if( !p )
		return NULL;

	ctx->rx_head = p->next;
	if( ctx->rx_head == NULL )
		ctx->rx_tail = NULL;

	p->next = NULL;
	return p;
}

static void try_send( struct tcp_client_ctx* ctx )
{
	struct tcp_pcb* tpcb = ctx->pcb;

	while( ctx->rx_head )
	{
		uint16_t sndbuf = tcp_sndbuf( tpcb );
		if( sndbuf == 0 )
			return;

		struct pbuf* p = ctx->rx_head;

		uint16_t remaining = p->len - ctx->rx_offset;
		uint16_t chunk = remaining;

		if( chunk > sndbuf )
			chunk = sndbuf;
		if( chunk > TCP_MSS )
			chunk = TCP_MSS;

		err_t err = tcp_write( tpcb , (uint8_t*) p->payload + ctx->rx_offset , chunk , TCP_WRITE_FLAG_COPY );

//		if( err == ERR_MEM )
//		{
//			/* Stop, retry later */
//			return;
//		}
		if( err != ERR_OK )
		{
			printf( "tcp_write error: %d\n" , err );
			return;
		}

		ctx->rx_offset += chunk;

		/* Fully consumed this pbuf? */
		if( ctx->rx_offset >= p->len )
		{
			ctx->rx_offset = 0;
			dequeue_pbuf( ctx );
			pbuf_free( p );
		}
	}

	tcp_output( tpcb );
}

/**
 * @brief  TCP error callback.
 * @retval None
 */
static void tcp_client_err( void* arg , err_t err )
{
	UNUSED( arg );
	UNUSED( err );

	printf( "TCP error: %d\n" , err );
	client_pcb = NULL;
}

/**
 * @brief  TCP connect callback.
 * @retval err_t error code
 */
static err_t tcp_client_connected( void* arg , struct tcp_pcb* tpcb , err_t err )
{
	UNUSED( arg );
	UNUSED( err );

	struct tcp_client_ctx* ctx = (struct tcp_client_ctx*) arg;

	ctx->pcb = tpcb;
	ctx->rx_head = NULL;
	ctx->rx_tail = NULL;
	ctx->rx_offset = 0;

	tcp_arg( tpcb , ctx );
	tcp_recv( tpcb , tcp_client_recv );
	tcp_sent( tpcb , tcp_client_sent );

	return ERR_OK;
}

/**
 * @brief  TCP recv callback. Implements loopback
 * @retval err_t error code
 */
static err_t tcp_client_recv( void* arg , struct tcp_pcb* tpcb , struct pbuf* p , err_t err )
{
	UNUSED( err );

	struct tcp_client_ctx* ctx = (struct tcp_client_ctx*) arg;

	if( p == NULL )
	{
		printf( "Connection closed\n" );
		tcp_close( tpcb );
		return ERR_OK;
	}

	/* Take ownership */
	pbuf_ref( p );
	enqueue_pbuf( ctx , p );

	/* Inform TCP stack immediately */
	tcp_recved( tpcb , p->tot_len );

	/* Try sending immediately */
	try_send( ctx );

	/* Free original reference */
	pbuf_free( p );

	return ERR_OK;
}

/**
 * @brief  TCP send callback. Tries to send any remaining data.
 * @retval err_t error code
 */
static err_t tcp_client_sent( void* arg , struct tcp_pcb* tpcb , u16_t len )
{
	UNUSED( len );

	struct tcp_client_ctx* ctx = (struct tcp_client_ctx*) arg;

	/* ACK freed buffer → send more */
	try_send( ctx );

	return ERR_OK;
}
#endif
