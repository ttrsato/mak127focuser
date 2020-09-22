/*
    Description: This exmpale can display the encoder gear reading of the PLUS Module and the state of the keys.
*/
#include <Arduino.h>
#include <Ticker.h>
#include <M5Stack.h>

const int IN1 = 16;
const int IN2 = 17;
const int IN3 = 21;
const int IN4 = 22;
const int ECA = 35;
const int ECB = 36;

const int DISP_W = 320;
const int DISP_H = 240;
const int CIR_R = 195 / 2;
const int V_STEP = 32;
const int H_OFST = 115;
const int H_OFS = 70;
const int V_OFS = 83;
const int V_BASE = 80;

uint8_t full_step_table [] = {
    0b0101, // 0
    0b1001, // 1
    0b1010, // 2
    0b0110, // 3
};

uint8_t half_step_table [] = {
    0b0101, // 0
    0b0001, // 1
    0b1001, // 2
    0b1000, // 3
    0b1010, // 4
    0b0010, // 5
    0b0110, // 6
    0b0100, // 7
};

enum {DIR_NONE, DIR_FW, DIR_BW};
enum {MODE_NULL, MODE_FULL, MODE_HALF};
enum {LOOP_FULL = 4, LOOP_HALF = 8};

Ticker seq_tick;
const float toggle_period_sec = 0.3;
volatile bool step_run_flg = false;
volatile int step_run_cnt = 0;
int step_run_dir = DIR_FW;
volatile int tgl_c = 1;

uint8_t *step_table;

int mark1 = 0;
int mark2 = 0;
int cur_pos = 0;
int last_pos = 0;
int step_round = 48;
float step_angle = 360.0 / step_round;

int color_fg = TFT_RED;
int color_bg = TFT_BLACK;

uint8_t step_phase = 0;
uint8_t l_step_phase = 0;
uint8_t step_loop = LOOP_FULL;
uint8_t step_dir = DIR_FW;
uint8_t last_step_dir = DIR_FW;
uint8_t cur_mode = MODE_FULL;
uint8_t ec, last_ec;

void setNextPhase(uint8_t dir)
{
    step_dir = dir;
    l_step_phase = step_phase;
    last_pos = cur_pos;
    if (dir == DIR_FW) {
        step_phase--;
        cur_pos--;

    } else {
        step_phase++;
        cur_pos++;
    }
    if (cur_pos < 0) {
        cur_pos = step_round - 1;
    }
    step_phase = step_phase % step_loop;
    cur_pos = cur_pos % step_round;
}

void setStepIN()
{
    int val;
    if (cur_mode == MODE_FULL) {
        val = full_step_table[step_phase];
    } else {
        val = half_step_table[step_phase];
    }
    int in1 = ((val & (0x10 >> 1)) == 0) ? LOW : HIGH;
    int in2 = ((val & (0x10 >> 2)) == 0) ? LOW : HIGH;
    int in3 = ((val & (0x10 >> 3)) == 0) ? LOW : HIGH;
    int in4 = ((val & (0x10 >> 4)) == 0) ? LOW : HIGH;
    Serial.printf("%d: %d %d %d %d\n", step_phase, in1, in2, in3, in4);
    digitalWrite(IN1, in1);
    digitalWrite(IN2, in2);
    digitalWrite(IN3, in3);
    digitalWrite(IN4, in4);
}

void drawVal(int ofs, float val)
{
    M5.Lcd.fillRect(DISP_W - H_OFS - 5, V_OFS + ofs, 70, 24, color_bg);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(color_fg);
    M5.Lcd.setCursor(DISP_W - H_OFS - 5, V_OFS + ofs);
    M5.Lcd.printf("%.02f", val);
}

void setStepMode(uint8_t mode)
{
    if (cur_mode != mode) {
        if (mode == MODE_FULL) {
            step_table = full_step_table;
            step_round = 48;
            step_loop = LOOP_FULL;
            step_phase /= 2;
            step_angle = 360.0 / step_round;
            cur_pos /= 2;
            Serial.println("FULL step");
        } else {
            step_table = half_step_table;
            step_round = 48 * 2;
            step_loop = LOOP_HALF;
            step_phase *= 2;
            step_angle = 360.0 / step_round;
            cur_pos *= 2;
            Serial.println("HALF step");
        }
        cur_mode = mode;
    }
    drawVal(0, step_angle);
    setStepIN();
}

