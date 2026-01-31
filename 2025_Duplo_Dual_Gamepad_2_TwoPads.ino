// This works well now with two controllers.
// It sends a comma-separated packet format C:0,btns,miscbtns,dpad,lx,ly,rx,ry
// over the serial port
// C:0,0,0,0,0,-84,84,-92
// C:1,0,0,0,0,0,0,0

#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_CONTROLLERS];

// Helper to handle deadzone and scaling (50-511 -> 0-100)
int scaleAxis(int value) {
    int absVal = abs(value);
    if (absVal < 50) return 0; // Deadzone
    
    // Scale remaining range (50 to 511) to (0 to 100)
    int scaled = map(absVal, 50, 511, 0, 100);
    
    // Maintain the original direction (positive or negative)
    return (value > 0) ? scaled : -scaled;
}

void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_CONTROLLERS; i++) {
        if (myControllers[i] == nullptr) {
            myControllers[i] = ctl;
            Serial.printf("Controller %d connected\n", i);
            
            // Fixed member name: setPlayerLEDs
            if (i == 0) {
                ctl->setPlayerLEDs(0x01); // LED 1
            } else if (i == 1) {
                ctl->setPlayerLEDs(0x03); // LED 1 & 2
            }
            break;
        }
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_CONTROLLERS; i++) {
        if (myControllers[i] == ctl) {
            myControllers[i] = nullptr;
            Serial.printf("Controller %d disconnected\n", i);
            break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.printf("ESP32 Controller Bridge has booted.\n");
    BP32.setup(&onConnectedController, &onDisconnectedController);
}

void loop() {
    BP32.update();

    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    // 10Hz non-blocking timer
    if (now - lastUpdate >= 100) {
        lastUpdate = now;

        for (int i = 0; i < BP32_MAX_CONTROLLERS; i++) {
            ControllerPtr ctl = myControllers[i];
            if (ctl && ctl->isConnected()) {
                
                // Scale axes using the helper function
                int lx = scaleAxis(ctl->axisX());
                int ly = scaleAxis(ctl->axisY());
                int rx = scaleAxis(ctl->axisRX());
                int ry = scaleAxis(ctl->axisRY());

                // Send Packet: C:[Idx],[Btn],[Misc],[DPad],[LX],[LY],[RX],[RY]

                Serial.printf("C:%d,%u,%d,%d,%d,%d,%d,%d\n", 
                    ctl->index(), 
                    ctl->buttons(), 
                    ctl->miscButtons(), // Moved to 3rd position
                    ctl->dpad(), 
                    lx, 
                    ly, 
                    rx, 
                    ry);
            }
        }
    }
}