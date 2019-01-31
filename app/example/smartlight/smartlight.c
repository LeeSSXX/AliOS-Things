/*
 * Copyright (C) 2019-2020 LSX Writer
 */

#include <stdio.h>
#include <aos/aos.h>
#include <netmgr.h>
#include "netmgr_wifi.h"
#include <aos/cli.h>
#include <aos/yloop.h>
#include "iot_export.h"
#include "aos/log.h"
#ifdef AOS_ATCMD
#include <atparser.h>
#endif
#include <hal/soc/soc.h>

#define GPIO_WIFI_LED 2
#define GPIO_LED_TAG 4
#define GPIO_KEY_AWSS 5

static char linkkit_started = 0;

// SmartConfig配网是否在运行
static char awss_running = 0;
// 判断高低电平
static int light_tag = 3;
// 闪烁的频率在网络连接成功时
static int ms_flash_led_network_ok = 800;
// 闪烁的频率在无网络时
static int ms_flash_led_network_fail = 300;
// 板载Led灯(GPIO2端口)
static gpio_dev_t wifi_led;
// GPIO4端口
static gpio_dev_t led_tag;
// GPIO5端口
static gpio_dev_t key_awss;

// 闪烁WiFi led灯函数在网络连接成功时
static void connect_network_ok_flash_led(void *args)
{
    if (light_tag == 1)
    {
        light_tag = 0;
        hal_gpio_output_low(&wifi_led);
        aos_post_delayed_action(ms_flash_led_network_ok, connect_network_ok_flash_led, NULL);
    }
    else if (light_tag == 0)
    {
        light_tag = 1;
        hal_gpio_output_high(&wifi_led);
        aos_post_delayed_action(ms_flash_led_network_ok, connect_network_ok_flash_led, NULL);
    }
    else
    {
        hal_gpio_output_high(&wifi_led);
    }
}

// 闪烁WiFi led灯函数在网络连接失败
static void connect_network_fail_flash_led(void *args)
{
    if (light_tag == 3)
    {
        light_tag = 4;
        hal_gpio_output_low(&wifi_led);
        aos_post_delayed_action(ms_flash_led_network_fail, connect_network_fail_flash_led, NULL);
    }
    else if (light_tag == 4)
    {
        light_tag = 3;
        hal_gpio_output_high(&wifi_led);
        aos_post_delayed_action(ms_flash_led_network_fail, connect_network_fail_flash_led, NULL);
    }
    else
    {
        hal_gpio_output_high(&wifi_led);
    }
}

/*
 * Note:
 * the linkkit_event_monitor must not block and should run to complete fast
 * if user wants to do complex operation with much time,
 * user should post one task to do this, not implement complex operation in linkkit_event_monitor
 */
static void linkkit_event_monitor(int event)
{
    switch (event)
    {
    case IOTX_AWSS_START: // AWSS start without enbale, just supports device discover
        // operate led to indicate user
        LOG("IOTX_AWSS_START");
        break;
    case IOTX_AWSS_ENABLE: // AWSS enable
        LOG("IOTX_AWSS_ENABLE");
        // operate led to indicate user
        break;
    case IOTX_AWSS_LOCK_CHAN: // AWSS lock channel(Got AWSS sync packet)
        LOG("IOTX_AWSS_LOCK_CHAN");
        // operate led to indicate user
        break;
    case IOTX_AWSS_PASSWD_ERR: // AWSS decrypt passwd error
        LOG("IOTX_AWSS_PASSWD_ERR");
        // operate led to indicate user
        break;
    case IOTX_AWSS_GOT_SSID_PASSWD:
        LOG("IOTX_AWSS_GOT_SSID_PASSWD");
        // operate led to indicate user
        break;
    case IOTX_AWSS_CONNECT_ADHA: // AWSS try to connnect adha (device discover, router solution)
        LOG("IOTX_AWSS_CONNECT_ADHA");
        // operate led to indicate user
        break;
    case IOTX_AWSS_CONNECT_ADHA_FAIL: // AWSS fails to connect adha
        LOG("IOTX_AWSS_CONNECT_ADHA_FAIL");
        // operate led to indicate user
        break;
    case IOTX_AWSS_CONNECT_AHA: // AWSS try to connect aha (AP solution)
        LOG("IOTX_AWSS_CONNECT_AHA");
        // operate led to indicate user
        break;
    case IOTX_AWSS_CONNECT_AHA_FAIL: // AWSS fails to connect aha
        LOG("IOTX_AWSS_CONNECT_AHA_FAIL");
        // operate led to indicate user
        break;
    case IOTX_AWSS_SETUP_NOTIFY: // AWSS sends out device setup information (AP and router solution)
        LOG("IOTX_AWSS_SETUP_NOTIFY");
        // operate led to indicate user
        break;
    case IOTX_AWSS_CONNECT_ROUTER: // AWSS try to connect destination router
        LOG("IOTX_AWSS_CONNECT_ROUTER");
        // operate led to indicate user
        break;
    case IOTX_AWSS_CONNECT_ROUTER_FAIL: // AWSS fails to connect destination router.
        LOG("IOTX_AWSS_CONNECT_ROUTER_FAIL");
        // operate led to indicate user
        break;
    case IOTX_AWSS_GOT_IP: // AWSS connects destination successfully and got ip address
        LOG("IOTX_AWSS_GOT_IP");
        // operate led to indicate user
        break;
    case IOTX_AWSS_SUC_NOTIFY: // AWSS sends out success notify (AWSS sucess)
        LOG("IOTX_AWSS_SUC_NOTIFY");
        // operate led to indicate user
        break;
    case IOTX_AWSS_BIND_NOTIFY: // AWSS sends out bind notify information to support bind between user and device
        LOG("IOTX_AWSS_BIND_NOTIFY");
        // operate led to indicate user
        break;
    case IOTX_CONN_CLOUD: // Device try to connect cloud
        LOG("IOTX_CONN_CLOUD");
        // operate led to indicate user
        break;
    case IOTX_CONN_CLOUD_FAIL: // Device fails to connect cloud, refer to net_sockets.h for error code
        LOG("IOTX_CONN_CLOUD_FAIL");
        // operate led to indicate user
        break;
    case IOTX_CONN_CLOUD_SUC: // Device connects cloud successfully
        LOG("IOTX_CONN_CLOUD_SUC");
        // operate led to indicate user
        break;
    case IOTX_RESET: // Linkkit reset success (just got reset response from cloud without any other operation)
        LOG("IOTX_RESET");
        // operate led to indicate user
        break;
    default:
        break;
    }
}

