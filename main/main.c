#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_check.h"

#define PIN_NUM_MOSI  9 //D10=GPIO9
#define PIN_NUM_CLK   7 //D8=GPIO7
#define PIN_NUM_CS    4 //D3=GPIO4
#define PIN_NUM_DC    5 //D4=GPIO5
#define PIN_NUM_RST   6 //  D5=GPIO6
#define PIN_NUM_BL    3 //  D2=GPIO3

#define LCD_W   128
#define LCD_H   128
#define X_OFF     1
#define Y_OFF     2

#define C_BLACK     0x0000
#define C_WHITE     0xFFFF
#define C_NAVY      0x000F
#define C_DKBLUE    0x0011
#define C_CYAN      0x07FF
#define C_LTCYAN    0x8FFF
#define C_TEAL      0x03EF
#define C_WATER     0x34BF
#define C_WAVECREST 0xAEFF
#define C_ORANGE    0xFD20
#define C_GREEN     0x07E0
#define C_LTGREEN   0x87F0
#define C_RED       0xF800
#define C_GRAY      0x8410
#define C_DKGRAY    0x4208
#define C_LGRAY     0xC618
#define C_GOLD      0xFEA0

typedef struct {
    float goal_L;    // total bottle capacity
    float drunk_L;   // amount already consumed (removed from bottle)
} WaterState;

static uint16_t fb[LCD_W * LCD_H];

static inline void fb_set(int x, int y, uint16_t color)
{
    if (x < 0 || x >= LCD_W || y < 0 || y >= LCD_H) return;
    fb[y * LCD_W + x] = color;
}

static spi_device_handle_t spi;

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_NUM_DC, 0);
    spi_transaction_t t = { .length = 8, .tx_buffer = &cmd };
    spi_device_polling_transmit(spi, &t);
}

static void lcd_data(const uint8_t *data, int len)
{
    if (len == 0) return;
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = { .length = (size_t)len * 8, .tx_buffer = data };
    spi_device_polling_transmit(spi, &t);
}

static inline void lcd_byte(uint8_t b) { lcd_data(&b, 1); }

static void step1_gpio(void)
{
    printf("[1] GPIO init\n");
    gpio_set_direction(PIN_NUM_DC,  GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BL,  GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_BL, 0);
    printf("[1] DONE\n");
}

static void step2_spi(void)
{
    printf("[2] SPI init\n");
    spi_bus_config_t bus = {
        .mosi_io_num     = PIN_NUM_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = PIN_NUM_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_W * LCD_H * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 27 * 1000 * 1000,
        .mode           = 0,
        .spics_io_num   = PIN_NUM_CS,
        .queue_size     = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &spi));
    printf("[2] DONE\n");
}

static void step3_reset(void)
{
    printf("[3] Hardware reset\n");
    gpio_set_level(PIN_NUM_RST, 1); vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_NUM_RST, 0); vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_NUM_RST, 1); vTaskDelay(pdMS_TO_TICKS(150));
    printf("[3] DONE\n");
}

static void step4_lcd_init(void)
{
    printf("[4] ST7735S init commands\n");
    lcd_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(255));
    lcd_cmd(0xB1); lcd_byte(0x05); lcd_byte(0x3C); lcd_byte(0x3C);
    lcd_cmd(0xB2); lcd_byte(0x05); lcd_byte(0x3C); lcd_byte(0x3C);
    lcd_cmd(0xB3); lcd_byte(0x05); lcd_byte(0x3C); lcd_byte(0x3C);
                   lcd_byte(0x05); lcd_byte(0x3C); lcd_byte(0x3C);
    lcd_cmd(0xB4); lcd_byte(0x03);
    lcd_cmd(0xC0); lcd_byte(0xAB); lcd_byte(0x0B); lcd_byte(0x04);
    lcd_cmd(0xC1); lcd_byte(0xC5);
    lcd_cmd(0xC2); lcd_byte(0x0D); lcd_byte(0x00);
    lcd_cmd(0xC3); lcd_byte(0x8D); lcd_byte(0x6A);
    lcd_cmd(0xC4); lcd_byte(0x8D); lcd_byte(0xEE);
    lcd_cmd(0xC5); lcd_byte(0x0F);
    lcd_cmd(0x36); lcd_byte(0xC8);
    lcd_cmd(0x3A); lcd_byte(0x05);
    lcd_cmd(0xE0);
    const uint8_t gp[] = {0x04,0x22,0x07,0x0A,0x2E,0x30,0x25,0x2A,
                           0x28,0x26,0x2E,0x3A,0x00,0x01,0x03,0x13};
    lcd_data(gp, 16);
    lcd_cmd(0xE1);
    const uint8_t gn[] = {0x04,0x16,0x06,0x0D,0x2D,0x26,0x23,0x27,
                           0x27,0x25,0x2D,0x3B,0x00,0x01,0x04,0x13};
    lcd_data(gn, 16);
    lcd_cmd(0x21);  // inversion ON (fixes colors on this panel)
    lcd_cmd(0x13);
    lcd_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(10));
    printf("[4] DONE\n");
}

