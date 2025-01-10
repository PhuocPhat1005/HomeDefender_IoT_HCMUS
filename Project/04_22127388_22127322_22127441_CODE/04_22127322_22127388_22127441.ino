#include <WiFi.h>
#include <WebServer.h>
#include <FirebaseESP32.h>
#include "PubSubClient.h"

#define APP_CPU 1
#define PRO_CPU 0

#define MAX_ANALOG_READ 4095
#define MAX_BRIGHTNESS 255

#include "src/OV2640.h"

#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"
#include "esp_camera.h"

// Khai báo các chân kết nối
const unsigned int pirPin = 12; // chân OUT của PIR Motion Sensor
const unsigned int ledRedPin = 14; // chân LED đỏ
const unsigned int ledGreenPin = 2; // chân LED xanh
const unsigned int flashPin = 4; // chân của đèn flash
const unsigned int buzzerPin = 15; // chân buzzer
const unsigned int photoresistorPin = 13; // chân PhotoResistor

const unsigned long led_delay_time = 500; // đặt thời gian delay cho mỗi đèn
const unsigned long buzzer_delay_time = 500; // đặt th  ời gian delay cho loa buzzer
const unsigned int photoresistor_threshold = 1000; // ngưỡng ánh sáng để bật flash

const int MAX_PAYLOAD_SIZE = 60000;

const int numSamples = 10;
unsigned long lastAlarmTime = 0;
const unsigned long alarmDuration = 10000; // thời gian báo động tối đa (10 giây)
bool emailSent = false;
unsigned long mode2StartTime = 0;
const unsigned long emailDelay = 10000;

const char* ssid = "KimDuyen";
const char* password = "25032008";
const char* mqtt_server = "broker.mqtt-dashboard.com";
const int mqtt_port = 1883;
const char* uniqueId = "22127999";

const char* topic_subscribe_active_device = "/HomeDefender/ActivateDevice"; // DONE
const char* topic_subscribe_scheduler = "/HomeDefender/ActivateDeviceScheduler"; // DONE
const char* topic_subscribe_streaming_video = "/HomeDefender/ActiveStreamingVideo"; // DONE
const char* topic_subscribe_control_flash = "/HomeDefender/ControlFlash"; // DONE
const char* topic_subscribe_force_alert = "/HomeDefender/ForceAlertMode"; // DONE
const char* topic_publishing_control_flash = "/HomeDefender/FlashStatus"; // DONE
const char* topic_publishing_photoresistor = "/HomeDefender/Photoresistor"; // DONE
const char* topic_publishing_update_history_warnings = "/HomeDefender/UpdateHistoryWarnings"; // DONE
const char* topic_publishing_update_history_photos = "/HomeDefender/UpdateHistoryPhotos"; // DONE
const char* topic_publishing_wifi_status = "/HomeDefender/WiFiStatus"; // DONE
const char* topic_publishing_mqtt_status = "/HomeDefender/MQTTStatus"; // DONE
const char* topic_publishing_force_alert = "/HomeDefender/ForceAlertNotification"; // DONE
const char* topic_system_warning = "HomeDefender/SystemWarning"; // DONE
const char* topic_publishing_system_mode = "/HomeDefender/SystemMode"; // DONE ***
const char* topic_publishing_streaming_video = "/HomeDefender/activeStreamingStatus";
const char* topic_subscribe_take_a_photo = "/HomeDefender/TakeAPhoto"; // DONE
const char* topic_publishing_activate_system_status = "/HomeDefender/activateSystemStatus"; // DONE
const char* topic_publishing_email_warnings = "/HomeDefender/SendWarning"; // DONE
const char* topic_publishing_email_notification = "/HomeDefender/EmailNotification"; // DONE
const char* topic_publishing_get_string_stream_link = "/HomeDefender/GetLinkStream"; //DONE
const char* topic_publishing_get_string_image_link = "/HomeDefender/GetLinkImage"; // DONE
const char* topic_publishing_take_a_photo = "/HomeDefender/TakeAPhotoNotification"; // DONE
const char* topic_subscribe_get_auto_image = "/HomeDefender/GetAutoImages";

WiFiClient espClient;
PubSubClient client(espClient);

bool take_a_photo = false;

OV2640 cam;
WebServer server(80); // Server HTTP cho video streaming

// ===== rtos task handles =========================
// Streaming is implemented with 3 tasks:
TaskHandle_t tMjpeg;   // handles client connections to the webserver
TaskHandle_t tCam;     // handles getting picture frames from the camera and storing them locally
TaskHandle_t tStream;  // actually streaming frames to all connected clients

// frameSync semaphore is used to prevent streaming buffer as it is replaced with the next frame
SemaphoreHandle_t frameSync = NULL;

