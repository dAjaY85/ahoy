//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __IMPROV_H__
#define __IMPROV_H__

#include <cstring>
#include <functional>
#include "dbg.h"

// https://www.improv-wifi.com/serial/
// https://github.com/jnthas/improv-wifi-demo/blob/main/src/esp32-wifiimprov/esp32-wifiimprov.ino

// configure ESP through Serial interface
class Improv {
    public:
        void setup(const char *devName, const char *version) {
            mDevName = devName;
            mVersion = version;
        }

        void tickSerial(void) {
            if(Serial.available() == 0)
                return;

            uint8_t buf[40];
            uint8_t len = Serial.readBytes(buf, 40);

            if(!checkPaket(&buf[0], len, [this](uint8_t type, uint8_t buf[], uint8_t len) {
                parsePayload(type, buf, len);
            })) {
                DBGPRINTLN(F("check paket failed"));
            }
            dumpBuf(buf, len);
        }

    private:
        enum State : uint8_t {
            STATE_STOPPED                = 0x00,
            STATE_AWAITING_AUTHORIZATION = 0x01,
            STATE_AUTHORIZED             = 0x02,
            STATE_PROVISIONING           = 0x03,
            STATE_PROVISIONED            = 0x04,
        };

        enum Command : uint8_t {
            UNKNOWN           = 0x00,
            WIFI_SETTINGS     = 0x01,
            IDENTIFY          = 0x02,
            GET_CURRENT_STATE = 0x02,
            GET_DEVICE_INFO   = 0x03,
            GET_WIFI_NETWORKS = 0x04,
            BAD_CHECKSUM      = 0xFF,
        };

        enum ImprovSerialType : uint8_t {
            TYPE_CURRENT_STATE = 0x01,
            TYPE_ERROR_STATE   = 0x02,
            TYPE_RPC           = 0x03,
            TYPE_RPC_RESPONSE  = 0x04
        };

        void dumpBuf(uint8_t buf[], uint8_t len) {
            for(uint8_t i = 0; i < len; i++) {
                DHEX(buf[i], false);
                DBGPRINT(" ", false);
            }
            DBGPRINTLN("", false);
        }

        inline uint8_t buildChecksum(uint8_t buf[], uint8_t len) {
            uint8_t calc = 0;
            for(uint8_t i = 0; i < len; i++) {
                calc += buf[i];
            }
            return calc;
        }

        inline bool checkChecksum(uint8_t buf[], uint8_t len) {
            /*DHEX(buf[len], false);
            DBGPRINT(F(" == "), false);
            DBGHEXLN(buildChecksum(buf, len), false);*/
            return ((buildChecksum(buf, len)) == buf[len]);
        }

        bool checkPaket(uint8_t buf[], uint8_t len, std::function<void(uint8_t type, uint8_t b[], uint8_t l)> cb) {
            if(len < 11)
                return false;

            if(0 != strncmp((char*)buf, "IMPROV", 6))
                return false;

            // verison check (only version 1 is supported!)
            if(0x01 != buf[6])
                return false;

            if(!checkChecksum(buf, (9 + buf[8])))
                return false;

            cb(buf[7], &buf[9], buf[8]);

            return true;
        }

        uint8_t char2Improv(const char *str, uint8_t buf[]) {
            uint8_t len = strlen(str);
            buf[0] = len;
            for(uint8_t i = 1; i <= len; i++) {
                buf[i] = (uint8_t)str[i-1];
            }
            return len + 1;
        }

        void sendDevInfo(void) {
            uint8_t buf[100];
            buf[7] = TYPE_RPC_RESPONSE;
            buf[9] = GET_DEVICE_INFO; // repsonse to cmd
            uint8_t p = 11;
            // firmware name
            p += char2Improv("AhoyDTU", &buf[p]);
            // firmware version
            p += char2Improv(mVersion, &buf[p]);
            // chip variant
            #if defined(ESP32)
                p += char2Improv("ESP32", &buf[p]);
            #else
                p += char2Improv("ESP8266", &buf[p]);
            #endif
            // device name
            p += char2Improv(mDevName, &buf[p]);

            buf[10] = p - 11; // sub length
            buf[8] = p - 9; // paket length

            sendPaket(buf, p);
        }

        void setState(uint8_t state) {
            uint8_t buf[20];
            buf[7] = TYPE_CURRENT_STATE;
            buf[8] = 0x01;
            buf[9] = state;
            sendPaket(buf, 10);
        }

        void sendPaket(uint8_t buf[], uint8_t len) {
            buf[0] = 'I';
            buf[1] = 'M';
            buf[2] = 'P';
            buf[3] = 'R';
            buf[4] = 'O';
            buf[5] = 'V';
            buf[6] = 1; // protocol version

            buf[len] = buildChecksum(buf, len);
            len++;
            Serial.write(buf, len);
            dumpBuf(buf, len);
        }

        void parsePayload(uint8_t type, uint8_t buf[], uint8_t len) {
            if(TYPE_RPC == type) {
                if(GET_CURRENT_STATE == buf[0])
                    setState(STATE_AUTHORIZED);
                else if(GET_DEVICE_INFO == buf[0])
                    sendDevInfo();
            }
        }

        const char *mDevName, *mVersion;
};

#endif /*__IMPROV_H__*/
