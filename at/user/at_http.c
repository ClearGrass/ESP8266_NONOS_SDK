/****************************************************************************

AT HTTP 连接通信

*****************************************************************************/
#include "c_types.h"
#include "at_custom.h"
#include "osapi.h"
#include "at_custom.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

#define GET "GET /%s HTTP/1.1\r\nContent-Type: text/html;charset=utf-8\r\nAccept: */*\r\nHost: %s\r\nConnection: Keep-Alive\r\n\r\n"
#define POST "POST /%s HTTP/1.1\r\nAccept: */*\r\nContent-Length: %d\r\nContent-Type: application/json\r\nHost: %s\r\nConnection: Keep-Alive\r\n\r\n%s"


static char host[32];
static char filename[208];
static unsigned short port;
static struct espconn user_tcp_conn;
static char *user_tcp_sendbuff = NULL;

LOCAL void ICACHE_FLASH_ATTR findStr(char * input, char * output) {
	char * outputStart = strchr(input, '{');
	char * outputEnd = NULL;
	if (NULL != outputStart) {
		outputEnd = strrchr(input, '}');
		if (NULL != outputEnd) {
			memcpy(output, outputStart, outputEnd - outputStart + 1);
		}
	}
	output[outputEnd - outputStart + 1] = '\0';
}


//成功接收到服务器返回数据函数
LOCAL void ICACHE_FLASH_ATTR user_tcp_recv_cb(void *arg, char *pdata,
		unsigned short len) {
	//uart0_sendStr("\r\n ----- 开始接受数据----- \r\n ");
	uint32_t i, offset;
	char *pJsonBuf = NULL;
	char *pParseBuffer = NULL;
	pParseBuffer = (char *)os_strstr(pdata, "\r\n\r\n");

    if (pParseBuffer == NULL) {
        return;
    }
    pParseBuffer += 4;
    pJsonBuf = os_malloc(strlen(pParseBuffer) + 32);
    if(pJsonBuf == NULL)
    {
    	at_response_error();
    	return;
    }
    offset = os_sprintf(pJsonBuf, "+HTTPPOST:");
	findStr(pParseBuffer, pJsonBuf+offset);
	at_response(pJsonBuf);
	os_free(pJsonBuf);
	at_port_print_irom_str("\r\n");
	at_response_ok();
	//uart0_sendStr("\r\n -----结束接受数据-----  \r\n ");

}

//发送数据到服务器成功的回调函数
LOCAL void ICACHE_FLASH_ATTR user_tcp_sent_cb(void *arg) {
	//uart0_sendStr("发送数据成功！\r\n ");
	if(user_tcp_sendbuff != NULL)
	{
		os_free(user_tcp_sendbuff);
	}
}

//断开服务器成功的回调函数
LOCAL void ICACHE_FLASH_ATTR user_tcp_discon_cb(void *arg) {
	//uart0_sendStr("断开连接成功！\r\n ");
	at_port_print_irom_str("\r\n+HTTPCONNECT:0\r\n");

	if(user_tcp_sendbuff != NULL)
	{
		os_free(user_tcp_sendbuff);
	}
}

//连接失败的回调函数，err为错误代码
LOCAL void ICACHE_FLASH_ATTR user_tcp_recon_cb(void *arg, sint8 err) {
	//uart0_sendStr("连接错误，错误代码为%d\r\n", err);
	//espconn_connect((struct espconn *) arg);
	at_response_error();

	if(user_tcp_sendbuff != NULL)
	{
		os_free(user_tcp_sendbuff);
	}
}

//成功连接到服务器的回调函数
LOCAL void ICACHE_FLASH_ATTR user_tcp_connect_cb(void *arg) {
	struct espconn *pespconn = arg;
	espconn_regist_recvcb(pespconn, user_tcp_recv_cb);
	espconn_regist_sentcb(pespconn, user_tcp_sent_cb);
	espconn_regist_disconcb(pespconn, user_tcp_discon_cb);

	//uart0_sendStr("\r\n ----- 请求数据开始----- \r\n");
	//uart0_tx_buffer(buffer, strlen(buffer));
	//uart0_sendStr("\r\n -----请求数据结束-----  \r\n");

	at_port_print_irom_str("\r\n+HTTPCONNECT:1\r\n");
	at_response_ok();
	//espconn_sent(pespconn, buffer, strlen(buffer));
}