// Queue stores currently connected clients to whom we are streaming
QueueHandle_t streamingClients;

// We will try to achieve 25 FPS frame rate
const int FPS = 14;

// We will handle web client requests every 50 ms (20 Hz)
const int WSINTERVAL = 100;

bool streamingActive = false;

int wifiReconnectAttempts = 0;
const int maxWifiReconnectAttempts = 5;

int mqttReconnectAttempts = 0;
const int maxMqttReconnectAttempts = 5;

bool isAlarming = false; // Biến trạng thái báo động
bool systemActive = true; // mặc định hệ thống đang hoạt động
bool schedulerActive = true; // mặc định hệ thống đang hoạt động

bool flashActive = false; // mặc đinh hệ thống ban đầu không sử dụng flash
bool previousFlashState = false;
int isDay = 0;
int isNight = 1;
bool forceAlert = false;
bool previousWiFiStatus = false;
bool previousMQTTStatus = false;
unsigned long lastWiFiPublish = 0;
unsigned long lastMQTTPublish = 0;
const unsigned long publishInterval = 5000;  // 5 giây

enum SystemStatus { OFF, SCHEDULED, ALWAYS_ON };
SystemStatus currentStatus = OFF;

void resetMode() {
  digitalWrite(ledRedPin, LOW);
  digitalWrite(ledGreenPin, LOW);
  // digitalWrite(ledYellowPin, LOW);
  analogWrite(flashPin, 0);
  noTone(buzzerPin);
  digitalWrite(pirPin, LOW);
  isAlarming = false;
  forceAlert = false;
}

// Kết nối WiFi
void WifiConnect() {
  wifiReconnectAttempts = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi ...");

  unsigned long previousMillis = millis();
  
  while (WiFi.status() != WL_CONNECTED && wifiReconnectAttempts < maxWifiReconnectAttempts) {
    if (millis() - previousMillis >= 1000) { // Chỉ chạy đoạn này sau 500ms
      previousMillis = millis();
      wifiReconnectAttempts++;
      Serial.print("Attempt ");
      Serial.print(wifiReconnectAttempts);
      Serial.println(" to connect to WiFi...");
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected !!!");
    wifiReconnectAttempts = 0; 
  } else {
    Serial.println("\nFailed to connect to WiFi.");
  }
}

// DONE
void connectMqttServer() {
  mqttReconnectAttempts = 0;
  while (!client.connected() && mqttReconnectAttempts < maxMqttReconnectAttempts) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(uniqueId)) { // Tên client
      Serial.println("Connected to MQTT!");
      Serial.println("Subcribing ...");

      if (client.subscribe(topic_subscribe_active_device)) {
        Serial.println("Subscribed to topic: ");
        Serial.println(topic_subscribe_active_device);
      } else {
        Serial.println("Subscription of Active Device failed!");
      }

      if (client.subscribe(topic_subscribe_scheduler)){
        Serial.println("Subscribed to topic: ");
        Serial.println(topic_subscribe_scheduler);
      }
      else {
        Serial.println("Subscription of Scheduler failed!");
      }

      if (client.subscribe(topic_subscribe_control_flash)){
        Serial.println("Subscribed to topic: ");
        Serial.println(topic_subscribe_control_flash);
      }
      else{
        Serial.println("Subscription of Control Flash failed!");
      }

      if (client.subscribe(topic_subscribe_streaming_video)){
        Serial.println("Subscribed to topic: ");
        Serial.println(topic_subscribe_streaming_video);
      }
      else{
        Serial.println("Subscription of Streaming Video failed!");
      }

      if (client.subscribe(topic_subscribe_take_a_photo)){
        Serial.println("Subscribed to topic: ");
        Serial.println(topic_subscribe_take_a_photo);
      }
      else {
        Serial.println("Subscription of Camera Without Flash failed!");
      }

      if (client.subscribe(topic_subscribe_force_alert)){
        Serial.println("Subscribed to topic: ");
        Serial.println(topic_subscribe_force_alert);
      }
      else{
        Serial.println("Subscription of Force Alert Mode failed!");
      }

      if (client.subscribe(topic_subscribe_get_auto_image)){
        Serial.println("Subscribed to topic: ");
        Serial.println(topic_subscribe_get_auto_image);
      }
      else{
        Serial.println("Subscription of Get Auto Images failed!");
      }

      Serial.println("Subcribe topics ...");
      mqttReconnectAttempts = 0;
      break;
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 5 seconds...");  
      delay(5000);
      mqttReconnectAttempts++;
    }
  }
  if (!client.connected()) {
    Serial.println("Failed to connect to MQTT after multiple attempts.");
    delay(10000);
  }
}