uint8_t readEC()
{
    uint8_t a, b;
    a  = digitalRead(ECA);
    b  = digitalRead(ECB);
    delay(1);
    a ^= digitalRead(ECA);
    b ^= digitalRead(ECB);
    delay(1);
    a ^= digitalRead(ECA);
    b ^= digitalRead(ECB);
    last_ec = ec;
    ec = a | b << 1;
    if (last_ec == 0b10 && ec == 0b11) {
        Serial.println("EC->FW");
        return DIR_FW;
    } else if (last_ec == 0b11 && ec == 0b10) {
        Serial.println("EC->BW");
        return DIR_BW;
    }
    return DIR_NONE;
}

void drawPos(int pos, int color, int fill)
{
    int center_x = CIR_R;
    int center_y = CIR_R - 2;
    float angle = pos * step_angle;
    float sx0 = cos((angle - 90) * 0.0174532925);
    float sy0 = sin((angle - 90) * 0.0174532925);
    float sx1 = cos((angle + 175 - 90) * 0.0174532925);
    float sy1 = sin((angle + 175 - 90) * 0.0174532925);
    float sx2 = cos((angle + 185 - 90) * 0.0174532925);
    float sy2 = sin((angle + 185 - 90) * 0.0174532925);
    int x0 = sx0 * (CIR_R - 25) + center_x;
    int y0 = sy0 * (CIR_R - 25) + center_y;
    int x1 = sx1 * (CIR_R - 25) + center_x;
    int y1 = sy1 * (CIR_R - 25) + center_y;
    int x2 = sx2 * (CIR_R - 25) + center_x;
    int y2 = sy2 * (CIR_R - 25) + center_y;
    if (fill) {
        M5.Lcd.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    } else {
        M5.Lcd.drawTriangle(x0, y0, x1, y1, x2, y2, color);
    }
    drawVal(V_STEP, step_angle * cur_pos);
    // Serial.printf("LINE %d, %d\n", pos, color);
}

void drawCircle(int clr)
{
    int center_x = CIR_R;
    int center_y = CIR_R - 2;
    if (clr == 0) {
        M5.Lcd.fillCircle(center_x, center_y, CIR_R - 1, color_fg);
    }
    M5.Lcd.fillCircle(center_x, center_y, CIR_R - 9, color_bg);

    for (int i = 0; i < 360; i += 30) {
        float sx  = cos((i - 90) * 0.0174532925);
        float sy  = sin((i - 90) * 0.0174532925);
        int x0 = sx * (CIR_R - 6)  + center_x;
        int y0 = sy * (CIR_R - 6)  + center_y;
        int x1 = sx * (CIR_R - 20) + center_x;
        int y1 = sy * (CIR_R - 20) + center_y;
        M5.Lcd.drawLine(x0, y0, x1, y1, color_fg);
    }
    for (int i = 0; i < step_round; i++) {
        float deg = i * step_angle;
        float sx  = cos((deg - 90) * 0.0174532925);
        float sy  = sin((deg - 90) * 0.0174532925);
        int x0 = sx * (CIR_R - 18) + center_x;
        int y0 = sy * (CIR_R - 18) + center_y;
        // Draw minute markers
        M5.Lcd.drawPixel(x0, y0, color_fg);
    }
    float deg = mark1 * step_angle;
    float sx  = cos((deg - 90) * 0.0174532925);
    float sy  = sin((deg - 90) * 0.0174532925);
    int x0 = sx * (CIR_R - 13) + center_x;
    int y0 = sy * (CIR_R - 13) + center_y;
    M5.Lcd.fillCircle(x0, y0, 3, color_fg);
    drawPos(cur_pos, color_fg, 1);
}

void countDownSeq()
{
    if (step_run_cnt <= 0) {
        return;
    }
    step_run_cnt--;
    step_run_flg = true;
    if (step_run_cnt == 0) {
        seq_tick.detach();
    }
}

int blinkCenter(int flg)
{
    int next_flg;
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(80 * 1 + 42, DISP_H - 30);
    if (flg == 0) {
        M5.Lcd.fillRect(80 * 1 + 38, DISP_H - 33, 77, 20, color_bg);
        M5.Lcd.setTextColor(color_fg);
        next_flg = 1;
    } else {
        M5.Lcd.fillRect(80 * 1 + 38, DISP_H - 33, 77, 20, color_fg);
        M5.Lcd.setTextColor(color_bg);
        next_flg = 0;
    }
    M5.Lcd.print("Center");
    return next_flg;
}

