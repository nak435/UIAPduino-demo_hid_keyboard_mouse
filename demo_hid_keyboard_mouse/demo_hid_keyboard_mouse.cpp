#include <stdio.h>
#include <string.h>
extern "C" {
    #include "ch32fun.h"
    #include "rv003usb.h"
	#include "ch32v003_GPIO_branchless.h"
    #include "TinyUSB_Mouse_Keyboard.h"
}

#define LED_PIN GPIOv_from_PORT_PIN(GPIO_port_C, 0)     // PC0
#define NUM_BUTTONS 4
const uint8_t button_pins[NUM_BUTTONS] = {
    GPIOv_from_PORT_PIN(GPIO_port_C, 7), // D9
    GPIOv_from_PORT_PIN(GPIO_port_C, 6), // D8
    GPIOv_from_PORT_PIN(GPIO_port_C, 5),  // D7
    GPIOv_from_PORT_PIN(GPIO_port_C, 3)  // D5
};

// 各ボタンの状態管理用
uint8_t btn_stable_state[NUM_BUTTONS]; // 確定した状態 (high/low)
uint8_t btn_last_raw[NUM_BUTTONS];     // 直近の生読み取り値
uint32_t btn_last_time[NUM_BUTTONS];   // 最後に状態が変わった時間
const uint32_t DEBOUNCE_MS = 30;       // 判定時間 (30ms)


uint8_t before_btn_state = 0;

int main()
{
	SystemInit();

	GPIO_port_enable(GPIO_port_C);
	GPIO_pinMode(LED_PIN, GPIO_pinMode_O_pushPull, GPIO_Speed_50MHz);
	// Button初期化
	for(int i=0; i<NUM_BUTTONS; i++) {
		GPIO_pinMode(button_pins[i], GPIO_pinMode_I_pullUp, GPIO_Speed_In);
		btn_stable_state[i] = high;
		btn_last_raw[i] = high;
	}

	Delay_Ms(1); // Ensures USB re-enumeration after bootloader or reset; Spec demand >2.5µs ( TDDIS )
	usb_setup(); // USB HID setup

	while(1)
	{
#if RV003USB_EVENT_DEBUGGING
		uint32_t * ue = GetUEvent();
		if( ue )
		{
			printf( "%lu %lx %lx %lx\n", ue[0], ue[1], ue[2], ue[3] );
		}
#endif

        uint32_t now = SysTick->CNT;

        for(int i=0; i<NUM_BUTTONS; i++) {
            uint8_t raw = GPIO_digitalRead(button_pins[i]);

            // 生の値が変わった瞬間、タイマーをリセット
            if (raw != btn_last_raw[i]) {
                btn_last_time[i] = now;
            }

            // 指定時間(DEBOUNCE_MS)以上、同じ値が続いていたら「確定」
            if ((now - btn_last_time[i]) > (DEBOUNCE_MS * (DELAY_MS_TIME))) {
                if (raw != btn_stable_state[i]) {
                    btn_stable_state[i] = raw;

                    // --- 状態が変化した瞬間の処理 ---
                    if (btn_stable_state[i] == low) {
                        // 押された時の処理
                        GPIO_digitalWrite_hi(LED_PIN);

                        switch(i) {
                            case 0: Keyboard.println("Hello UIAPduino CH32V003"); break;
                            case 1: Keyboard.press(KEY_LEFT_GUI); Keyboard.press(KEY_LEFT_SHIFT); Keyboard.press('s'); Keyboard.releaseAll(); break;
                          //case 1: Keyboard.press(KEY_LEFT_GUI); Keyboard.press(KEY_LEFT_SHIFT); Keyboard.press('5'); Keyboard.releaseAll(); break;
                            case 2: Mouse.move(3, 0, 0, 0); break; // X+3
                            case 3: Mouse.move(0, 0, 0, 3); break; // 垂直スクロール
                        }
                    } else {
                        // 離された時の処理
                        GPIO_digitalWrite_lo(LED_PIN);
                    }
                }
            }
            btn_last_raw[i] = raw;
        }
    }
}