void publishHistoryData(bool warnings = false, bool photos = false) {
  client.publish(topic_publishing_update_history_warnings, warnings ? "true" : "false");
  client.publish(topic_publishing_update_history_photos, photos ? "true" : "false");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Topic: ");
  Serial.println(topic);

  if (length == 0){
    Serial.println("Empty payload received.");
    return;
  }

  if (length > MAX_PAYLOAD_SIZE){
    Serial.println("Payload is too large !!!");
    return; 
  }

  String stMessage;
  for (int i = 0; i < length; i++){
    stMessage += (char)payload[i];
  }

  Serial.print("Message: ");
  Serial.println(stMessage);

  if (String(topic) == topic_subscribe_active_device) {
    Serial.println("Handling Activate Device ...");
    if (stMessage == "true"){
      systemActive = true; // bật hệ thống
      Serial.println("Hệ thống đã được kích hoạt !!!");
    }
    else if (stMessage == "false"){
      systemActive = false;
      Serial.println("Hệ thống đã tắt !!!");
      resetMode();
    }
    else{
      Serial.println("Invalid payload received !!!");
    }
  }
  else if (String(topic) == topic_subscribe_control_flash) {
    Serial.println("Handling Control Flash ...");
    if(!systemActive){
      Serial.println("Hệ thống đang tắt! Không thể bật đèn Flash!!!");
      publishSystemWarning("Hệ thống đang tắt! Không thể bật đèn Flash!!!");
      return;
    }
    if (stMessage == "true"){
      flashActive = true;
      Serial.println("Đèn flash đã được kích hoạt !!!");
    }
    else if (stMessage == "false"){
      flashActive = false;
      Serial.println("Đèn flash đã tắt !!!");
    }
    else{
      Serial.println("Invalid payload received !!!");
    }
  }
  else if (String(topic) == topic_subscribe_force_alert){
    Serial.println("Handling Force Alert Mode ...");
    if(!systemActive){
      Serial.println("Hệ thống đang tắt! Không thể kích hoạt Force Alert !!!");
      publishSystemWarning("Hệ thống đang tắt! Không thể kích hoạt Force Alert !!!");
      return;
    }

    if (stMessage == "true"){
      forceAlert = true;
      isAlarming = true; // Báo động luôn được kích hoạt
      lastAlarmTime = millis();
      publishForceAlertNotification(true);
      Serial.println("Chế độ Force Alert đã được kích hoạt !!!");
    }
    else if (stMessage == "false"){
      isAlarming = false;
      forceAlert = false;
      resetMode(); // đặt lại hệ thống về trạng thái ban đầu
      publishForceAlertNotification(false);
      Serial.println("Chế độ Force Alert đã tắt !!!");
    }
    else{
      Serial.println("Invalid payload received !!!");
    }
  }
  else if (String(topic) == topic_subscribe_streaming_video){
    Serial.println("Handling Streaming Video");
    if(!systemActive){
      Serial.println("Hệ thống đang tắt! Không thể Streaming Video!!!");
      publishSystemWarning("Hệ thống đang tắt! Không thể Streaming Video!!!");
      return;
    }

    if (stMessage == "true"){
      streamingActive = true;
      Serial.println("Streaming video activated !!!");
    }
    else if (stMessage == "false"){
      streamingActive = false;
      Serial.println("Streaming Video deactivated!!!");
    }
    else{
      Serial.println("Invalid payload received !!!");
    }
  }
  else if (String(topic) == topic_subscribe_take_a_photo){
    if (stMessage == "true"){
      take_a_photo = true;
      Serial.println("Hệ thống bắt đầu chụp ảnh ....");
    }
    else if (stMessage == "false"){
      take_a_photo = "false";
      Serial.println("Hệ thống hết chụp ảnh ...");
    }
    else{
      Serial.println("Invalid payload received !!!");
    }
  }
  else if (String(topic) == topic_subscribe_scheduler){
    Serial.println("Handling Active System via Scheduler ...");
    if (stMessage == "true"){
      schedulerActive = true;
      Serial.println("Hệ thống đã đặt lịch thành công...");
    }
    else if (stMessage == "false"){
      schedulerActive = false;
      Serial.println("Hệ thống chưa đặt lịch cho sẵn ...");
    }
    else{
      Serial.println("Invalid payload received !!!");
    }
  }
  else if (String(topic) == topic_subscribe_get_auto_image){
    Serial.println("Handling Get Auto Images ...");
    if (stMessage == "true"){
      Serial.println("Hệ thống đã tải ảnh vào database ...");
      publishHistoryData(false, true);
    }
    else if (stMessage == "false"){
      Serial.println("Hệ thống không tải ảnh vào database ...");
    }
    else{
      Serial.println("Invalid payload received !!!");
    }
  }
}

