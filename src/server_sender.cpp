#include "server_sender.h"

const String serverName = "detect.roboflow.com";
const String serverPath = "/jogo-da-velha/1?api_key=90XfuO2de4MjvmkrPI8G";
const int serverPort = 80;
WiFiClientSecure client;

String photo2Base64()
{
    Serial.println("Capturing and converting photo to base64...");
    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Camera capture failed");
        return "";
    }

    String imageFile = "data:image/jpeg;base64,";
    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    for (int i = 0; i < fb->len; i++)
    {
        base64_encode(output, (input++), 3);
        if (i % 3 == 0)
            imageFile += urlencode(String(output));
    }

    esp_camera_fb_return(fb);

    return imageFile;
}

// https://www.arduino.cc/reference/en/libraries/urlencode/
String urlencode(String str)
{
    const char *msg = str.c_str();
    const char *hex = "0123456789ABCDEF";
    String encodedMsg = "";
    while (*msg != '\0')
    {
        if (('a' <= *msg && *msg <= 'z') || ('A' <= *msg && *msg <= 'Z') || ('0' <= *msg && *msg <= '9') || *msg == '-' || *msg == '_' || *msg == '.' || *msg == '~')
        {
            encodedMsg += *msg;
        }
        else
        {
            encodedMsg += '%';
            encodedMsg += hex[(unsigned char)*msg >> 4];
            encodedMsg += hex[*msg & 0xf];
        }
        msg++;
    }
    return encodedMsg;
}

String sendPhoto()
{
    String getAll;
    String getBody;

    String imageFile = photo2Base64();

    uint32_t dataLength = imageFile.length();
    int chunkSize = 1024;
    Serial.printf("Image size: %d\n", dataLength);

    Serial.println("Connecting to server: " + serverName);
    client.setInsecure();
    client.setTimeout(20000);

    if (client.connect(serverName.c_str(), serverPort))
    {
        Serial.println("Connection successful!");

        client.println("POST " + serverPath + " HTTP/1.1");
        client.println("Host: " + serverName);
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.println("Content-Length: " + String(dataLength));
        client.println();
        // send file in chunks of 1024
        for (int i = 0; i < dataLength; i += chunkSize)
        {
            int thisChunkSize = ((i + chunkSize) > dataLength) ? (dataLength - i) : chunkSize;
            client.write((const uint8_t *)imageFile.substring(i, i + thisChunkSize).c_str(), thisChunkSize);
        }

        int timeoutTimer = 30000;
        long startTimer = millis();
        bool state = false;

        while ((startTimer + timeoutTimer) > millis())
        {
            Serial.print(".");
            delay(100);
            while (client.available())
            {
                char c = client.read();
                if (c == '\n')
                {
                    if (getAll.length() == 0)
                    {
                        state = true;
                    }
                    getAll = "";
                }
                else if (c != '\r')
                {
                    getAll += String(c);
                }
                if (state == true)
                {
                    getBody += String(c);
                }
                startTimer = millis();
            }

            if (getBody.length() > 0)
            {
                break;
            }
        }
        Serial.println();
        client.stop();
        Serial.println(getBody);
    }
    else
    {
        getBody = "Connection to " + serverName + " failed.";
        Serial.println(getBody);
    }
    return getBody;
}
