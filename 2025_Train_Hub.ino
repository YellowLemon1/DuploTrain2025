// Basic code to interact with Lego DUPLO Train models from 2025
// LEGO part numbers 10427 or 10428
// Works in Arduino IDE; needs NimBLEDevice, I used version 1.4.3
// I tried using Legoino but it seems broken in 2025 due to newer dependency versions

#include <NimBLEDevice.h>

// The Lego Wireless Protocol 3 (LWP3) Service and Characteristic UUID's
#define SVC_UUID "00001623-1212-efde-1623-785feabcd123"
#define CHR_UUID "00001624-1212-efde-1623-785feabcd123"

NimBLERemoteCharacteristic* pRemoteChar;
bool connected = false;

// Basic commands
uint8_t activateCmd[] = {0x05, 0x00, 0x01, 0x02, 0x02}; 
uint8_t cmdHorn[]     = {0x0A, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x07, 0x01, 0x01};
uint8_t cmdStop[]     = {0x08, 0x00, 0x81, 0x32, 0x01, 0x51, 0x00, 0x00};

// Hub needs to be activated by subscribing to button events
void activateHub() {
    Serial.println("Activating Hub (Enabling notifications)...");
    pRemoteChar->writeValue(activateCmd, sizeof(activateCmd), true);
}

// Function to handle the renaming via Hub Property 0x01
void renameHub(const char* newName) {

    size_t nameLen = strlen(newName); 

    uint8_t totalLen = (uint8_t)(nameLen + 5); 
    
    uint8_t* pkt = new uint8_t[totalLen];
    pkt[0] = totalLen;  // Length
    pkt[1] = 0x00;      // Hub ID
    pkt[2] = 0x01;      // Msg Type: Hub Property
    pkt[3] = 0x01;      // Property: Advertising Name
    pkt[4] = 0x01;      // Operation: Set (Write)
    

    memcpy(&pkt[5], newName, nameLen);

    Serial.printf("Renaming hub to '%s' (%d bytes total)...\n", newName, totalLen);
    
    if(pRemoteChar->writeValue(pkt, totalLen, true)) {
        Serial.println("Success! Reset the train to see the clean name.");
    } else {
        Serial.println("Failed.");
    }
    
    delete[] pkt;
}


void sendLegoCommand(uint8_t* cmd, size_t len) {
    if (connected && pRemoteChar) {
        pRemoteChar->writeValue(cmd, len, false);
        Serial.println("Command Sent.");
    }
}


// ... Standard NimBLE Client Callbacks ...
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) { Serial.println("Connected!"); }
    void onDisconnect(NimBLEClient* pClient) { connected = false; Serial.println("Disconnected."); }
};

void setup() {
    Serial.begin(115200);
    NimBLEDevice::init("");
    Serial.println("Scanning for DUPLO Train...");
}

void loop() {
    if (!connected) {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        NimBLEScanResults results = pScan->start(5, false);

        for (int i = 0; i < results.getCount(); i++) {
            NimBLEAdvertisedDevice device = results.getDevice(i);
            if (device.haveManufacturerData()) {
                std::string mfg = device.getManufacturerData();
                // Match LEGO (0x97 0x03) and Duplo Hub (0x21)
                if (mfg.length() >= 4 && (uint8_t)mfg[0] == 0x97 && (uint8_t)mfg[1] == 0x03 && (uint8_t)mfg[3] == 0x21) {
                    NimBLEClient* pClient = NimBLEDevice::createClient();
                    pClient->setClientCallbacks(new ClientCallbacks());
                    if (pClient->connect(&device)) {
                        auto pSvc = pClient->getService(SVC_UUID);
                        pRemoteChar = pSvc ? pSvc->getCharacteristic(CHR_UUID) : nullptr;
                        if (pRemoteChar) {
                            connected = true;
                            activateHub(); 
                            Serial.println("System Ready.");
                            Serial.println("Commands: 'h'=Horn, 's'=Stop, 'r'=Rename to DUPLO Train");
                            break;
                        }
                    }
                }
            }
        }
    }

    if (Serial.available()) {
        char c = Serial.read();
        switch(c) {
            case 'h': sendLegoCommand(cmdHorn, 10); break;
            case 's': sendLegoCommand(cmdStop, 8); break;
            case 'r': renameHub("DUPLO Train"); break;

          
        }
    }
    delay(10);
}
