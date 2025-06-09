

#include <AccelStepper.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

#define STEP_PIN 9
#define DIR_PIN 3
#define RELAY_PIN 4
#define BUTTON_PUSK 5
#define AUTO_PIN 6
#define BUT_RIGHT A0
#define BUT_LEFT A1
#define BUT_OK A2

// === Настройки ===
struct Settings {
    uint16_t length_mm;
    uint16_t steps_per_mm;
    uint8_t  speed;
    uint16_t accel;
    uint16_t auto_cycles;
};
Settings settings;

uint16_t cycle_count = 0;  // счётчик выполненных циклов

// === Параметры меню ===
const char* menu_items[] = {
    "Length (mm)",
    "Steps/mm",
    "Speed",
    "Accel",
    "Auto cycles",
    "Cycle counter"};
const uint8_t MENU_COUNT = sizeof(menu_items) / sizeof(menu_items[0]);
uint8_t       menu_index = 0;
unsigned long ok_hold_start = 0;

// === LCD ===
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// === Шаговик ===
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// === EEPROM ===
void loadSettings() {
    EEPROM.get(0, settings);
    if (settings.steps_per_mm == 0xFFFF || settings.steps_per_mm < 1 || settings.steps_per_mm > 100) {
        settings = {1000, 16, 5, 200, 10};
    }
}
void saveSettings() {
    EEPROM.put(0, settings);
}

// === Кнопки ===
bool button(uint8_t pin) {
    return digitalRead(pin) == LOW;
}

void showMenu() {
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print(menu_items[menu_index]);
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    switch (menu_index) {
        case 0:
            lcd.print(settings.length_mm);
            lcd.print(" mm   ");
            break;
        case 1:
            lcd.print(settings.steps_per_mm);
            lcd.print(" st/mm");
            break;
        case 2:
            lcd.print(settings.speed);
            lcd.print(" spd  ");
            break;
        case 3:
            lcd.print(settings.accel);
            lcd.print(" acc  ");
            break;
        case 4:
            lcd.print(settings.auto_cycles);
            lcd.print(" auto ");
            break;
        case 5:
            lcd.print("Count: ");
            lcd.print(cycle_count);
            break;
    }
}

void applyStepperSettings() {
    stepper.setMaxSpeed(settings.speed * 1500.);  // скорость условная
    stepper.setAcceleration(settings.accel * 7.);
}

void runCycle() {
    long steps = (long)settings.length_mm * settings.steps_per_mm;
    lcd.clear();
    lcd.print("Unwinding...");
    stepper.moveTo(steps);
    while (stepper.distanceToGo() != 0) {
        stepper.run();
    }
    delay(100);
    lcd.clear();
    lcd.print("Welding...");
    digitalWrite(RELAY_PIN, HIGH);
    delay(2000);
    digitalWrite(RELAY_PIN, LOW);
    stepper.setCurrentPosition(0);
    cycle_count++;
}

void setup() {
    pinMode(BUTTON_PUSK, INPUT_PULLUP);
    pinMode(AUTO_PIN, INPUT_PULLUP);
    pinMode(BUT_OK, INPUT_PULLUP);
    pinMode(BUT_LEFT, INPUT_PULLUP);
    pinMode(BUT_RIGHT, INPUT_PULLUP);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    lcd.init();
    lcd.backlight();
    loadSettings();
    applyStepperSettings();
    showMenu();
    Serial.begin(9600);
}

void loop() {
    static uint16_t auto_done = 0;
    // Serial.printf("Buttons: PUSK=%d, AUTO=%d, LEFT=%d, RIGHT=%d, OK=%d\n",
    //               button(BUTTON_PUSK), button(AUTO_PIN), button(BUT_LEFT), button(BUT_RIGHT), button(BUT_OK));

    if (button(BUTTON_PUSK) && auto_done > 0) {
        auto_done = 0;  // сброс счётчика автоциклов
    }

    if (button(BUTTON_PUSK) && (!button(AUTO_PIN))) {
        auto_done = 0;  // сброс счётчика автоциклов
        runCycle();
        showMenu();
    }

    if (button(AUTO_PIN) && button(BUTTON_PUSK)) {
        while (settings.auto_cycles > 0 && auto_done < settings.auto_cycles) {
            runCycle();
            auto_done++;
            showMenu();
            while (!button(AUTO_PIN)) {
                if (button(BUTTON_PUSK)) {
                    break;
                }
            }
            if (button(BUTTON_PUSK)) {
                break;
            }
        }
    }

    // Меню
    if (button(BUT_OK)) {
        if (ok_hold_start == 0)
            ok_hold_start = millis();
        else if (millis() - ok_hold_start > 3000 && menu_index == 5) {
            cycle_count = 0;
            lcd.clear();
            lcd.print("Counter reset");
            delay(1000);
            showMenu();
        }
    } else if (ok_hold_start > 0) {
        menu_index = (menu_index + 1) % MENU_COUNT;
        ok_hold_start = 0;
        showMenu();
    }

    if (button(BUT_LEFT)) {
        switch (menu_index) {
            case 0:
                if (settings.length_mm >= 10) settings.length_mm -= 10;
                break;
            case 1:
                if (settings.steps_per_mm > 1) settings.steps_per_mm--;
                break;
            case 2:
                if (settings.speed > 1) settings.speed--;
                break;
            case 3:
                if (settings.accel > 1) settings.accel -= 10;
                break;
            case 4:
                if (settings.auto_cycles > 1) settings.auto_cycles--;
                break;
        }
        applyStepperSettings();
        saveSettings();
        showMenu();
        delay(150);
    }

    if (button(BUT_RIGHT)) {
        switch (menu_index) {
            case 0:
                if (settings.length_mm <= 9990) settings.length_mm += 10;
                break;
            case 1:
                if (settings.steps_per_mm < 100) settings.steps_per_mm++;
                break;
            case 2:
                if (settings.speed < 10) settings.speed++;
                break;
            case 3:
                if (settings.accel < 9990) settings.accel += 10;
                break;
            case 4:
                if (settings.auto_cycles < 9999) settings.auto_cycles++;
                break;
        }
        applyStepperSettings();
        saveSettings();
        showMenu();
        delay(150);
    }
{ // debug print state inputs
    delay(150);
    lcd.setCursor(14, 0);
    lcd.print(button(BUTTON_PUSK));
    lcd.setCursor(15, 0);
    lcd.print(button(AUTO_PIN));
    
}
} 
