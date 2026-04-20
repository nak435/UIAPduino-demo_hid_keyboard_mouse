[UIAPduino Pro Micro CH32V003 V1.4](https://www.uiap.jp/uiapduino/pro-micro/ch32v003/v1dot4) で HIDキーボード＆マウス を作る（[`TinyUSB Mouse and Keyboard`](https://github.com/cyborg5/TinyUSB_Mouse_and_Keyboard) ライクなAPIを実装）

# はじめに

https://qiita.com/nak435/items/1f4949041fe9f97d9728

↑ 前回は、`UIAPduino Pro Micro CH32V003 V1.4` を `Arduino` で使うための環境構築と、`Arduino IDE` で `Lチカ`（`Blink`）を実行しました。

今回は、`UIAPduino Pro Micro CH32V003 V1.4` で `HIDキーボード＆マウス` を作ります。

結論を先に書きますが、`HIDキーボード＆マウス`は`Arduino`環境では実現できませんでした。ビルドは通って`UIAPduino`に書き込めるのですが、上手く動作しません。

--- 
https://github.com/YuukiUmeta-UIAP/rv003usb

↑ このリポジトリの中に、`demo_composite_hid`という、HIDキーボード、マウスのサンプルコードがあります。

このコードは、1秒ごとに"b"キーをタイプし、マウスを小さな四角い枠内を移動させます。

---
`UIAPduino`ではありませんが、`CH32V003`を使った[@74th](https://74th.hateblo.jp)さんの独自ボードで、`demo_composite_hid`を参考に 1キーの HIDキーボード を作成されたブログ記事がありました。

https://74th.hateblo.jp/entry/ch32v003-1key

---

本稿では、`demo_composite_hid`を参考にして、[`TinyUSB Mouse and Keyboard`](https://github.com/cyborg5/TinyUSB_Mouse_and_Keyboard)に似せたAPIを作成し、キー入力とマウス操作を自由に行えるようにしたいと思います。


# APIイメージ

`TinyUSB Mouse and Keyboard`に似せた、以下のAPIとします。

```c++:TinyUSB Mouse and Keyboard
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
    size_t println(const char* s);  //with newline
    size_t press(uint8_t k);
    size_t release(uint8_t k);
    void releaseAll();
};

extern TinyMouse_ Mouse;
extern TinyKeyboard_ Keyboard;
```

`ASCIIコード → HIDキーコード変換`もAPI内で行うため、アプリは普通に文字列を指定してタイプできます。また、`Alt+Ctrl+DEL`のようなモディファイアキーを伴うタイプも可能です。
さらに、HIDキーコード変換は、JISキーボードとUSキーボードの両方に対応します（後述する#define値で指定）。


# プロジェクトのファイル構成

参考にした、前述の`demo_composite_hid`プロジェクトのファイル構成は次の通りです。
<sup>（`README.md`等はビルドに無関係のため、除外しています。）</sup>

```text
demo_composite_hid
├── Makefile
├── funconfig.h
├── usb_config.h
└── demo_composite_hid.c (メイン)
```

一方、今回作成した`TinyUSB Mouse and Keyboard API`を使用したプロジェクトのファイル構成は次の通りです。
`demo_composite_hid`プロジェクトと同じ階層に作成して `make`します。

<sup>コンパイル＆ビルド環境の構築に関しては、冒頭の前回記事を参照してください。</sup>

```text
demo_hid_keyboard_mouse
├── Makefile
├── funconfig.h
├── usb_config.h
├── demo_hid_keyboard_mouse.cpp (メイン)
├── TinyUSB_Mouse_Keyboard.cpp
├── TinyUSB_Mouse_Keyboard.h
├── US_Keyboard.h
└── JIS_Keyboard.h
```

`demo_composite_hid`と同名のファイルが存在しますが、内容に『違い』があります。`TinyUSB Mouse and Keyboard API`を使用したプロジェクトを作成する場合は、こちらのファイルを一式コピーして（必要により改修して）使用してください。

以降、`Makefile`, `funconfig.h`, `usb_config.h`のファイルについて『違い』を説明します。

## `Makefile`

メインファイルを `.cpp` としたことによる変更です。
- ターゲットファイルのファイル拡張子（cpp）の指定
- コンパイラを`gcc`から`g++`に変更

```makefile:Makefile
all : flash

TARGET:=demo_hid_keyboard_mouse
TARGET_EXT:=cpp  # メインファイルが .cpp
CH32FUN:=../ch32v003fun/ch32fun
TARGET_MCU:=CH32V003

ADDITIONAL_C_FILES+=../rv003usb/rv003usb.S ../rv003usb/rv003usb.c TinyUSB_Mouse_Keyboard.cpp
EXTRA_CFLAGS:=-I../lib -I../rv003usb

include $(CH32FUN)/ch32fun.mk

# g++ に差し替え
CC:=$(PREFIX)-g++

flash : cv_flash
clean : cv_clean
```

## `funconfig.h`

使用するキーボード種別の指定を追加しました。
`FUNCONF_USE_US_KEYBOARD`と`FUNCONF_USE_JIS_KEYBOARD`いずれかを`1`にします。両方`1`とすると、USキーボードを採用します。
両方`0`（または`未定義`）とすると、コンパイルエラー<b><font color=red>"No keyboard type was specified."</font></b>となります。

```c:funconfig.h
#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

#define FUNCONF_USE_DEBUGPRINTF 1
#define CH32V003                1
#define FUNCONF_SYSTICK_USE_HCLK 1

#define FUNCONF_USE_US_KEYBOARD 1
#define FUNCONF_USE_JIS_KEYBOARD 0

#endif
```

## `usb_config.h`

マウスのHIDディスクリプター（`mouse_hid_desc`）に、『水平スクロール』の項目を追加しました。
`Mouse.move(x, y, wheel, pan)`APIの4番目(`pan`)の引数が該当します。

```c:usb_config.h
#ifndef _USB_CONFIG_H
#define _USB_CONFIG_H

//Defines the number of endpoints for this device. (Always add one for EP0). For two EPs, this should be 3.
#define ENDPOINTS 3

#define USB_PORT D     // [A,C,D] GPIO Port to use with D+, D- and DPU
#define USB_PIN_DP 3   // [0-4] GPIO Number for USB D+ Pin
#define USB_PIN_DM 4   // [0-4] GPIO Number for USB D- Pin
#define USB_PIN_DPU 5  // [0-7] GPIO for feeding the 1.5k Pull-Up on USB D- Pin; Comment out if not used / tied to 3V3!

#define RV003USB_OPTIMIZE_FLASH 1
#define RV003USB_EVENT_DEBUGGING 0
#define RV003USB_HANDLE_IN_REQUEST 1
#define RV003USB_OTHER_CONTROL 0
#define RV003USB_HANDLE_USER_DATA 1
#define RV003USB_HID_FEATURES 0

#ifndef __ASSEMBLER__

#include <tinyusb_hid.h>

#ifdef INSTANCE_DESCRIPTORS
//Taken from http://www.usbmadesimple.co.uk/ums_ms_desc_dev.htm
static const uint8_t device_descriptor[] = {
	18, //Length
	1,  //Type (Device)
	0x10, 0x01, //Spec
	0x0, //Device Class
	0x0, //Device Subclass
	0x0, //Device Protocol  (000 = use config descriptor)
	0x08, //Max packet size for EP0 (This has to be 8 because of the USB Low-Speed Standard)
	0x09, 0x12, //ID Vendor
	0x03, 0xc0, //ID Product
	0x02, 0x00, //ID Rev
	1, //Manufacturer string
	2, //Product string
	3, //Serial string
	1, //Max number of configurations
};

static const uint8_t mouse_hid_desc[] = {  //From http://eleccelerator.com/tutorial-about-usb-hid-report-descriptors/
	HID_USAGE_PAGE( HID_USAGE_PAGE_DESKTOP ),         // USAGE_PAGE (Generic Desktop)
	HID_USAGE( HID_USAGE_DESKTOP_MOUSE ),  // USAGE (Mouse)
	HID_COLLECTION ( HID_COLLECTION_APPLICATION ),    // COLLECTION (Application)
		HID_USAGE( HID_USAGE_DESKTOP_POINTER ),       //   USAGE (Pointer)
		HID_COLLECTION ( HID_COLLECTION_PHYSICAL ),   //   COLLECTION (Physical)
			HID_USAGE_PAGE( HID_USAGE_PAGE_BUTTON ),  //     USAGE_PAGE (Button)
			HID_USAGE_MIN( 1 ),                       //     USAGE_MINIMUM (Button 1)
			HID_USAGE_MAX( 3 ),                       //     USAGE_MAXIMUM (Button 3)
			HID_LOGICAL_MIN( 0 ),                     //     LOGICAL_MINIMUM (0)
			HID_LOGICAL_MAX( 1 ),                     //     LOGICAL_MAXIMUM (1)
			HID_REPORT_COUNT( 3 ),                    //     REPORT_COUNT (3)
			HID_REPORT_SIZE( 1 ),                     //     REPORT_SIZE (1)
			HID_INPUT( 0x02 ),                        //     INPUT (Data,Var,Abs)
			HID_REPORT_COUNT( 1 ),                    //     REPORT_COUNT (1)
			HID_REPORT_SIZE( 5 ),                     //     REPORT_SIZE (5)
			HID_INPUT( 0x03 ),                        //     INPUT (Cnst,Var,Abs)
			HID_USAGE_PAGE( HID_USAGE_PAGE_DESKTOP ), //     USAGE_PAGE (Desktop)
			HID_USAGE( HID_USAGE_DESKTOP_X ),         //     USAGE (X)
			HID_USAGE( HID_USAGE_DESKTOP_Y ),         //     USAGE (Y)
			HID_USAGE( HID_USAGE_DESKTOP_WHEEL ),     //     USAGE (Wheel)
			HID_LOGICAL_MIN( 0x81 ),                  //     LOGICAL_MINIMUM -127
			HID_LOGICAL_MAX( 0x7f ),                  //     LOGICAL_MAXIMUM (127)
			HID_REPORT_SIZE( 8 ),                     //     REPORT_SIZE (8)
			HID_REPORT_COUNT( 3 ),                    //     REPORT_COUNT (3)
			HID_INPUT( 0x06 ),                        //     INPUT (Data,Var,Rel)
		HID_COLLECTION_END,                           //   END_COLLECTION
        // --- Horizontal Pan (Consumer Page) ---
        HID_USAGE_PAGE( HID_USAGE_PAGE_CONSUMER ),    //   USAGE_PAGE (CONSUMER)
        HID_USAGE_N( 0x0238, 2 ),                     //   AC Pan
        HID_LOGICAL_MIN( 0x81 ),                      //   LOGICAL_MINIMUM -127
		HID_LOGICAL_MAX( 0x7F ),                      //   LOGICAL_MAXIMUM (127)
        HID_REPORT_SIZE( 8 ),                         //   REPORT_SIZE (8)
		HID_REPORT_COUNT( 1 ),                        //   REPORT_COUNT (1)
        HID_INPUT( 0x06 ),                            //   INPUT (Data,Var,Rel)
	HID_COLLECTION_END,                               // END_COLLECTIONs
};

//From http://codeandlife.com/2012/06/18/usb-hid-keyboard-with-v-usb/
static const uint8_t keyboard_hid_desc[] = {   /* USB report descriptor */
	HID_USAGE_PAGE( HID_USAGE_PAGE_DESKTOP ),         // USAGE_PAGE (Generic Desktop)
	HID_USAGE( HID_USAGE_DESKTOP_KEYBOARD ),          // USAGE (Keyboard)
	HID_COLLECTION ( HID_COLLECTION_APPLICATION ),    // COLLECTION (Application)
		HID_REPORT_SIZE( 1 ),                         //     REPORT_SIZE (8)
		HID_REPORT_COUNT( 8 ),                        //     REPORT_COUNT (3)
		HID_USAGE_PAGE( HID_USAGE_PAGE_KEYBOARD ),    //     USAGE_PAGE (Keyboard)(Key Codes)
    	HID_USAGE_MIN( 0xe0 ),                        //     USAGE_MINIMUM (Keyboard LeftControl)(224)
    	HID_USAGE_MAX( 0xe7 ),                        //     USAGE_MAXIMUM (Keyboard Right GUI)(231)
		HID_LOGICAL_MIN( 0 ),                         //     LOGICAL_MINIMUM (0)
		HID_LOGICAL_MAX( 1 ),                         //     LOGICAL_MAXIMUM (1)
		HID_INPUT( 0x02 ),                            //     INPUT (Data,Var,Abs) ; Modifier byte
		HID_REPORT_COUNT( 1 ),                        //     REPORT_COUNT (1)
		HID_REPORT_SIZE( 8 ),                         //     REPORT_SIZE (8)
		HID_INPUT( 0x03 ),                            //     INPUT (Cnst,Var,Abs) ; Reserved byte
		HID_REPORT_COUNT( 5 ),                        //     REPORT_COUNT (5)
		HID_REPORT_SIZE( 1 ),                         //     REPORT_SIZE (1)
		HID_USAGE_PAGE( HID_USAGE_PAGE_LED ),         //     USAGE_PAGE (LEDs)
    	HID_USAGE_MIN( 0x01 ),                        //     USAGE_MINIMUM (Num Lock)
	    HID_USAGE_MAX( 0x05 ),                        //     USAGE_MAXIMUM (Kana)
		HID_OUTPUT( 0x02 ),                           //     OUTPUT (Data,Var,Abs) ; LED report
		HID_REPORT_COUNT( 1 ),                        //     REPORT_COUNT (1)
		HID_REPORT_SIZE( 3 ),                         //     REPORT_SIZE (3)
		HID_OUTPUT( 0x03 ),                           //     OUTPUT (Cnst,Var,Abs) ; LED report padding
		HID_REPORT_COUNT( 7 ),                        //     REPORT_COUNT (7)
		HID_REPORT_SIZE( 8 ),                         //     REPORT_SIZE (8)
		HID_OUTPUT( 0x03 ),						      //     OUTPUT (Cnst,Var,Abs) ; Padding to fill buffer to 8 bytes
		HID_REPORT_COUNT( 6 ),                        //     REPORT_COUNT (6)
		HID_REPORT_SIZE( 8 ),                         //     REPORT_SIZE (8)
		HID_LOGICAL_MIN( 0 ),                         //     LOGICAL_MINIMUM (0)
		HID_LOGICAL_MAX( 167 ),                       //     LOGICAL_MAXIMUM (167)  (Normally would be 101, but we want volume buttons)
    	HID_USAGE_PAGE( HID_USAGE_PAGE_KEYBOARD ),    //     USAGE_PAGE (Keyboard)(Key Codes)
    	HID_USAGE_MIN( 0x00 ),                        //     USAGE_MINIMUM (0)
	    HID_USAGE_MAX( 167 ),                         //     USAGE_MAXIMUM (Keyboard Application)(101) (Now 167)
	HID_INPUT( 0 ),                                   //   INPUT (Data,Ary,Abs)
    HID_COLLECTION_END,                               // END_COLLECTION
};

//Ever wonder how you have more than 6 keys down at the same time on a USB keyboard?  It's easy. Enumerate two keyboards!
// No, really, that's what some hardware manufacturers do.

static const uint8_t config_descriptor[] = {  //Mostly stolen from a USB mouse I found.
	// configuration descriptor, USB spec 9.6.3, page 264-266, Table 9-10
	9, 					// bLength;
	2,					// bDescriptorType;
	0x3b, 0x00,			// wTotalLength
	0x02,					// bNumInterfaces (Normally 1)
	0x01,					// bConfigurationValue
	0x00,					// iConfiguration
	0x80,					// bmAttributes (was 0xa0)
	0x64,					// bMaxPower (200mA)

	// This descriptor shows how to embed two HIDs, to build a composite HID device.


	//Mouse
	9,					// bLength
	4,					// bDescriptorType
	0,			// bInterfaceNumber (unused, would normally be used for HID)
	0,					// bAlternateSetting
	1,					// bNumEndpoints
	0x03,					// bInterfaceClass (0x03 = HID)
	0x01,					// bInterfaceSubClass
	0x02,					// bInterfaceProtocol (Mouse)
	0,					// iInterface

	9,					// bLength
	0x21,					// bDescriptorType (HID)
	0x10,0x01,		//bcd 1.1
	0x00, //country code
	0x01, //Num descriptors
	0x22, //DescriptorType[0] (HID)
	sizeof(mouse_hid_desc), 0x00,

	7, //endpoint descriptor (For endpoint 1)
	0x05, //Endpoint Descriptor (Must be 5)
	0x81, //Endpoint Address
	0x03, //Attributes
	0x05,	0x00, //Size
	10, //Interval (Number of milliseconds between polls)


	//Keyboard  (It is unusual that this would be here)
	9,					// bLength
	4,					// bDescriptorType
	1,			// bInterfaceNumber  = 1 instead of 0 -- well make it second.
	0,					// bAlternateSetting
	1,					// bNumEndpoints
	0x03,					// bInterfaceClass (0x03 = HID)
	0x01,					// bInterfaceSubClass
	0x01,					// bInterfaceProtocol (??)
	0,					// iInterface

	9,					// bLength
	0x21,					// bDescriptorType (HID)
	0x10,0x01,		//bcd 1.1
	0x00, //country code
	0x01, //Num descriptors
	0x22, //DescriptorType[0] (HID)
	sizeof(keyboard_hid_desc), 0x00,

	7, //endpoint descriptor (For endpoint 1)
	0x05, //Endpoint Descriptor (Must be 5)
	0x82, //Endpoint Address
	0x03, //Attributes
	0x08,	0x00, //Size (8 bytes)
	10, //Interval Number of milliseconds between polls.
};

#define STR_MANUFACTURER u"CNLohr"
#define STR_PRODUCT      u"RV003USB"
#define STR_SERIAL       u"000"

struct usb_string_descriptor_struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wString[];
};
const static struct usb_string_descriptor_struct string0 __attribute__((section(".rodata"))) = {
	4,
	3,
	{0x0409}
};
const static struct usb_string_descriptor_struct string1 __attribute__((section(".rodata")))  = {
	sizeof(STR_MANUFACTURER),
	3,
	STR_MANUFACTURER
};
const static struct usb_string_descriptor_struct string2 __attribute__((section(".rodata")))  = {
	sizeof(STR_PRODUCT),
	3,
	STR_PRODUCT
};
const static struct usb_string_descriptor_struct string3 __attribute__((section(".rodata")))  = {
	sizeof(STR_SERIAL),
	3,
	STR_SERIAL
};

// This table defines which descriptor data is sent for each specific
// request from the host (in wValue and wIndex).
const static struct descriptor_list_struct {
	uint32_t	lIndexValue;
	const uint8_t	*addr;
	uint8_t		length;
} descriptor_list[] = {
	{0x00000100, device_descriptor, sizeof(device_descriptor)},
	{0x00000200, config_descriptor, sizeof(config_descriptor)},
	{0x00002200, mouse_hid_desc, sizeof(mouse_hid_desc)},
	{0x00012200, keyboard_hid_desc, sizeof(keyboard_hid_desc)},
	{0x00000300, (const uint8_t *)&string0, 4},
	{0x04090301, (const uint8_t *)&string1, sizeof(STR_MANUFACTURER)},
	{0x04090302, (const uint8_t *)&string2, sizeof(STR_PRODUCT)},
	{0x04090303, (const uint8_t *)&string3, sizeof(STR_SERIAL)}
};
#define DESCRIPTOR_LIST_ENTRIES ((sizeof(descriptor_list))/(sizeof(struct descriptor_list_struct)) )

#endif // INSTANCE_DESCRIPTORS

#endif

#endif
```


# サンプルコード

`demo_hid_keyboard_mouse`プロジェクトのメインファイル`demo_hid_keyboard_mouse.cpp`を以下に示します。
動作としては、次のことを行います。

## 動作仕様

<img width="280" alt="UIAPduino-v1.4_circuit-1.png" src="https://qiita-image-store.s3.ap-northeast-1.amazonaws.com/0/283293/be9fa1c0-da2b-47ce-b641-d85ed89d8c11.png" />


1. 追加の回路：4つのキースイッチ（タクトスイッチ）SW1〜SW4 を、`UIAPduino`の D9(PC7)、D8(PC6)、D7(PC5)、D5(PC3)に接続し、他方をGNDに接続
2. SW1〜SW4が押されたら：UIAPduinoのオンボードLEDを点灯
2. SW1が押されたら："Hello UIAPduino CH32V003"改行 をタイプ
2. SW2が押されたら：`Win+Shift+S`（or `Cmd+Shift+5`）をタイプ
2. SW3が押されたら：マウスカーソルをX軸 +3移動
2. SW4が押されたら：マウスの水平スクロール +3移動
2. SW1〜SW4が離されたら：UIAPduinoのオンボードLEDを消灯
3. スイッチのチャタリング対応として、30msデバウンスを実装

```cpp:demo_hid_keyboard_mouse.cpp
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
    GPIOv_from_PORT_PIN(GPIO_port_C, 5)  // D7
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
        if ((now - btn_last_time[i]) > (DEBOUNCE_MS * (DELAY_MS_TIMEBASE/1))) {
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
                        case 3: Mouse.move(0, 0, 0, 3); break; // 水平スクロール +3
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
```

# ビルド

```terminal
$ cd rv003usb/demo_hid_keyboard_mouse
demo_hid_keyboard_mouse % make
riscv64-unknown-elf-gcc -E -P -x c -DTARGET_MCU=CH32V003 -DMCU_PACKAGE=1 -DTARGET_MCU_LD=0 -DTARGET_MCU_MEMORY_SPLIT= ../ch32v003fun/ch32fun/ch32fun.ld > ../ch32v003fun/ch32fun/generated__.ld
riscv64-unknown-elf-gcc -o demo_hid_keyboard_mouse.elf ../ch32v003fun/ch32fun/ch32fun.c demo_hid_keyboard_mouse.cpp   ../rv003usb/rv003usb.S ../rv003usb/rv003usb.c TinyUSB_Mouse_Keyboard.cpp  -g -Os -flto -ffunction-sections -fdata-sections -fmessage-length=0 -msmall-data-limit=8 -march=rv32ec -mabi=ilp32e -DCH32V003 -static-libgcc -I/usr/include/newlib -I../ch32v003fun/ch32fun/../extralibs -I../ch32v003fun/ch32fun -nostdlib -I. -Wall -I../lib -I../rv003usb -Wl,--print-memory-usage -Wl,-Map=demo_hid_keyboard_mouse.map -L../ch32v003fun/ch32fun/../misc -lgcc -lgcc -T ../ch32v003fun/ch32fun/generated__.ld -Wl,--gc-sections
TinyUSB_Mouse_Keyboard.cpp:9:49: note: '#pragma message: US keyboard was specified.'
    9 |     #pragma message("US keyboard was specified.")
      |                                                 ^
Memory region         Used Size  Region Size  %age Used
           FLASH:        3656 B        16 KB     22.31%
             RAM:         308 B         2 KB     15.04%
riscv64-unknown-elf-objdump -S demo_hid_keyboard_mouse.elf > demo_hid_keyboard_mouse.lst
riscv64-unknown-elf-objcopy  -O binary demo_hid_keyboard_mouse.elf demo_hid_keyboard_mouse.bin
riscv64-unknown-elf-objcopy -O ihex demo_hid_keyboard_mouse.elf demo_hid_keyboard_mouse.hex
make -C ../ch32v003fun/ch32fun/../minichlink all
make[1]: Nothing to be done for `all'.
../ch32v003fun/ch32fun/../minichlink/minichlink -w demo_hid_keyboard_mouse.bin flash -b
VID:0x1209, PID:0xb003
Error: Could not initialize any supported programmers
make: *** [cv_flash] Error 224

# -c VIDPID を指定して再実行
demo_hid_keyboard_mouse % ../ch32v003fun/ch32fun/../minichlink/minichlink -c 0x1209b803 -w demo_hid_keyboard_mouse.bin flash -b
VID:0x1209, PID:0xb803
Halting Boot Countdown
Detected CH32V003
Flash Storage: 16 kB
Part UUID: f3-85-ab-cd-5b-5a-bc-08
Read protection: disabled
Interface Setup
Writing image
...................................................................................................................................................................................................................................................................................................
Image written.
Booting

demo_hid_keyboard_mouse % 
```

ちゃんと動作しました。


`TinyUSB Mouse and Keyboard`APIを含む、`demo_hid_keyboard_mouse`プロジェクト一式を[Github](https://github.com/nak435/UIAPduino-demo_hid_keyboard_mouse/)に公開しました。