// ======== Server Connection Handler Task ==========================
void mjpegCB(void* pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(WSINTERVAL);

  // Creating frame synchronization semaphore and initializing it
  frameSync = xSemaphoreCreateBinary();
  xSemaphoreGive( frameSync );

  // Creating a queue to track all connected clients
  streamingClients = xQueueCreate( 10, sizeof(WiFiClient*) );

  //=== setup section  ==================

  //  Creating RTOS task for grabbing frames from the camera
  xTaskCreatePinnedToCore(
    camCB,        // callback
    "cam",        // name
    4096,         // stacj size
    NULL,         // parameters
    2,            // priority
    &tCam,        // RTOS task handle
    APP_CPU);     // core

  //  Creating task to push the stream to all connected clients
  xTaskCreatePinnedToCore(
    streamCB,
    "strmCB",
    4 * 1024,
    NULL, //(void*) handler,
    2,
    &tStream,
    APP_CPU);

  //  Registering webserver handling routines
  server.on("/mjpeg/1", HTTP_GET, handleJPGSstream);
  server.on("/jpg", HTTP_GET, handleJPG);
  server.onNotFound(handleNotFound);

  //  Starting webserver
  server.begin();

  //=== loop() section  ===================
  xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    server.handleClient();

    //  After every server client handling request, we let other tasks run and then pause
    taskYIELD();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// Commonly used variables:
volatile size_t camSize;    // size of the current frame, byte
volatile char* camBuf;      // pointer to the current frame

// ==== RTOS task to grab frames from the camera =========================
void camCB(void* pvParameters) {

  TickType_t xLastWakeTime;

  //  A running interval associated with currently desired frame rate
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);

  // Mutex for the critical section of swithing the active frames around
  portMUX_TYPE xSemaphore = portMUX_INITIALIZER_UNLOCKED;

  //  Pointers to the 2 frames, their respective sizes and index of the current frame
  char* fbs[2] = { NULL, NULL };
  size_t fSize[2] = { 0, 0 };
  int ifb = 0;

  //=== loop() section  ===================
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {

    //  Grab a frame from the camera and query its size
    cam.run();
    size_t s = cam.getSize();

    //  If frame size is more that we have previously allocated - request  125% of the current frame space
    if (s > fSize[ifb]) {
      fSize[ifb] = s * 4 / 3;
      fbs[ifb] = allocateMemory(fbs[ifb], fSize[ifb]);
    }

    //  Copy current frame into local buffer
    char* b = (char*) cam.getfb();
    memcpy(fbs[ifb], b, s);

    //  Let other tasks run and wait until the end of the current frame rate interval (if any time left)
    taskYIELD();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    //  Only switch frames around if no frame is currently being streamed to a client
    //  Wait on a semaphore until client operation completes
    xSemaphoreTake( frameSync, portMAX_DELAY );

    //  Do not allow interrupts while switching the current frame
    portENTER_CRITICAL(&xSemaphore);
    camBuf = fbs[ifb];
    camSize = s;
    ifb++;
    ifb &= 1;  // this should produce 1, 0, 1, 0, 1 ... sequence
    portEXIT_CRITICAL(&xSemaphore);

    //  Let anyone waiting for a frame know that the frame is ready
    xSemaphoreGive( frameSync );

    //  Technically only needed once: let the streaming task know that we have at least one frame
    //  and it could start sending frames to the clients, if any
    xTaskNotifyGive( tStream );

    //  Immediately let other (streaming) tasks run
    taskYIELD();

    //  If streaming task has suspended itself (no active clients to stream to)
    //  there is no need to grab frames from the camera. We can save some juice
    //  by suspedning the tasks
    if ( eTaskGetState( tStream ) == eSuspended ) {
      vTaskSuspend(NULL);  // passing NULL means "suspend yourself"
    }
  }
}

// ==== Memory allocator that takes advantage of PSRAM if present =======================
char* allocateMemory(char* aPtr, size_t aSize) {

  //  Since current buffer is too smal, free it
  if (aPtr != NULL) free(aPtr);


  size_t freeHeap = ESP.getFreeHeap();
  char* ptr = NULL;

  // If memory requested is more than 2/3 of the currently free heap, try PSRAM immediately
  if ( aSize > freeHeap * 2 / 3 ) {
    if ( psramFound() && ESP.getFreePsram() > aSize ) {
      ptr = (char*) ps_malloc(aSize);
    }
  }
  else {
    //  Enough free heap - let's try allocating fast RAM as a buffer
    ptr = (char*) malloc(aSize);

    //  If allocation on the heap failed, let's give PSRAM one more chance:
    if ( ptr == NULL && psramFound() && ESP.getFreePsram() > aSize) {
      ptr = (char*) ps_malloc(aSize);
    }
  }

  // Finally, if the memory pointer is NULL, we were not able to allocate any memory, and that is a terminal condition.
  if (ptr == NULL) {
    ESP.restart();
  }
  return ptr;
}