void setup() {
    M5.begin();
    // M5.Power.begin();
    dacWrite(25, 0); // STOP SP
    digitalWrite(IN1, 0);
    digitalWrite(IN2, 0);
    digitalWrite(IN3, 0);
    digitalWrite(IN4, 0);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    pinMode(ECA, INPUT);
    pinMode(ECB, INPUT);
    readEC();
    M5.Lcd.setTextColor(color_fg);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(DISP_W - H_OFST, V_BASE);
    M5.Lcd.print("Step");
    M5.Lcd.setCursor(DISP_W - H_OFST, V_BASE + 12);
    M5.Lcd.print("Angle");
    setStepMode(MODE_FULL);
    drawCircle(0);
    M5.Lcd.fillRect(DISP_W - H_OFST - 8, 2, 124, 35, color_fg);
    M5.Lcd.setTextColor(color_bg);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(DISP_W - H_OFST - 5, 5);
    M5.Lcd.print("Focus");
    M5.Lcd.setCursor(DISP_W - H_OFST - 5, 20);
    M5.Lcd.print("controller");
    M5.Lcd.setCursor(DISP_W - H_OFST - 4, 5);
    M5.Lcd.print("Focus");
    M5.Lcd.setCursor(DISP_W - H_OFST - 4, 20);
    M5.Lcd.print("controller");
    M5.Lcd.setTextColor(color_fg);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(DISP_W - H_OFST, V_BASE + V_STEP);
    M5.Lcd.print("Cur");
    M5.Lcd.setCursor(DISP_W - H_OFST, V_BASE + V_STEP + 12);
    M5.Lcd.print("Pos");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(DISP_W - H_OFST, V_BASE + V_STEP * 2 + 6);
    M5.Lcd.print("Mark");
    drawVal(V_STEP * 2, step_angle * mark1);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(color_fg);
    M5.Lcd.setCursor(80 * 0 + 42, DISP_H - 30);
    M5.Lcd.print("Mark");
    M5.Lcd.setCursor(80 * 2 + 67, DISP_H - 30);
    M5.Lcd.print("Step");
    // M5.Lcd.setCursor(80 * 1 + 42, DISP_H - 30);
    // M5.Lcd.print("Center");
    blinkCenter(0);
}

void loop() {
    if (step_run_cnt > 0 || step_run_flg) {
        if (step_run_flg) {
            setNextPhase(step_run_dir);
            drawPos(last_pos, color_bg, 1);
            drawPos(cur_pos, color_fg, 1);
            setStepIN();
            tgl_c = blinkCenter(tgl_c);
            step_run_flg = false;
        }
        if (step_run_cnt == 0) {
            blinkCenter(0);
        }
        delay(10);
    } else {
        M5.update();
        if (M5.BtnA.wasPressed()) {
            Serial.println("Mark");
            mark1 = cur_pos;
            drawVal(V_STEP * 2, step_angle * mark1);
            drawCircle(1);
        } else if (M5.BtnB.wasPressed()) {
            int pos_diff;
            if (cur_pos >= mark1) {
                pos_diff = cur_pos - mark1;
                step_run_dir = DIR_FW;
            } else {
                pos_diff = mark1 - cur_pos;
                step_run_dir = DIR_BW;
            }
            if (pos_diff > step_round / 2) {
                pos_diff = step_round - pos_diff;
                step_run_dir = (step_run_dir == DIR_FW) ? DIR_BW : DIR_FW;
            }
            step_run_cnt = pos_diff / 2;
            Serial.printf("Center: diff %d %d", step_run_cnt, step_run_dir);
            tgl_c = 1;
            seq_tick.attach(toggle_period_sec, countDownSeq);
        } else if (M5.BtnC.wasPressed()) {
            Serial.print('C');
            int next_mode = (cur_mode == MODE_FULL) ? MODE_HALF : MODE_FULL;
            setStepMode(next_mode);
            drawCircle(1);
        }
        uint8_t ec_dir = readEC();
        if (ec_dir != DIR_NONE) {
            setNextPhase(ec_dir);
            drawPos(last_pos, color_bg, 1);
            drawPos(cur_pos, color_fg, 1);
            setStepIN();
        }

    }
}
