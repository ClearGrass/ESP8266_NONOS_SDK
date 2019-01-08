/****************************************************************************

AT 获取网络时间

*****************************************************************************/
#include "osapi.h"
#include "at_custom.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

static ip_addr_t ntp_ip;
static struct espconn user_conn;
static struct _esp_udp user_udp;
static os_timer_t user_os_timer;

LOCAL void ICACHE_FLASH_ATTR
udp_client_recv_cb(void *arg, char *pdata, unsigned short len)
{
    // UDP 接收数据回调
	struct espconn *pespconn = (struct espconn *)arg;
	uint32_t timestamp = 0;
	char buff[32], i;
	timestamp = (pdata[40]<<24) + (pdata[41]<<16) + (pdata[42]<<8) + pdata[43];
	os_sprintf(buff, "+NETCLK:%d\r\n", timestamp-2208988800UL);
	at_response(buff);
	at_response_ok();
	espconn_delete(pespconn);
}

LOCAL void ICACHE_FLASH_ATTR
udp_client_send_cb(void *arg)
{
}

LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
	int ret;

	struct espconn *pespconn = (struct espconn *)arg;
	uint8_t ucGetTimeCmd[48] = {0xE3 ,0x00 ,0x06 ,0xEC ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x31 ,0x4E ,0x31 ,0x34 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00};

    // 关闭 UDP 连接
	espconn_delete(pespconn);

    if(ipaddr == NULL)
    {
    	at_response_error();
        return;
    }
    pespconn->proto.udp->remote_ip[0] = ip4_addr1(ipaddr);
    pespconn->proto.udp->remote_ip[1] = ip4_addr2(ipaddr);
    pespconn->proto.udp->remote_ip[2] = ip4_addr3(ipaddr);
    pespconn->proto.udp->remote_ip[3] = ip4_addr4(ipaddr);
    pespconn->proto.udp->remote_port = 123;

    ret |= espconn_create(pespconn);

    ret |= espconn_sendto(pespconn, ucGetTimeCmd, sizeof(ucGetTimeCmd));

    if(ret != ESPCONN_OK)
    {
    	at_response_error();
    	return;
    }
}

LOCAL void ICACHE_FLASH_ATTR
UDP_TimeOut_CallBack(void *arg)
{
    // 超时回调关闭UDP连接
	struct espconn *pespconn = (struct espconn *)arg;
	espconn_delete(pespconn);
}

void ICACHE_FLASH_ATTR
at_queryCmdNetClk(uint8_t id)
{
	uint8 getState = wifi_station_get_connect_status();

    // 网络是否在连接状态
	if(getState != STATION_GOT_IP)
	{
		at_response_error();
		return;
	}

    wifi_set_broadcast_if(STATION_MODE);

    // 设置 UDP 连接
    user_conn.type = ESPCONN_UDP;
    user_conn.proto.udp = &user_udp;

    // 注册发送和接收的回调函数
	espconn_regist_sentcb(&user_conn, udp_client_send_cb);
	espconn_regist_recvcb(&user_conn, udp_client_recv_cb);

    // 获取 NTP 网络时间 IP
    espconn_gethostbyname(&user_conn, "1.cn.pool.ntp.org", &ntp_ip, user_esp_platform_dns_found);

    // 创建定时器任务，用于超时断开同步时间连接
	os_timer_disarm( &user_os_timer );
	os_timer_setfn( &user_os_timer, (ETSTimerFunc *) ( UDP_TimeOut_CallBack ), &user_conn );
	os_timer_arm( &user_os_timer, 1000, false );
}
