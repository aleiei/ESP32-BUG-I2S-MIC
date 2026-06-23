/*
   Copyright (C) 2021 Alessandro Orlando

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as published
   by the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program. If not, see <https://www.gnu.org/licenses/>.
   
   Description:
   Simple Bug with I2S INMP441 MEMS microphone and Audio Streaming via UDP
   Under Linux for listener:
   netcat -u -p 16500 -l | play -t s16 -r 48000 -c 2 -
   Under Linux for recorder (give for file.mp3 the name you prefer) : 
   netcat -u -p 16500 -l | rec -t s16 -r 48000 -c 2 - file.mp3
   
*/

#include <Arduino.h>
#include "WiFi.h"
#include "AsyncUDP.h"
#include <driver/i2s.h>
#include <soc/i2s_reg.h>


const char* ssid = "your_ssid";
const char* pswd = "your_password";


IPAddress udpAddress(192, 168, 1, 40);
const int udpPort = 16500;

boolean connected = false;

AsyncUDP udp;

const i2s_port_t I2S_PORT = I2S_NUM_0;
const int block_size = 128;

void setup() {
    Serial.begin(115200);
    Serial.println("Configuring WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pswd);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("WiFi Failed");
        while (1) {
            delay(1000);
        }
    }

    
    Serial.println("Configuring I2S port");
    esp_err_t err;
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), 
        .sample_rate = 48000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, 
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,                    
        .dma_buf_len = block_size,
    };

    
    const i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = -1,
        .data_in_num = 32,
    };

   
    err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed installing driver: %d\n", err);
        while (true);
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Failed setting pin: %d\n", err);
        while (true);
    }
    Serial.println("I2S driver OK");
}

int32_t buffer[512];
volatile size_t rpt = 0;

void i2s_mic()
{
    size_t num_bytes_read = 0;
    i2s_read(I2S_PORT, ((uint8_t*)buffer) + rpt, block_size, &num_bytes_read, portMAX_DELAY);
    rpt += num_bytes_read;
    if (rpt >= sizeof(buffer)) rpt = 0;
}


void loop() {
    static uint8_t state = 0; 
    i2s_mic();

    if (!connected) {
        if (udp.connect(udpAddress, udpPort)) {
            connected = true;
            Serial.println("Connected to UDP Listener");
            Serial.println("Under Linux for listener use: netcat -u -p 16500 -l | play -t s16 -r 48000 -c 2 -");
            Serial.println("Under Linux for recorder use: netcat -u -p 16500 -l | rec -t s16 -r 48000 -c 2 - file.mp3");

        }
    }
    else {
        switch (state) {
            case 0:
                if (rpt > (sizeof(buffer) / 2 - 1)) {
                state = 1;
                }
                break;
            case 1:
                state = 2;
                udp.write(((uint8_t*)buffer), sizeof(buffer) / 2);
                break;
            case 2:
                if (rpt < (sizeof(buffer) / 2)) {
                    state = 3;
                }
                break;
            case 3:
                state = 0;
                udp.write(((uint8_t*)buffer) + (sizeof(buffer) / 2), sizeof(buffer) / 2);
                break;
        }
    }   
}
