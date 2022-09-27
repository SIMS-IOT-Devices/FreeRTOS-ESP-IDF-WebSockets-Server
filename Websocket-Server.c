// Websocket Server responsive to GET request

#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "my_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <esp_http_server.h>

static const char *TAG = "Websocket Server: ";

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

void wifi_connection()
{
    nvs_flash_init();
    // 1 - Wi-Fi/LwIP Init Phase
    esp_netif_init();                    // TCP/IP initiation 					s1.1
    esp_event_loop_create_default();     // event loop 			                s1.2
    esp_netif_create_default_wifi_sta(); // WiFi station 	                    s1.3
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); // 					                    s1.4
    // 2 - Wi-Fi Configuration Phase
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = SSID,
            .password = PASS}};
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    // 3 - Wi-Fi Start Phase
    esp_wifi_start();
    // 4- Wi-Fi Connect Phase
    esp_wifi_connect();
}

// Asynchronous response data structure
struct async_resp_arg
{
    httpd_handle_t hd; // Server instance
    int fd;            // Session socket file descriptor
};

// The asynchronous response
static void generate_async_resp(void *arg)
{
    // Data format to be sent from the server as a response to the client
    char http_string[250];
    char *data_string = "Hello from ESP32 websocket server ...";
    sprintf(http_string, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", strlen(data_string));

    // Initialize asynchronous response data structure
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;

    // Send data to the client
    ESP_LOGI(TAG, "Executing queued work fd : %d", fd);
    httpd_socket_send(hd, fd, http_string, strlen(http_string), 0);
    httpd_socket_send(hd, fd, data_string, strlen(data_string), 0);

    free(arg);
}

// Initialize a queue for asynchronous communication
static esp_err_t async_get_handler(httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    ESP_LOGI(TAG, "Queuing work fd : %d", resp_arg->fd);
    httpd_queue_work(req->handle, generate_async_resp, resp_arg);
    return ESP_OK;
}

// Create URI (Uniform Resource Identifier) 
// for the server which is added to default gateway
static const httpd_uri_t uri_handler = {
    .uri = "/ws",           // URL added to WiFi's default gateway
    .method = HTTP_GET,
    .handler = async_get_handler,
    .user_ctx = NULL,
};

static void websocket_app_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Registering the uri_handler
        ESP_LOGI(TAG, "Registering URI handler");
        httpd_register_uri_handler(server, &uri_handler);
    }
}

void app_main(void)
{
    wifi_connection();
    vTaskDelay(1500 / portTICK_RATE_MS);
    websocket_app_start();
}