void start_linkkitapp(void *parms)
{
    LOG("linkkit app");
    //linkkit_app();
}

static void wifi_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_WIFI)
    {
        return;
    }

    if (event->code != CODE_WIFI_ON_GOT_IP)
    {
        return;
    }
    // WIFI连接成功后，闪烁板载led灯
    light_tag = 1;
    aos_post_delayed_action(ms_flash_led_network_ok, connect_network_ok_flash_led, NULL);

    netmgr_ap_config_t config;
    memset(&config, 0, sizeof(netmgr_ap_config_t));
    netmgr_get_ap_config(&config);
    LOG("wifi_service_event config.ssid %s", config.ssid);
    if (strcmp(config.ssid, "adha") == 0 || strcmp(config.ssid, "aha") == 0)
    {
        return;
    }

    if (!linkkit_started)
    {
        aos_post_delayed_action(50, start_linkkitapp, NULL);
        linkkit_started = 1;
    }
}

static void cloud_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_YUNIO)
    {
        return;
    }

    LOG("cloud_service_event %d", event->code);

    if (event->code == CODE_YUNIO_ON_CONNECTED)
    {
        LOG("user sub and pub here");
        return;
    }

    if (event->code == CODE_YUNIO_ON_DISCONNECTED)
    {
    }
}

static void start_netmgr(void *p)
{
    iotx_event_regist_cb(linkkit_event_monitor);
    netmgr_start(false);
    aos_task_exit(0);
}

void do_awss_active()
{
    LOG("do_awss_active %d\n", awss_running);
    awss_running = 1;
    extern int awss_config_press();
    awss_config_press();

    hal_gpio_clear_irq(&key_awss);
}

static void netmgr_reset(void *p)
{
    netmgr_clear_ap_config();
    HAL_Sys_reboot();
}

extern int awss_report_reset();
static void do_awss_reset()
{
    aos_task_new("reset", (void (*)(void *))awss_report_reset, NULL, 2048);
    aos_post_delayed_action(2000, netmgr_reset, NULL);
}

static void handle_awss_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call(do_awss_active, NULL);
}

static void gpio_isr_handler(void *arg)
{
    hal_gpio_disable_irq(&key_awss);
    aos_post_delayed_action(1000, do_awss_active, NULL);
}

static struct cli_command awss = {
    .name = "active_awss",
    .help = "active_awss [start]",
    .function = handle_awss_cmd};

void init_gpio()
{
    wifi_led.port = GPIO_WIFI_LED;
    wifi_led.config = OUTPUT_PUSH_PULL;
    hal_gpio_init(&wifi_led);
    hal_gpio_output_high(&wifi_led);

    led_tag.port = GPIO_LED_TAG;
    led_tag.config = OUTPUT_PUSH_PULL;
    hal_gpio_init(&led_tag);
    hal_gpio_output_low(&led_tag);

    key_awss.port = GPIO_KEY_AWSS;
    key_awss.config = IRQ_MODE;
    hal_gpio_init(&key_awss);
}

void linkkit_key_process(input_event_t *eventinfo, void *priv_data)
{
    if (eventinfo->type != EV_KEY) {
        return;
    }
    LOG("awss config press %d\n", eventinfo->value);

    if (eventinfo->code == CODE_BOOT) {
        if (eventinfo->value == VALUE_KEY_CLICK) {

            do_awss_active();
        } else if (eventinfo->value == VALUE_KEY_LTCLICK) {
            do_awss_reset();
        }
    }
}

int application_start(int argc, char *argv[])
{
    // 初始化GIPO端口
    init_gpio();

    aos_post_delayed_action(ms_flash_led_network_fail, connect_network_fail_flash_led, NULL);

    //hal_gpio_enable_irq(&key_awss, IRQ_TRIGGER_FALLING_EDGE, gpio_isr_handler, (void *)GPIO_KEY_AWSS);

    LOG("application_start()");

#if AOS_ATCMD
    at.set_mode(ASYN);
    at.init(AT_RECV_PREFIX, AT_RECV_SUCCESS_POSTFIX,
            AT_RECV_FAIL_POSTFIX, AT_SEND_DELIMITER, 1000);
#endif
#ifdef WITH_SAL
    sal_init();
#endif
    aos_set_log_level(AOS_LL_DEBUG);

    netmgr_init();

    aos_register_event_filter(EV_KEY, linkkit_key_process, NULL);
    aos_register_event_filter(EV_WIFI, wifi_service_event, NULL);
    aos_register_event_filter(EV_YUNIO, cloud_service_event, NULL);

    aos_cli_register_command(&awss);

    aos_task_new("netmgr", start_netmgr, NULL, 4096);

    aos_loop_run();
    return 0;
}