static void step5_bl_on(void)
{
    printf("[5] Backlight ON\n");
    gpio_set_level(PIN_NUM_BL, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    printf("[5] DONE\n");
}

static void lcd_flush(void)
{
    lcd_cmd(0x2A);
    uint8_t col[] = { 0x00, X_OFF, 0x00, (uint8_t)(X_OFF + LCD_W - 1) };
    lcd_data(col, 4);
    lcd_cmd(0x2B);
    uint8_t row[] = { 0x00, Y_OFF, 0x00, (uint8_t)(Y_OFF + LCD_H - 1) };
    lcd_data(row, 4);
    lcd_cmd(0x2C);
    static uint8_t line_buf[LCD_W * 2 * 4];
    gpio_set_level(PIN_NUM_DC, 1);
    for (int y = 0; y < LCD_H; y += 4) {
        int rows = (y + 4 <= LCD_H) ? 4 : (LCD_H - y);
        for (int r = 0; r < rows; r++)
            for (int x = 0; x < LCD_W; x++) {
                uint16_t c = fb[(y+r)*LCD_W+x];
                line_buf[(r*LCD_W+x)*2]   = c >> 8;
                line_buf[(r*LCD_W+x)*2+1] = c & 0xFF;
            }
        spi_transaction_t t = { .length=(size_t)LCD_W*rows*16, .tx_buffer=line_buf };
        spi_device_polling_transmit(spi, &t);
    }
}

// ================================================================
//  DRAWING PRIMITIVES
// ================================================================
static void draw_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            fb_set(x+dx, y+dy, color);
}

static void draw_hline(int x, int y, int w, uint16_t color)
{ for (int i = 0; i < w; i++) fb_set(x+i, y, color); }

static void draw_vline(int x, int y, int h, uint16_t color)
{ for (int i = 0; i < h; i++) fb_set(x, y+i, color); }

static void draw_round_rect(int x, int y, int w, int h, int r, uint16_t color)
{
    draw_hline(x+r, y,       w-2*r, color);
    draw_hline(x+r, y+h-1,   w-2*r, color);
    draw_vline(x,     y+r,   h-2*r, color);
    draw_vline(x+w-1, y+r,   h-2*r, color);
    for (int i = 0; i <= r; i++) {
        int j = (int)(r - sqrt((float)(r*r-i*i)) + 0.5f);
        fb_set(x+j,     y+r-i,       color);
        fb_set(x+w-1-j, y+r-i,       color);
        fb_set(x+j,     y+h-1-r+i,   color);
        fb_set(x+w-1-j, y+h-1-r+i,   color);
    }
}

static void fill_round_rect(int x, int y, int w, int h, int r, uint16_t color)
{
    draw_rect(x+r, y,   w-2*r, h,     color);
    draw_rect(x,   y+r, r,     h-2*r, color);
    draw_rect(x+w-r, y+r, r,   h-2*r, color);
    for (int i = 0; i <= r; i++) {
        int j = (int)(r - sqrt((float)(r*r-i*i)) + 0.5f);
        draw_hline(x+j, y+r-i,       w-2*j, color);
        draw_hline(x+j, y+h-1-r+i,   w-2*j, color);
    }
}

static void fill_circle(int cx, int cy, int r, uint16_t color)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x+y*y <= r*r) fb_set(cx+x, cy+y, color);
}

// ================================================================
//  FONT
// ================================================================
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
    {0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x41,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x04,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},{0x00,0x00,0x7F,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20},{0x41,0x41,0x7F,0x00,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x08,0x54,0x54,0x54,0x3C},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
    {0x00,0x7F,0x10,0x28,0x44},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},{0x08,0x08,0x2A,0x1C,0x08},
};

static void draw_char(int x, int y, char c, uint16_t fg, int scale)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t *g = font5x7[c-32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++)
            if (bits & (1<<row)) {
                if (scale==1) fb_set(x+col, y+row, fg);
                else draw_rect(x+col*scale, y+row*scale, scale, scale, fg);
            }
    }
}

