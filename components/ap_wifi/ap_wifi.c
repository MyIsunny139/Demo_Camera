#include "ap_wifi.h"
#include "ws_server.h"
#include "cJSON.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define TAG     "apcfg"

// WiFi 连接超时时间（秒）
#define WIFI_CONNECT_TIMEOUT_SEC    15

//html网页在spiffs文件系统中的路径
#define INDEX_HTML_PATH "/spiffs/apcfg.html"

//html网页缓存
static char* index_html = NULL;

//配网事件
static EventGroupHandle_t   apcfg_event = NULL;

//接收到ap配网的ssid和密码
static char current_ssid[32];
static char current_password[64];

#define APCFG_BIT   (BIT0)

// 前向声明
static void wifi_connect_wait_task(void* param);

/** 从spiffs中加载html页面到内存
 * @param 无
 * @return 无 
*/
static char* initi_web_page_buffer(void)
{
    //定义挂载点
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",            //挂载点
        .partition_label = "html",         //分区名称
        .max_files = 5,                    //最大打开的文件数
        .format_if_mount_failed = false    //挂载失败是否执行格式化
        };
    //挂载spiffs
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    //查找文件是否存在
    struct stat st;
    if (stat(INDEX_HTML_PATH, &st))
    {
        ESP_LOGE(TAG, "apcfg.html not found");
        return NULL;
    }
    //打开html文件并且读取到内存中
    char* page = (char*)malloc(st.st_size + 1);
    if(!page)
    {
        return NULL;
    }
    memset(page,0,st.st_size + 1);
    FILE *fp = fopen(INDEX_HTML_PATH, "r");
    if (fread(page, st.st_size, 1, fp) == 0)
    {
        free(page);
        page = NULL;
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);
    return page;
}

/** wifi扫描完成
 * @param numbers 扫描到的ap个数
 * @param ap_records ap信息
 * @return 无 
*/
static void wifi_scan_finish_handle(int numbers,wifi_ap_record_t *ap_records)
{
    cJSON* root = cJSON_CreateObject();
    cJSON* wifilist_js = cJSON_AddArrayToObject(root,"wifi_list");
    for(int i = 0;i < numbers;i++)
    {
        cJSON* wifi_js = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_js,"ssid",(char*)ap_records[i].ssid);
        cJSON_AddNumberToObject(wifi_js,"rssi",ap_records[i].rssi);
        if(ap_records[i].authmode == WIFI_AUTH_OPEN)
            cJSON_AddBoolToObject(wifi_js,"encrypted",0);
        else
            cJSON_AddBoolToObject(wifi_js,"encrypted",1);
        cJSON_AddItemToArray(wifilist_js,wifi_js);
    }
    char* data = cJSON_Print(root);
    ESP_LOGI(TAG,"WS send:%s",data);
    web_ws_send((uint8_t*)data,strlen(data));
    cJSON_free(data);
    cJSON_Delete(root);
}

/** ws接收回调函数
 * @param payload 数据
 * @param len 数据长度
 * @return 无 
*/
static void ws_receive_handle(uint8_t* payload,int len)
{
    cJSON* root = cJSON_Parse((char*)payload);
    if(root)
    {
        cJSON* scan_js = cJSON_GetObjectItem(root,"scan");
        cJSON* ssid_js = cJSON_GetObjectItem(root,"ssid");
        cJSON* password_js = cJSON_GetObjectItem(root,"password");
        if(scan_js)
        {
            char* scan_value = cJSON_GetStringValue(scan_js);
            if(strcmp(scan_value,"start") == 0)
            {
                wifi_manager_scan(wifi_scan_finish_handle);
            }
        }
        if(ssid_js && password_js)
        {
            char* ssid = cJSON_GetStringValue(ssid_js);
            char* password = cJSON_GetStringValue(password_js);
            snprintf(current_ssid,sizeof(current_ssid),"%s",ssid);
            snprintf(current_password,sizeof(current_password),"%s",password);
            ESP_LOGI(TAG,"Receive ssid:%s,password:%s,now stop http server",current_ssid,current_password);
            //此回调函数里面由websocket底层调用，不宜直接调用关闭服务器操作
            xEventGroupSetBits(apcfg_event,APCFG_BIT);  
        }
    }
    else
    {
        ESP_LOGE(TAG,"Receive invaild json");
    }
}

static void ap_wifi_task(void* param)
{
    EventBits_t ev;
    while(1)
    {
        ev = xEventGroupWaitBits(apcfg_event,APCFG_BIT,pdTRUE,pdFALSE,pdMS_TO_TICKS(10*1000));
        if(ev &APCFG_BIT)
        {
            web_ws_stop();
            // 保存WiFi配置到NVS
            wifi_manager_save_config(current_ssid, current_password);
            wifi_manager_connect(current_ssid,current_password);
        }
    }
}

/** WiFi 连接等待任务（非阻塞）
 * @param param 未使用
 * @return 无
*/
static void wifi_connect_wait_task(void* param)
{
    int timeout = WIFI_CONNECT_TIMEOUT_SEC * 10;  // 100ms 一次检查
    
    ESP_LOGI(TAG, "等待WiFi连接，超时时间: %d秒", WIFI_CONNECT_TIMEOUT_SEC);
    
    while (timeout-- > 0) {
        if (wifi_manager_is_connect()) {
            ESP_LOGI(TAG, "WiFi连接成功");
            vTaskDelete(NULL);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // 连接超时，进入配网模式
    ESP_LOGW(TAG, "WiFi连接超时 (%d秒)，启动配网模式", WIFI_CONNECT_TIMEOUT_SEC);
    ap_wifi_apcfg(true);
    
    vTaskDelete(NULL);
}

/** wifi功能和ap配网功能初始化
 * @param f wifi连接状态回调函数
 * @return 无 
*/
void ap_wifi_init(p_wifi_state_callback f)
{
    index_html = initi_web_page_buffer();
    wifi_manager_init(f);
    apcfg_event = xEventGroupCreate();
    xTaskCreatePinnedToCore(ap_wifi_task,"apcfg",4096,NULL,2,NULL,1);
    
    // 检查是否有保存的WiFi配置
    if (wifi_manager_has_saved_config()) {
        char saved_ssid[32] = {0};
        char saved_password[64] = {0};
        
        if (wifi_manager_load_config(saved_ssid, saved_password) == ESP_OK) {
            ESP_LOGI(TAG, "尝试连接保存的WiFi: %s", saved_ssid);
            wifi_manager_connect(saved_ssid, saved_password);
            
            // 创建一个任务来等待WiFi连接，避免阻塞初始化
            xTaskCreatePinnedToCore(wifi_connect_wait_task, "wifi_wait", 2048, NULL, 2, NULL, 1);
        }
    } else {
        // 没有保存的配置，直接进入配网模式
        ESP_LOGI(TAG, "无保存的WiFi配置，启动配网模式");
        ap_wifi_apcfg(true);
    }
}

/** 连接某个热点
 * @param ssid
 * @param password
 * @return 无 
*/
void ap_wifi_set(const char* ssid,const char* password)
{
    wifi_manager_connect(ssid,password);
}

/** 启动配网模式
 * @param enable 暂无用，强制true
 * @return 无 
*/
void ap_wifi_apcfg(bool enable)
{
    if(enable)
    {
        wifi_manager_ap();
        ws_cfg_t ws = 
        {
            .html_code = index_html,
            .receive_fn = ws_receive_handle,
        };
        web_ws_start(&ws);
    }
}
