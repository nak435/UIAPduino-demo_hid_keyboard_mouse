#ifndef TINYUSB_MOUSE_KEYBOARD_H
#define TINYUSB_MOUSE_KEYBOARD_H

#include <stdint.h>
#include <stddef.h>

// 修飾キー定義
#define KEY_LEFT_CTRL   0x80  // bit 0: 0x01
#define KEY_LEFT_SHIFT  0x81  // bit 1: 0x02
#define KEY_LEFT_ALT    0x82  // bit 2: 0x04
#define KEY_LEFT_GUI    0x83  // bit 3: 0x08
#define KEY_RIGHT_CTRL  0x84  // bit 4: 0x10
#define KEY_RIGHT_SHIFT 0x85  // bit 5: 0x20
#define KEY_RIGHT_ALT   0x86  // bit 6: 0x40
#define KEY_RIGHT_GUI   0x87  // bit 7: 0x80
#define MOUSE_LEFT      1
#define MOUSE_RIGHT     2
#define MOUSE_MIDDLE    4

// 1つのHIDレポート状態
typedef struct {
    uint8_t modifiers;
    uint8_t key_codes[6];
    uint8_t step; // 0:送信のみ, 1:送信後に自動でAll-0(離す)を送る
} kbd_report_t;

class TinyMouse_ {
public:
    void begin();
    void move(int8_t x, int8_t y, int8_t wheel = 0, int8_t pan = 0);
    void press(uint8_t b = MOUSE_LEFT);
    void release(uint8_t b = MOUSE_LEFT);
    void click(uint8_t b = MOUSE_LEFT);
};

class TinyKeyboard_ {
public:
    void begin();
    size_t write(uint8_t k);
    size_t print(const char* s);
    size_t println(const char* s);
    size_t press(uint8_t k);
    size_t release(uint8_t k);
    void releaseAll();
private:
    void _push_report(uint8_t step);
};

extern TinyMouse_ Mouse;
extern TinyKeyboard_ Keyboard;

// rv003usbから参照するための内部構造体宣言
extern struct mouse_internal_t {
    int8_t x, y, wheel, pan;
    uint8_t buttons;
    volatile bool changed;
} mouse_internal;

extern struct kbd_internal_t {
    kbd_report_t buffer[16]; // バッファサイズ
    volatile int head;
    volatile int tail;
    kbd_report_t current;
} kbd_internal;

extern "C" {
    void usb_handle_user_in_request( struct usb_endpoint * e, uint8_t * scratchpad, int endp, uint32_t sendtok, struct rv003usb_internal * ist );
    void usb_handle_user_data( struct usb_endpoint * e, int current_endpoint, uint8_t * data, int len, struct rv003usb_internal * ist );
}

#endif