static void draw_str(int x, int y, const char *s, uint16_t fg, int scale)
{ while (*s) { draw_char(x, y, *s++, fg, scale); x += 6*scale; } }

static void draw_str_center(int y, const char *s, uint16_t fg, int scale)
{
    int w = strlen(s)*6*scale - scale;
    draw_str((LCD_W-w)/2, y, s, fg, scale);
}

static void draw_waterdrop(int cx, int cy, int size, uint16_t color)
{
    int r = size/2;
    fill_circle(cx, cy+r/2, r, color);
    for (int dy = 0; dy < r+2; dy++) {
        int half = (r*dy)/(r+2);
        draw_hline(cx-half, cy-dy, half*2+1, color);
    }
}

// ================================================================
//  WAVE LINE — clipped to a horizontal band [clip_y_top, clip_y_bot]
// ================================================================
static void draw_wave_line_clipped(int y_base, int phase, uint16_t color,
                                   int clip_y_top, int clip_y_bot)
{
    for (int x = 0; x < LCD_W; x++) {
        int a = (x + phase) % 20;
        int b = (x + phase + 10) % 20;
        int wa = a < 10 ? (a*(10-a))/6 : -((a-10)*(20-a))/6;
        int wb = b < 10 ? (b*(10-b))/8 : -((b-10)*(20-b))/8;
        int py = y_base + wa + wb;
        if (py >= clip_y_top && py <= clip_y_bot)
            fb_set(x, py, color);
    }
}

// ================================================================
//  BOTTLE — shows REMAINING water (full at start, empties as you drink)
//
//  remaining_pct = (goal - drunk) / goal
//  e.g. drunk=0   → remaining=1.0 → bottle full
//       drunk=goal → remaining=0.0 → bottle empty
// ================================================================
static void draw_bottle(int x, int y, int w, int h,
                        float remaining_pct, uint16_t water_color)
{
    if (remaining_pct < 0.0f) remaining_pct = 0.0f;
    if (remaining_pct > 1.0f) remaining_pct = 1.0f;

    int neck_h = h / 6;
    int body_h = h - neck_h;
    int neck_w = w / 2;
    int neck_x = x + (w - neck_w) / 2;

    // Body boundary (inside = 1px inset from outline)
    int body_top    = y + neck_h;          // top of body
    int body_bot    = y + neck_h + body_h; // bottom of body
    int inner_left  = x + 1;
    int inner_right = x + w - 2;
    int inner_top   = body_top + 1;
    int inner_bot   = body_bot - 1;
    int inner_h     = inner_bot - inner_top;

    // Water fills from BOTTOM up by remaining_pct
    int fill_h = (int)(inner_h * remaining_pct);
    int water_top = inner_bot - fill_h;   // water surface y-coordinate

    // Fill water region
    if (fill_h > 0) {
        fill_round_rect(inner_left, water_top,
                        inner_right - inner_left + 1,
                        fill_h, 2, water_color);

        // Wave shimmer at the water surface (clipped to bottle interior)
        draw_wave_line_clipped(water_top, 4,  C_WAVECREST,
                               inner_top, inner_bot);
        draw_wave_line_clipped(water_top+1, 11, C_CYAN,
                               inner_top, inner_bot);
    }

    // Draw bottle outline ON TOP so water doesn't overdraw the border
    draw_round_rect(x, body_top, w, body_h, 3, C_LTCYAN);
    draw_rect(neck_x, y, neck_w, neck_h+1, C_BLACK);
    draw_round_rect(neck_x, y, neck_w, neck_h+2, 2, C_LTCYAN);

    // Tick marks at 25% 50% 75% of body height
    for (int t = 1; t <= 3; t++) {
        int ty = inner_bot - (inner_h * t) / 4;
        draw_hline(x + w - 5, ty, 4, C_LGRAY);
    }
}

