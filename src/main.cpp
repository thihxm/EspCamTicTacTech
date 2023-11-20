#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"          // Disable brownour problems
#include "soc/rtc_cntl_reg.h" // Disable brownour problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>
#include <HTTPClient.h>

#include "app_cam_esp.h"
#include "server_sender.h"
#include "main.h"

// Replace with your network credentials
const char *ssid = "iPhone di Thiago ";
const char *password = "thiago23";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

bool takeNewPhoto = false;
void capturePhotoSaveSpiffs(void);

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
  </style>
</head>
<body>
  <div id="container">
    <h2>ESP32-CAM Last Photo</h2>
    <p>It might take more than 5 seconds to capture a photo.</p>
    <p>
      <button onclick="rotatePhoto();">ROTATE</button>
      <button onclick="capturePhoto()">CAPTURE PHOTO</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
    </p>
  </div>
  <div><img src="saved-photo" id="photo" width="70%"></div>
</body>
<script>
  var deg = 0;
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
  }
  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 90;
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }
    else{ document.getElementById("container").className = "hori"; }
    img.style.transform = "rotate(" + deg + "deg)";
  }
  function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>
</html>)rawliteral";

void setup()
{
    // Serial port for debugging purposes
    Serial.begin(115200);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    WiFi.mode(WIFI_AP_STA);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        ESP.restart();
    }
    else
    {
        delay(500);
        Serial.println("SPIFFS mounted successfully");
    }

    // Print ESP32 Local IP Address
    Serial.print("IP Address: http://");
    Serial.println(WiFi.localIP());

    // Turn-off the 'brownout detector'
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // OV2640 camera module

    int ret = app_camera_init();
    if (ret != 0)
    {
        Serial.printf("Camera init failed with error");
        ESP.restart();
    }
    Serial.printf("Camera Initialized\n");

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { Serial.println("Requesting index.html");
                request->send_P(200, "text/html", index_html); });

    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo"); });

    server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, FILE_PHOTO, "image/jpg", false); });

    // Start server
    server.begin();
    Serial.println("HTTP server started");
}

void loop()
{

    if (takeNewPhoto)
    {
        Serial.println("Taking a photo...");
        capturePhotoSaveSpiffs();
        // String body = sendPhoto();
        // Serial.println(body);
        takeNewPhoto = false;
    }
    delay(1);
}

// Check if photo capture was successful
bool checkPhoto(fs::FS &fs)
{
    File f_pic = fs.open(FILE_PHOTO);
    unsigned int pic_sz = f_pic.size();
    return (pic_sz > 100);
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs(void)
{
    camera_fb_t *fb = NULL; // pointer
    bool ok = 0;            // Boolean indicating if the picture has been taken correctly

    do
    {
        // Take a photo with the camera
        Serial.println("Taking a photo...");

        fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera capture failed");
            return;
        }

        // Photo file name
        Serial.printf("Picture file name: %s\n", FILE_PHOTO);
        File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

        // Insert the data in the photo file
        if (!file)
        {
            Serial.println("Failed to open file in writing mode");
        }
        else
        {
            file.write(fb->buf, fb->len); // payload (image), payload length

            Serial.print("The picture has been saved in ");
            Serial.print(FILE_PHOTO);
            Serial.print(" - Size: ");
            Serial.print(file.size());
            Serial.println(" bytes");
        }
        // Close the file
        esp_camera_fb_return(fb);

        // check if file has been correctly saved in SPIFFS
        ok = checkPhoto(SPIFFS);
    } while (!ok);
}

int sendPhotoToServer(uint8_t *data, size_t len)
{
    Serial.printf("Sending data (%f)\n", len);
    HTTPClient http;

    http.addHeader("Content-Type", "image/jpeg");

    int httpResponseCode = http.POST(data, len);

    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);

        // TODO: Send this response via ESP-NOW
        // ...
    }
    else
    {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
    }

    http.end();
    return httpResponseCode;
}