LOCAL void ICACHE_FLASH_ATTR my_station_init(struct ip_addr *remote_ip,
		struct ip_addr *local_ip, int remote_port) {

	int ret;
	if((remote_port == 0) || (remote_ip == NULL) || (local_ip == NULL))
	{
		at_response_error();
		return;
	}

	//配置
	user_tcp_conn.type = ESPCONN_TCP;
	user_tcp_conn.state = ESPCONN_NONE;
	user_tcp_conn.proto.tcp = (esp_tcp *) os_zalloc(sizeof(esp_tcp));
	os_memcpy(user_tcp_conn.proto.tcp->local_ip, local_ip, 4);
	os_memcpy(user_tcp_conn.proto.tcp->remote_ip, remote_ip, 4);
	user_tcp_conn.proto.tcp->local_port = espconn_port();
	user_tcp_conn.proto.tcp->remote_port = remote_port;

	//注册
	espconn_regist_connectcb(&user_tcp_conn, user_tcp_connect_cb);
	espconn_regist_reconcb(&user_tcp_conn, user_tcp_recon_cb);
	//连接服务器
	ret = espconn_connect(&user_tcp_conn);
	if(ret != 0)
	{
		if(ret == ESPCONN_ISCONN)
		{
			at_port_print_irom_str("\r\n+HTTPCONNECT:1\r\n");
			at_response_ok();
			return;
		}
		else
		{
			at_response_error();
		}
	}
}


//剖析URL
LOCAL void ICACHE_FLASH_ATTR http_parse_request_url(char *URL, char *host,char *filename, unsigned short *port) {


	char *PA;
	char *PB;

	memset(host, 0, sizeof(host));
	memset(filename, 0, strlen(filename));

	*port = 0;

	if (!(*URL)){
		//uart0_sendStr("\r\n ----- URL return -----  \r\n");
		return;
	}

	PA = URL;

	if (!strncmp(PA, "http://", strlen("http://"))) {
		PA = URL + strlen("http://");
	} else if (!strncmp(PA, "https://", strlen("https://"))) {
		PA = URL + strlen("https://");
	}

	PB = strchr(PA, '/');

	if (PB) {
		//uart0_sendStr("\r\n ----- PB=true -----  \r\n");
		memcpy(host, PA, strlen(PA) - strlen(PB));
		if (PB + 1) {
			memcpy(filename, PB + 1, strlen(PB - 1));
			filename[strlen(PB) - 1] = 0;
		}
		host[strlen(PA) - strlen(PB)] = 0;

		//uart0_sendStr(host,strlen(host));

	} else {
		//uart0_sendStr("\r\n ----- PB=false -----  \r\n");
		memcpy(host, PA, strlen(PA));
		host[strlen(PA)] = 0;
		//uart0_sendStr(host,strlen(host));
	}

	PA = strchr(host, ':');

	if (PA){
		*port = atoi(PA + 1);
	}else{
		*port = 80;
	}
}

LOCAL void ICACHE_FLASH_ATTR
Http_Post(char *data)
{
	if(user_tcp_sendbuff != NULL)
	{
		os_free(user_tcp_sendbuff);
	}
	user_tcp_sendbuff = os_malloc(strlen(data)+1);
	os_memset(user_tcp_sendbuff, 0x00, strlen(data) + 1);
	os_sprintf(user_tcp_sendbuff,POST,filename,strlen(data), host, data);
	espconn_sent(&user_tcp_conn, user_tcp_sendbuff, strlen(user_tcp_sendbuff));
}

//寻找DNS解析，并且配置
LOCAL void ICACHE_FLASH_ATTR user_esp_dns_found(const char *name, ip_addr_t *ipaddr,void *arg) {

	struct ip_info info;
	wifi_get_ip_info(STATION_IF, &info);
	my_station_init(ipaddr, &info.ip, port);

}

//定义Get请求的实现
LOCAL void ICACHE_FLASH_ATTR startHttpQuestByGET(char *URL){
	struct ip_addr addr;
	uint8 getState = wifi_station_get_connect_status();

	if(getState != STATION_GOT_IP)
	{
		at_response_error();
		return;
	}

	http_parse_request_url(URL,host,filename,&port);
	espconn_gethostbyname(&user_tcp_conn,host, &addr,
	user_esp_dns_found);
}


//定义Post请求的实现
LOCAL void ICACHE_FLASH_ATTR startHttpQuestByPOST(char *URL,char *method,char *postdata){
	struct ip_addr addr;
	uint8 getState = wifi_station_get_connect_status();

	if(getState != STATION_GOT_IP)
	{
		at_response_error();
		return;
	}
	http_parse_request_url(URL,host,filename,&port);
	espconn_gethostbyname(&user_tcp_conn,host, &addr,
	user_esp_dns_found);
}


void ICACHE_FLASH_ATTR
at_setupCmdHttpConnect(uint8_t id, char *pPara)
{
	uint16_t iLen = 0;
	char url[256];
	pPara++; // skip '='
	iLen = strlen(pPara);
	iLen -= 2;
	if(iLen >= 256)
	{
		return;
	}
	os_memset(url, 0x00, sizeof(url));
	os_memcpy(url, pPara, iLen);
	startHttpQuestByGET(url);
}

void ICACHE_FLASH_ATTR
at_setupCmdHttpPost(uint8_t id, char *pPara)
{
	extern void ICACHE_FLASH_ATTR Http_Post(char *data);
	uint16_t iLen = 0;
	pPara++; // skip '='
	iLen = strlen(pPara);
	iLen -= 2;
	Http_Post(pPara);
}










