#include <NimBLEDevice.h>

// --- Configuration ---
#define MAX_TRAINS 2
#define RX1_PIN 18
#define TX1_PIN 17
#define ALL_TRAINS_FLAG 99
#define CRUISE_SPEED 65

// Verified Bitmasks from your hardware:
#define MASK_L2 0x0040 
#define MASK_R2 0x0080 

#define SVC_UUID "00001623-1212-efde-1623-785feabcd123"
#define CHR_UUID "00001624-1212-efde-1623-785feabcd123"

enum ControlSource { SOURCE_NONE, SOURCE_ANALOG_L, SOURCE_ANALOG_R, SOURCE_DPAD, SOURCE_BUTTON };

struct TrainHub {
    NimBLEClient* pClient = nullptr;
    NimBLERemoteCharacteristic* pChar = nullptr;
    bool connected = false;
    int8_t lastSentSpeed = 127;
    int speedStepIdx = 3; 
    int colorStep = 0;
    ControlSource activeSource = SOURCE_NONE;
    int currentOwner = -1; 
};

TrainHub trains[MAX_TRAINS];
int8_t speedSteps[] = {-100, -60, -30, 0, 30, 60, 100};
uint8_t colorSequence[] = {10, 14, 11, 15, 16, 17, 18, 4, 20, 21, 0, 1};
const int totalColors = sizeof(colorSequence) / sizeof(colorSequence[0]);

int controllerMap[2] = {0, 1}; 

void setTrainSpeed(int tIdx, int8_t target, int controllerNum, const char* stick) {
    if (tIdx < 0 || tIdx >= MAX_TRAINS || !trains[tIdx].connected) return;
    if (target == trains[tIdx].lastSentSpeed) return;

    uint8_t m[] = {0x08, 0x00, 0x81, 0x32, 0x11, 0x51, 0x00, (uint8_t)target};
    trains[tIdx].pChar->writeValue(m, 8, false);
    trains[tIdx].lastSentSpeed = target;
    Serial.printf("[TRAIN %d] Speed -> %d%% (CTRL %d %s)\n", tIdx + 1, target, controllerNum, stick);
}

void sendEvent(int tIdx, uint8_t eventId, uint8_t value) {
    if (tIdx < 0 || tIdx >= MAX_TRAINS || !trains[tIdx].connected) return;
    uint8_t cmd[] = {0x0A, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, eventId, 0x01, value};
    trains[tIdx].pChar->writeValue(cmd, sizeof(cmd), false);
}

class MyClientCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient* pClient) {
        for (int i = 0; i < MAX_TRAINS; i++) {
            if (trains[i].pClient == pClient) {
                trains[i].connected = false;
                trains[i].pClient = nullptr;
                trains[i].pChar = nullptr;
                Serial.printf("[BLE] Train %d Disconnected\n", i + 1);
            }
        }
    }
};