// ==== STREAMING ======================================================
const char HEADER[] = "HTTP/1.1 200 OK\r\n" \
                      "Access-Control-Allow-Origin: *\r\n" \
                      "Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321\r\n";
const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";
const int hdrLen = strlen(HEADER);
const int bdrLen = strlen(BOUNDARY);
const int cntLen = strlen(CTNTTYPE);


// ==== Handle connection request from clients ===============================
void handleJPGSstream(void)
{
  //  Can only acommodate 10 clients. The limit is a default for WiFi connections
  if ( !uxQueueSpacesAvailable(streamingClients) ) return;


  //  Create a new WiFi Client object to keep track of this one
  WiFiClient* client = new WiFiClient();
  *client = server.client();

  //  Immediately send this client a header
  client->write(HEADER, hdrLen);
  client->write(BOUNDARY, bdrLen);

  // Push the client to the streaming queue
  xQueueSend(streamingClients, (void *) &client, 0);

  // Wake up streaming tasks, if they were previously suspended:
  if ( eTaskGetState( tCam ) == eSuspended ) vTaskResume( tCam );
  if ( eTaskGetState( tStream ) == eSuspended ) vTaskResume( tStream );
}

// ==== Actually stream content to all connected clients ========================
void streamCB(void * pvParameters) {
  char buf[16];
  TickType_t xLastWakeTime;
  TickType_t xFrequency;

  //  Wait until the first frame is captured and there is something to send
  //  to clients
  ulTaskNotifyTake( pdTRUE,          /* Clear the notification value before exiting. */
                    portMAX_DELAY ); /* Block indefinitely. */

  xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    // Default assumption we are running according to the FPS
    xFrequency = pdMS_TO_TICKS(1000 / FPS);

    //  Only bother to send anything if there is someone watching
    UBaseType_t activeClients = uxQueueMessagesWaiting(streamingClients);
    if ( activeClients ) {
      // Adjust the period to the number of connected clients
      xFrequency /= activeClients;

      //  Since we are sending the same frame to everyone,
      //  pop a client from the the front of the queue
      WiFiClient *client;
      xQueueReceive (streamingClients, (void*) &client, 0);

      //  Check if this client is still connected.

      if (!client->connected()) {
        //  delete this client reference if s/he has disconnected
        //  and don't put it back on the queue anymore. Bye!
        delete client;
      }
      else {

        //  Ok. This is an actively connected client.
        //  Let's grab a semaphore to prevent frame changes while we
        //  are serving this frame
        xSemaphoreTake( frameSync, portMAX_DELAY );

        client->write(CTNTTYPE, cntLen);
        sprintf(buf, "%d\r\n\r\n", camSize);
        client->write(buf, strlen(buf));
        client->write((char*) camBuf, (size_t)camSize);
        client->write(BOUNDARY, bdrLen);

        // Since this client is still connected, push it to the end
        // of the queue for further processing
        xQueueSend(streamingClients, (void *) &client, 0);

        //  The frame has been served. Release the semaphore and let other tasks run.
        //  If there is a frame switch ready, it will happen now in between frames
        xSemaphoreGive( frameSync );
        taskYIELD();
      }
    }
    else {
      //  Since there are no connected clients, there is no reason to waste battery running
      vTaskSuspend(NULL);
    }
    //  Let other tasks run after serving every client
    taskYIELD();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

const char JHEADER[] = "HTTP/1.1 200 OK\r\n" \
                       "Content-disposition: inline; filename=capture.jpg\r\n" \
                       "Content-type: image/jpeg\r\n\r\n";
const int jhdLen = strlen(JHEADER);

// ==== Serve up one JPEG frame =============================================
void handleJPG(void)
{
  WiFiClient client = server.client();

  if (!client.connected()) return;
  cam.run();
  client.write(JHEADER, jhdLen);
  client.write((char*)cam.getfb(), cam.getSize());
}


// ==== Handle invalid URL requests ============================================
void handleNotFound()
{
  String message = "Server is running!\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  server.send(200, "text / plain", message);
}

void cameraSetup(){
  // Configure the camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  if (cam.init(config) != ESP_OK) {
    Serial.println("Error initializing the camera");
    delay(10000);
    ESP.restart();
  }
}

