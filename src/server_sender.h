#include "Arduino.h"
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "Base64.h"

String photo2Base64();
String urlencode(String str);
String sendPhoto();