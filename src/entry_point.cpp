// This is not my code
// I had to change from the arduino framework to improve ram for ssh
// This is what helps it still work out the same
#include <Arduino.h>
extern void setup();
extern void loop();
extern "C" void app_main() {
    initArduino();
    setup();
    xTaskCreateUniversal(
        [](void *param) {
            for (;;) {
                loop();
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        },
        "loopTask",
        4096,
        NULL,
        1,
        NULL,
        CONFIG_ARDUINO_RUNNING_CORE);
}