// Hàm hỗ trợ
int readAverageLight() {
  int total = 0;
  for (int i = 0; i < numSamples; i++) {
    total += analogRead(photoresistorPin);
  }
  return total / numSamples; // giá trị trung bình
}

void blink_led_and_buzzer(int led_pin, int buzzer_pin, long delay_time) {
  static unsigned long previousMillis = 0;
  static bool state = LOW;
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= delay_time) {
    previousMillis = currentMillis;
    state = !state;
    digitalWrite(led_pin, state);
    if (state) {
      tone(buzzer_pin, 1000); // Buzzer kêu
    } else {
      noTone(buzzer_pin); // Buzzer tắt
    }
  }
}

void control_flash_brightness(int light_value) {
  // Chuyển đổi giá trị ánh sáng (0 - 4095) sang độ sáng PWM (0 - 255)
  int brightness = map(light_value, 0, MAX_ANALOG_READ, MAX_BRIGHTNESS, 0);
  // light_value càng nhỏ -> brightness càng lớn (đèn sáng hơn)
  brightness = constrain(brightness, 0, MAX_BRIGHTNESS); // Giới hạn trong khoảng PWM 0-255
  analogWrite(flashPin, brightness); // Điều khiển độ sáng đèn flash
}

void force_alert_mode(){
  Serial.println("Chế độ Force Alert: Báo động liên tục !!!");
  digitalWrite(ledRedPin, HIGH);
  tone(buzzerPin, 1000);

  publishSystemMode("Mode 3: Emergency Warnings System Mode !!!");
}

void publishGetStreamLink(const String& message){
  if (!client.connected()){
    Serial.println("MQTT không được kết nối. Đang kết nối lại ....");
    connectMqttServer();
  }

  bool success = client.publish(topic_publishing_get_string_stream_link, message.c_str()); // publish hệ thống đang ở chế độ nào

  if (success) {
    Serial.print("✅ Thông báo đã được gửi thành công: ");
    Serial.println(message);
  } else {
    Serial.print("❌ Lỗi khi gửi thông báo: ");
    Serial.println(message);
  }
}

void publishTakeAPhotoNotification(const String& message){
  if (!client.connected()){
    Serial.println("MQTT không được kết nối. Đang kết nối lại ....");
    connectMqttServer();
  }

  bool success = client.publish(topic_publishing_take_a_photo, message.c_str()); // publish hệ thống đang ở chế độ nào

  if (success) {
    Serial.print("✅ Thông báo đã được gửi thành công: ");
    Serial.println(message);
  } else {
    Serial.print("❌ Lỗi khi gửi thông báo: ");
    Serial.println(message);
  }
}

void publishGetImageLink(const String& message){
  if (!client.connected()){
    Serial.println("MQTT không được kết nối. Đang kết nối lại ....");
    connectMqttServer();
  }

  bool success = client.publish(topic_publishing_get_string_image_link, message.c_str()); // publish hệ thống đang ở chế độ nào

  if (success) {
    Serial.print("✅ Thông báo đã được gửi thành công: ");
    Serial.println(message);
  } else {
    Serial.print("❌ Lỗi khi gửi thông báo: ");
    Serial.println(message);
  }
}

// Cài đặt ban đầu
void setup() {
  pinMode(pirPin, INPUT);           // cảm biến chuyển động
  pinMode(ledRedPin, OUTPUT);       // LED đỏ
  pinMode(ledGreenPin, OUTPUT);     // LED xanh
  pinMode(buzzerPin, OUTPUT);       // buzzer
  pinMode(flashPin, OUTPUT);        // flash
  pinMode(photoresistorPin, INPUT); // photoresistor

  Serial.println("ESP32-CAM setup complete.");

  Serial.begin(115200);
  delay(1000); // wait for a second to let Serial connect

  cameraSetup();
  WifiConnect();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  client.setBufferSize(MAX_PAYLOAD_SIZE);
  
  IPAddress ip;

  ip = WiFi.localIP();
  Serial.println(F("WiFi connected"));
  Serial.println("");
  Serial.print("Stream Link: http://");
  Serial.print(ip);
  Serial.println("/mjpeg/1");
  Serial.print("Image Link: http://");
  Serial.print(ip);
  Serial.println("/jpg");

  String stream_link = "http://" + WiFi.localIP().toString() + "/mjpeg/1";
  publishGetStreamLink(stream_link);
  String image_link = "http://" + WiFi.localIP().toString() + "/jpg";
  publishGetImageLink(image_link);

  // Start mainstreaming RTOS task
  xTaskCreatePinnedToCore(
    mjpegCB,
    "mjpeg",
    4 * 1024,
    NULL,
    2,
    &tMjpeg,
    APP_CPU);
}

