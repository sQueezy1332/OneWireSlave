#include "OneWireSlave.h"
#if defined(ARDUINO_ARCH_ESP32)
#define LED 2
#define ibuttonpin 32
#endif
#if defined(__AVR__)
#define LED 2
#define ibuttonpin A5
#endif
#define ledtimer 100
#define ledblinkcounter 5 * 2
#define ignore_errors 0

OneWireSlave ibutton(ibuttonpin);
byte key[8];
byte rom_green[8] = { 0x1, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x0, 0xE1 };
byte rom_red[8]{ 0x1, 0xE7, 0xF7, 0xD1, 0xC, 0x0, 0x0, 0x53 };
byte cmd;
bool led_state = 0;

void setup() {
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    Serial.println("\nReady");
    ibutton.init(rom_green);
}
#if ONEWIRESLAVE_CRC
void crc() {
    if (ibutton.crc8(key, 7) != key[7])
        Serial.print("\tCRC is not valid!");
}
#endif
void led_blink() {
    uint32_t timestamp = 0;
    bool led_state = 0;
    for (byte i = 0; i < ledblinkcounter; i++) {
        timestamp = millis() + ledtimer;
        while (!(millis() > timestamp));
        digitalWrite(LED, (led_state = !led_state));
    }
}
void keyvalidcheck() {
    byte rom[8] = { 0x1, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x0, 0xE1 };
    for (int i = 0; i < 8; i++) {
        if (key[i] != rom[i]) {
            Serial.println();
            return;
        }
    }
    Serial.println("\tKey is valid!");
    led_blink();
}
void printcmd() {
    switch (cmd) {
    case 0xF0: // SEARCH ROM
        Serial.print("SEARCH\t0xF0\t");
        return;
    case 0x33: // READ ROM
        Serial.print("READ\t0x33\t");
        return;
    case 0x55: // MATCH ROM - Choose/Select ROM
        Serial.print("MATCH\t0x55\t");
        return;
    case 0xEC: // ALARM SEARCH
        Serial.print("ALARM\t0xEC\t");
        return;
    case 0xCC:
        Serial.print("SKIP\t0xCC\t");
        return;
    default:
        Serial.print("UNKNOWN\t0x");
        Serial.print(cmd, HEX);
        Serial.print("\t");
    }
}
void printkey() {
    Serial.print("ROM = ");
    /*for (int i = 0; i < 8; i++) {
        Serial.print(' ');
        Serial.print(key[i], HEX);
    }*/
    //for (byte& y : rom_green)
        //&y = { 0x1 0x34 0x5B 0xB7 0xD 0x0 0x0 0xEE };
        //Serial.print(' ');
        //Serial.print(y, HEX);
}

void loop() {
    while (!ibutton.waitForRequest(0)){};
    digitalWrite(LED, HIGH);
    //printkey();
    //delay(1000);
    //led_blink;
    //crc();
    //Serial.println();
    delay(50);
    digitalWrite(LED, LOW);
}