// ================================================================
//  RENDER FULL SCREEN
// ================================================================
static void render_screen(const WaterState *ws)
{
    // Background
    for (int y = 0; y < LCD_H; y++)
        draw_hline(0, y, LCD_W, y < LCD_H/2 ? C_NAVY : C_DKBLUE);

    // Top bar
    draw_rect(0, 0, LCD_W, 13, 0x0019);
    draw_hline(0, 13, LCD_W, C_TEAL);
    draw_str(3, 3, "08 APR 2026", C_CYAN, 1);
    draw_str(LCD_W - 24, 3, "TUE", C_GOLD, 1);

    // Title
    draw_str_center(17, "HYDRATION", C_LTCYAN, 1);
    draw_str_center(26, "TRACKER",   C_TEAL,   1);
    draw_hline(20, 35, LCD_W-40, C_DKGRAY);

    // ---- KEY CHANGE: bottle shows REMAINING water ----
    float remaining_pct = (ws->goal_L - ws->drunk_L) / ws->goal_L;
    draw_bottle(8, 38, 28, 60, remaining_pct, C_WATER);

    // Stats
    char buf[16];
    float drank_pct = ws->drunk_L / ws->goal_L;

    // Color: red if just started, orange halfway, green when done
    uint16_t dc = (drank_pct >= 1.0f) ? C_LTGREEN :
                  (drank_pct >= 0.5f) ? C_ORANGE  : C_RED;

    draw_str(44, 40, "GOAL", C_GRAY, 1);
    snprintf(buf, sizeof(buf), "%.1fL", ws->goal_L);
    draw_str(44, 49, buf, C_WHITE, 1);

    draw_hline(44, 60, 76, C_DKGRAY);
    draw_str(44, 63, "DRANK", C_GRAY, 1);
    snprintf(buf, sizeof(buf), "%.1fL", ws->drunk_L);
    draw_str(44, 72, buf, dc, 1);

    draw_hline(44, 83, 76, C_DKGRAY);
    float left = ws->goal_L - ws->drunk_L;
    if (left < 0) left = 0;
    draw_str(44, 86, "LEFT", C_GRAY, 1);
    snprintf(buf, sizeof(buf), "%.1fL", left);
    draw_str(44, 95, buf, C_LGRAY, 1);

    // Progress bar (shows how much you've DRUNK — fills up as you drink)
    int bar_y=103, bar_x=8, bar_w=LCD_W-16, bar_h=8;
    fill_round_rect(bar_x, bar_y, bar_w, bar_h, 3, C_DKGRAY);
    int fill_w = (int)(bar_w * drank_pct);
    if (fill_w > 0) {
        for (int bx = 0; bx < fill_w; bx++)
            draw_vline(bar_x+bx, bar_y+1, bar_h-2,
                       bx < fill_w/2 ? C_TEAL : C_CYAN);
    }
    snprintf(buf, sizeof(buf), "%d%%", (int)(drank_pct*100));
    draw_str_center(bar_y+bar_h+2, buf,
                    drank_pct>=1.0f ? C_LTGREEN : C_CYAN, 1);

    // Status
    const char *msg = drank_pct>=1.0f  ? "GOAL MET! :)" :
                      drank_pct>=0.75f ? "ALMOST THERE" :
                      drank_pct>=0.5f  ? "KEEP GOING!"  :
                      drank_pct>=0.25f ? "DRINK MORE"   : "START NOW!";
    draw_str_center(119, msg, dc, 1);

    // Deco drops
    draw_waterdrop(116, 18, 8, C_WATER);
    draw_waterdrop(108, 22, 5, C_TEAL);
}

// ================================================================
//  DEMO: bottle starts FULL, empties as you drink
// ================================================================
static void demo_sequence(void)
{
    WaterState ws = { .goal_L = 3.0f, .drunk_L = 0.0f };

    // drunk_L increases from 0 → 3.0L  (bottle goes full → empty)
    float steps[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f,
                      1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f };
    int n = sizeof(steps)/sizeof(steps[0]);

    for (int i = 0; i < n; i++) {
        ws.drunk_L = steps[i];
        float remaining = ws.goal_L - ws.drunk_L;
        printf("[RENDER] Drunk=%.2fL  Remaining=%.2fL  (%d%%)\n",
               ws.drunk_L, remaining,
               (int)((ws.drunk_L / ws.goal_L) * 100));

        memset(fb, 0, sizeof(fb));
        render_screen(&ws);
        lcd_flush();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    printf("[DEMO DONE] Holding final frame.\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(5000));
}

// ================================================================
//  MAIN
// ================================================================
void app_main(void)
{
    printf("==============================\n");
    printf("  Water Tracker - XIAO ESP32-S3\n");
    printf("  Waveshare 0.85\" 128x128 LCD\n");
    printf("==============================\n");
    step1_gpio();
    step2_spi();
    step3_reset();
    step4_lcd_init();
    step5_bl_on();
    printf("[6] Starting demo...\n");
    demo_sequence();
}