// Chế độ an toàn
void mode1() {
  isAlarming = false; // Tắt trạng thái báo động
  digitalWrite(ledGreenPin, HIGH);
  digitalWrite(ledRedPin, LOW);
  noTone(buzzerPin); // Tắt buzzer
  Serial.println("Không phát hiện chuyển động. Hệ thống an toàn !!!");
  publishSystemMode("Mode 1: Safe System Mode !!!");
}

// Chế độ báo động
void mode2() {
  unsigned long currentMillis = millis();

  Serial.println("Phát hiện trộm, báo động ....");
  digitalWrite(ledGreenPin, LOW); // Tắt LED xanh
  
  // Nhấp nháy LED đỏ và bật/tắt buzzer
  blink_led_and_buzzer(ledRedPin, buzzerPin, led_delay_time);
  publishHistoryData(true, false);

  if (!emailSent && currentMillis - mode2StartTime >= emailDelay){
    publishEmail();
  }

  if (currentMillis - lastAlarmTime >= alarmDuration) {
    Serial.println("Báo động đã kết thúc.");
    mode1(); // Trở lại chế độ an toàn
    mode2StartTime = 0;
    return;
  }
  
  publishSystemMode("Mode 2: Alert System Mode !!!");
}

// Test
void publishFlash(bool flashActive) {
  char buffer[10]; // Sufficient buffer for "true" or "false"

  // Convert flashActive to "true" or "false"
  sprintf(buffer, "%s", flashActive ? "true" : "false");

  // Publish the flash status to the MQTT topic
  client.publish(topic_publishing_control_flash, buffer);
}

void publishPhotoresistor(int isDay){
  char buffer[10];
  sprintf(buffer, "%d", isDay);
  client.publish("/HomeDefender/Photoresistor", buffer); // Topic publish trạng thái
}

void publishWiFiStatus(bool isConnected){
  char buffer[10];
  sprintf(buffer, "%s", isConnected ? "true" : "false");
  client.publish(topic_publishing_wifi_status, buffer);
}

void publishMQTTStatus(bool isConnected){
  char buffer[10];
  sprintf(buffer, "%s", isConnected ? "true" : "false");
  client.publish(topic_publishing_mqtt_status, buffer);
}

void publishForceAlertNotification(bool forceAlertActive){
  char buffer[10];
  sprintf(buffer, "%s", forceAlertActive ? "true" : "false");
  client.publish(topic_publishing_force_alert, buffer);
}

void checkDayOrNight(int light_value){
  if (light_value > photoresistor_threshold){
    isDay = 1;
    isNight = 0;
  }
  else{
    isDay = 0;
    isNight = 1;
  }
}

void publishSystemStatus(SystemStatus status){
  String message;
  switch(status){
    case OFF:
      message = "System is OFF mode";
      break;
    case SCHEDULED:
      message = "System is in SCHEDULED mode";
      break;
    case ALWAYS_ON:
      message = "System is ALWAYS ON mode";
      break;
  }
  client.publish(topic_publishing_activate_system_status, message.c_str());
  Serial.println("Sent status: " + message);
}

void publishEmail() {
  if (!emailSent){
    Serial.println("Gửi email cảnh báo ...");

    char buffer[10];
    sprintf(buffer, "%d", true);
    client.publish(topic_publishing_email_warnings, buffer);

    publishEmailNotification("Email cảnh báo đã được gửi đến bạn !!!");
    emailSent = true;
  }
}

void checkWifiConnection() {
  bool currentWiFiStatus = (WiFi.status() == WL_CONNECTED);

  if (!currentWiFiStatus) {
    Serial.println("WiFi disconnected. Reconnecting...");
    resetMode();
    while (!currentWiFiStatus) {
      blink_led_and_buzzer(ledGreenPin, buzzerPin, 500);
      WifiConnect();
      currentWiFiStatus = (WiFi.status() == WL_CONNECTED);
      delay(500);
    }
  }
  else{
    if (currentWiFiStatus != previousWiFiStatus) {
    previousWiFiStatus = currentWiFiStatus;
    publishWiFiStatus(currentWiFiStatus);
    }
  }
}

void publishSystemMode(const String& message){
  if (!client.connected()){
    Serial.println("MQTT không được kết nối. Đang kết nối lại ....");
    connectMqttServer();
  }

  bool success = client.publish(topic_publishing_system_mode, message.c_str()); // publish hệ thống đang ở chế độ nào

  if (success) {
    Serial.print("✅ Thông báo đã được gửi thành công: ");
    Serial.println(message);
  } else {
    Serial.print("❌ Lỗi khi gửi thông báo: ");
    Serial.println(message);
  }
}