void scanAndConnect() {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    NimBLEScanResults results = pScan->start(1, false);
    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice device = results.getDevice(i);
        if (device.haveManufacturerData() && (uint8_t)device.getManufacturerData()[0] == 0x97) {
            int slot = -1;
            for (int s = 0; s < MAX_TRAINS; s++) { if (!trains[s].connected) { slot = s; break; } }
            if (slot != -1) {
                NimBLEClient* pClient = NimBLEDevice::createClient();
                pClient->setClientCallbacks(new MyClientCallbacks());
                if (pClient->connect(&device)) {
                    auto pSvc = pClient->getService(SVC_UUID);
                    if (pSvc) {
                        trains[slot].pChar = pSvc->getCharacteristic(CHR_UUID);
                        if (trains[slot].pChar) {
                            trains[slot].pClient = pClient;
                            trains[slot].connected = true;
                            uint8_t act[] = {0x05, 0x00, 0x01, 0x02, 0x02};
                            trains[slot].pChar->writeValue(act, 5, true);
                            Serial.printf("[OK] Train %d Online\n", slot + 1);
                        }
                    }
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, RX1_PIN, TX1_PIN);
    NimBLEDevice::init("S3_Dispatcher_Pro");

    Serial.println("\n===========================================");
    Serial.println("  S3 DUPLO TRAIN DUAL DISPATCHER BOOTED: READY AND WAITING...  ");
    Serial.println("  L2 -> T1 (65%) | R2 -> T2 (65%)          ");
    Serial.println("===========================================\n");
}

void loop() {
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 5000) {
        bool needMore = false;
        for(int i=0; i<MAX_TRAINS; i++) if(!trains[i].connected) needMore = true;
        if(needMore) scanAndConnect();
        lastScan = millis();
    }

    if (Serial1.available()) {
        String line = Serial1.readStringUntil('\n');
        if (line.startsWith("C:")) {
            int cIdx; uint32_t btn; int misc, dpad, lx, ly, rx, ry;
           
            if (sscanf(line.substring(2).c_str(), "%d,%u,%d,%d,%d,%d,%d,%d",
                &cIdx, &btn, &misc, &dpad, &lx, &ly, &rx, &ry) == 8) {

                if (cIdx >= 0 && cIdx < 2) {
                    int cNum = cIdx + 1; 
                    static uint32_t lastBtn[2] = {0, 0};
                    static int lastMisc[2] = {0, 0};
                    static int lastDpad[2] = {0, 0};

                    // --- GLOBAL PRESETS (Triggered on both controllers) ---
                    // L2 (0x0040) -> Train 1 Cruise
                    if ((btn & MASK_L2) && !(lastBtn[cIdx] & MASK_L2)) {
                        trains[0].activeSource = SOURCE_BUTTON;
                        trains[0].currentOwner = -1; 
                        trains[0].speedStepIdx = 5; 
                        setTrainSpeed(0, CRUISE_SPEED, cNum, "CRUISE-L2");
                    }
                    // R2 (0x0080) -> Train 2 Cruise
                    if ((btn & MASK_R2) && !(lastBtn[cIdx] & MASK_R2)) {
                        trains[1].activeSource = SOURCE_BUTTON;
                        trains[1].currentOwner = -1;
                        trains[1].speedStepIdx = 5;
                        setTrainSpeed(1, CRUISE_SPEED, cNum, "CRUISE-R2");
                    }

                    // Mode switching
                    if ((btn & 0x0030) == 0x0030) { controllerMap[cIdx] = ALL_TRAINS_FLAG; }
                    else if ((misc & 0x02) && !(lastMisc[cIdx] & 0x02)) { controllerMap[cIdx] = 0; }
                    else if ((misc & 0x04) && !(lastMisc[cIdx] & 0x04)) { controllerMap[cIdx] = 1; }

                    for (int tIdx = 0; tIdx < MAX_TRAINS; tIdx++) {
                        if (!trains[tIdx].connected) continue;

                        int currentStickVal = 0;
                        ControlSource stickType = SOURCE_NONE;
                        const char* stickName = "";

                        if (controllerMap[cIdx] == ALL_TRAINS_FLAG || controllerMap[cIdx] == tIdx) {
                            currentStickVal = ly; stickType = SOURCE_ANALOG_L; stickName = "L-STICK";
                        } else {
                            currentStickVal = ry; stickType = SOURCE_ANALOG_R; stickName = "R-STICK";
                        }

                        bool isMoving = (abs(currentStickVal) > 5);

                        if (trains[tIdx].currentOwner == -1 && isMoving) {
                            trains[tIdx].currentOwner = cIdx;
                        }

                        if (trains[tIdx].currentOwner == cIdx || trains[tIdx].currentOwner == -1) {
                            if (isMoving) {
                                trains[tIdx].activeSource = stickType;
                                setTrainSpeed(tIdx, (int8_t)(-currentStickVal), cNum, stickName);
                            } 
                            else if (trains[tIdx].activeSource == stickType) {
                                setTrainSpeed(tIdx, 0, cNum, stickName);
                                trains[tIdx].activeSource = SOURCE_NONE;
                                trains[tIdx].currentOwner = -1; 
                                trains[tIdx].speedStepIdx = 3; 
                            }
                        }

                        // Buttons
                        if (controllerMap[cIdx] == ALL_TRAINS_FLAG || controllerMap[cIdx] == tIdx) {
                            if ((dpad & 0x01) && !(lastDpad[cIdx] & 0x01)) { 
                                trains[tIdx].activeSource = SOURCE_DPAD;
                                if (trains[tIdx].speedStepIdx < 6) trains[tIdx].speedStepIdx++;
                                setTrainSpeed(tIdx, speedSteps[trains[tIdx].speedStepIdx], cNum, "DPAD");
                            }
                            if ((dpad & 0x02) && !(lastDpad[cIdx] & 0x02)) { 
                                trains[tIdx].activeSource = SOURCE_DPAD;
                                if (trains[tIdx].speedStepIdx > 0) trains[tIdx].speedStepIdx--;
                                setTrainSpeed(tIdx, speedSteps[trains[tIdx].speedStepIdx], cNum, "DPAD");
                            }
                            if ((btn & 0x01) && !(lastBtn[cIdx] & 0x01)) { // A
                                sendEvent(tIdx, 0x04, colorSequence[trains[tIdx].colorStep]);
                                trains[tIdx].colorStep = (trains[tIdx].colorStep + 1) % totalColors;
                            }
                            if ((btn & 0x02) && !(lastBtn[cIdx] & 0x02)) { // B
                                trains[tIdx].activeSource = SOURCE_NONE;
                                trains[tIdx].currentOwner = -1; 
                                trains[tIdx].speedStepIdx = 3;
                                setTrainSpeed(tIdx, 0, cNum, "STOP-BTN");
                            }
                            if ((btn & 0x04) && !(lastBtn[cIdx] & 0x04)) sendEvent(tIdx, 0x07, 0x01); // X
                            if ((btn & 0x08) && !(lastBtn[cIdx] & 0x08)) sendEvent(tIdx, 0x06, 0x01); // Y
                        }
                    }
                    lastBtn[cIdx] = btn; lastMisc[cIdx] = misc; lastDpad[cIdx] = dpad;
                }
            }
        }
    }
}