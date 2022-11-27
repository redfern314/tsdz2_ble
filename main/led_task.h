#ifndef LED_TASK
#define LED_TASK

#include <stdint.h>

typedef enum {
    RED,
    GREEN,
    BLUE,
    NO_COLOR,
} color_t;

void runLedTask(void* pvParameters);

uint8_t getColor(color_t color);
void setColor(color_t color, uint8_t val);
uint32_t getDelay();
void setDelay(uint32_t ms);

#endif // LED_TASK