void publishSystemWarning(const String& message){
  if (!client.connected()){
    Serial.println("MQTT không được kết nối. Đang kết nối lại ....");
    connectMqttServer();
  }

  bool success = client.publish(topic_system_warning, message.c_str()); // publish hệ thống đang ở chế độ nào

  if (success) {
    Serial.print("✅ Thông báo đã được gửi thành công: ");
    Serial.println(message);
  } else {
    Serial.print("❌ Lỗi khi gửi thông báo: ");
    Serial.println(message);
  }
}

void publishEmailNotification(const char* message){
  if (!client.connected()){
    Serial.println("MQTT không được kết nối. Đang kết nối lại ....");
    connectMqttServer();
  }

  bool success = client.publish(topic_publishing_email_notification, message);

  if (success) {
    Serial.print("✅ Thông báo đã được gửi thành công: ");
    Serial.println(message);
  } else {
    Serial.print("❌ Lỗi khi gửi thông báo: ");
    Serial.println(message);
  }
}

void checkMqttConnection() {
  bool currentMqttStatus = client.connected();

  if (!currentMqttStatus) {
    Serial.println("MQTT disconnected. Reconnecting...");
    resetMode();
    while (!client.connected()) {
      blink_led_and_buzzer(ledGreenPin, buzzerPin, 500);
      connectMqttServer();
      if (!client.connected()) {
        Serial.println("Unable to reconnect to MQTT. Retrying later...");
        delay(3000);
      }
    }
  }

  if (currentMqttStatus != previousMQTTStatus) {
    publishMQTTStatus(currentMqttStatus);
    previousMQTTStatus = currentMqttStatus;
  }
}

void activateSystem(){
  // digitalWrite(pirPin, HIGH); // Kích hoạt cảm biến PIR
  int pirState = digitalRead(pirPin); // Đọc tín hiệu từ PIR Sensor

  // Xử lý HTTP request từ server (chỉ khi hệ thống hoạt động)
  server.handleClient();

  if (forceAlert){
    force_alert_mode(); // kích hoạt chế độ force alert
    publishHistoryData(true, false);
    publishEmail();
    return; // không cần xử lý các chế độ khác nhau nếu force alert đang kích hoạt
  }

  Serial.println(take_a_photo);
  if (take_a_photo){
    take_a_photo = false;
    publishHistoryData(false, true);
    publishTakeAPhotoNotification("You have taken and saved image successfully !!!"); // in thông báo đã chụp hình
  }

  int current_photoresistor_value = readAverageLight(); // Đọc giá trị ánh sáng

  checkDayOrNight(current_photoresistor_value);

  publishPhotoresistor(isDay);
  
  if (flashActive){
    control_flash_brightness(current_photoresistor_value);
  }
  else{
    analogWrite(flashPin, 0);
  }

  if (flashActive != previousFlashState){
    publishFlash(flashActive);
    previousFlashState = flashActive;
  }

  if (pirState == HIGH) { 
    // Nếu phát hiện chuyển động
    if (!isAlarming) {
      isAlarming = true;
      lastAlarmTime = millis(); // Bắt đầu báo động
    } 
    mode2(); // Gọi chế độ báo động
  } else{
    if(isAlarming && millis() - lastAlarmTime >= alarmDuration){
      isAlarming = false;
    }
    if (!isAlarming){
      mode1();
    }
  }
}

// Vòng lặp chính
void loop() {
  // Kiểm tra kết nối server MQTT
  checkMqttConnection();

  // Kiểm tra kết nối WiFi
  checkWifiConnection();

  // xử lý MQTT
  client.loop();

  vTaskDelay(1000);
  
  // Xử lý logic hệ thống
  if (!systemActive && !schedulerActive) {
    // Hệ thống tắt hoàn toàn
    if (currentStatus != OFF) {
      currentStatus = OFF;
      publishSystemStatus(currentStatus);
      Serial.println("Hệ thống đang tắt hoàn toàn.");
    }
    resetMode();
  }
  else if (!systemActive && schedulerActive) {
    // Hệ thống hoạt động theo lịch
    if (currentStatus != SCHEDULED) {
      currentStatus = SCHEDULED;
      publishSystemStatus(currentStatus);
      Serial.println("Hệ thống hoạt động theo lịch.");
    }
    activateSystem();
  } 
  else if (systemActive) {
    // Hệ thống hoạt động 24/24
    if (currentStatus != ALWAYS_ON) {
      currentStatus = ALWAYS_ON;
      publishSystemStatus(currentStatus);
      Serial.println("Hệ thống hoạt động 24/24.");
    }
    activateSystem(); // Gọi hàm liên tục để xử lý
  }
}
