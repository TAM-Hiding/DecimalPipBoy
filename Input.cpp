#include "Input.h"
#include "Button.h"
#include "Globals.h"
#include "Touch.h"

//======================================================
// Physical buttons
//======================================================

static Button leftButton(BUTTON_LEFT);
static Button selectButton(BUTTON_SELECT);
static Button rightButton(BUTTON_RIGHT);

static bool touchLeftEvent = false;
static bool touchSelectEvent = false;
static bool touchRightEvent = false;

static bool rawTouchEvent = false;
static uint16_t rawTouchX = 0;
static uint16_t rawTouchY = 0;

//======================================================
// Public functions
//======================================================

void inputInit()
{
    leftButton.begin();
    selectButton.begin();
    rightButton.begin();

    pinMode(CONTROL_KNOB, INPUT);
    analogReadResolution(10);

#if HAS_TOUCH
    touchInit();
#endif
}

void inputUpdate()
{
    leftButton.update();
    selectButton.update();
    rightButton.update();

    // Touch events last for one update only.
    touchLeftEvent = false;
    touchSelectEvent = false;
    touchRightEvent = false;
    rawTouchEvent = false;

#if HAS_TOUCH
    touchUpdate();

    if (touchPressed())
    {
        const uint16_t x = touchX();
        const uint16_t y = touchY();
        
        rawTouchEvent = true;
        rawTouchX = x;
        rawTouchY = y;

        Serial.print("TOUCH DOWN  ");
        Serial.print(x);
        Serial.print(", ");
        Serial.println(y);

        // Divide the 480-pixel-wide screen into three controls.
        // Large footer navigation hitboxes.
        constexpr uint16_t FOOTER_Y = 260;
        constexpr uint16_t PREV_MAX_X = 125;
        constexpr uint16_t NEXT_MIN_X = 355;

        if (y >= FOOTER_Y && x <= PREV_MAX_X)
        {
            touchLeftEvent = true;
            Serial.println("TOUCH ACTION: PREVIOUS");
        }
        else if (y >= FOOTER_Y && x >= NEXT_MIN_X)
        {
            touchRightEvent = true;
            Serial.println("TOUCH ACTION: NEXT");
        }
        else if (y < FOOTER_Y)
        {
            // Keep the broad center-tap Select behavior for now.
            touchSelectEvent = true;
            Serial.println("TOUCH ACTION: SELECT");
        }
    }

    if (touchReleased())
    {
        Serial.println("TOUCH UP");
    }
#endif
}

bool inputLeftPressed()
{
    return leftButton.pressed() || touchLeftEvent;
}

bool inputSelectPressed()
{
    return selectButton.pressed() || touchSelectEvent;
}

bool inputRightPressed()
{
    return rightButton.pressed() || touchRightEvent;
}

uint16_t inputKnobValue()
{
    return analogRead(CONTROL_KNOB);
}

bool inputTouchPressed()
{
    return rawTouchEvent;
}

uint16_t inputTouchX()
{
    return rawTouchX;
}

uint16_t inputTouchY()
{
    return rawTouchY;
}
