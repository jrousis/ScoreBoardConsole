#define LV_USE_PERF_MONITOR 0

//#include "lv_demo_widgets.h"
// #include <tinyxml2.h>
#include <WiFi.h>
#include <lvgl.h>
//#include <demos/lv_demos.h>
#include "ui.h"
#include "ui_events.h"
#include <Arduino_GFX_Library.h>
#define TFT_BL 2
#include "EEPROM.h"

// #include <SPIFFS.h>
// #include <ArduinoJson.h>


// const char* settingsFile = "/settings.json";

struct Settings {
  char ssid[32] = "DefaultSSID";
  char password[32] = "DefaultPASS";
  uint8_t brightness = 25;
  char mac1[32] = "00:00:00:00:00:01";
  char mac2[32] = "00:00:00:00:00:02";
  char countdown1[32] = "00:01:12";
  char countdown2[32] = "00:00:30";
};

Settings deviceSettings;



#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

unsigned long timeout;
// server congfig
const char *host = "192.168.1.51";
const uint16_t port = 1001;
WiFiClient client;

// Set a struct for the XML data Instruction
struct Instruction {
  String type;
  String password;
  String posNr;
  String categoryNr;
};

struct Posdata {
  String Type;
  String CategoryNr;
  String CategoryName;
  String Queue;
  String PeopleWaiting;
  String AVGtime;
  String LastServiceTime;
  String Note;
};
Posdata posdata;

// typedef struct {
//   char SSID[32] = "Rousis";
//   char SSPS[32] = "rousis074520198";
//   char IP[32] = "100:1:1:1";
// } config_t;

// config_t device_config;

void eeprom_read(uint16_t *dstAddr, uint16_t srcAddr, uint16_t len) {
  uint8_t *dst = (uint8_t *)dstAddr;
  for (uint16_t i = 0; i < len; i++) {
    dst[i] = (uint8_t)EEPROM.read(srcAddr + i);
  }
}

void eeprom_write(uint16_t dstAddr, void *srcAddr, uint16_t len) {
  uint8_t *src = (uint8_t *)srcAddr;
  for (uint16_t i = 0; i < len; i++) {
    EEPROM.write(dstAddr + i, src[i]);
    EEPROM.commit();
  }
}




Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
  GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
  41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
  14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
  9 /* G0 */, 46 /* G1 */, 3 /* G2 */, 8 /* G3 */, 16 /* G4 */, 1 /* G5 */,
  15 /* B0 */, 7 /* B1 */, 6 /* B2 */, 5 /* B3 */, 4 /* B4 */
);
// option 1:
// 7寸 50PIN 800*480
Arduino_RPi_DPI_RGBPanel *gfx = new Arduino_RPi_DPI_RGBPanel(
  bus,


  800 /* width */, 0 /* hsync_polarity */, 210 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
  480 /* height */, 0 /* vsync_polarity */, 22 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */,
  1 /* pclk_active_neg */, 16000000 /* prefer_speed */, true /* auto_flush */);

#endif /* !defined(DISPLAY_DEV_KIT) */
/*******************************************************************************
 * End of Arduino_GFX setting
 ******************************************************************************/

/*******************************************************************************
 * Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"

/* Change to your screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

bool connected_flag = false;
/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (touch_has_signal()) {
    if (touch_touched()) {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
      // Serial.print( "Data x " );
      // Serial.println( data->point.x );
      // Serial.print( "Data y " );
      // Serial.println( data->point.y );
    } else if (touch_released()) {
      data->state = LV_INDEV_STATE_REL;
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}
//================================================================================================

// Replace with your receiver MAC Address
// 80:64:6F:EE:6F:D8 (1η του Βασιλειάδη)
// 70:B8:F6:78:8E:B4 (2η του Βασιλειάδη)
uint8_t broadcastAddress[] = { 0x70, 0xB8, 0xF6, 0x78, 0x8E, 0xB4 };

char page_buf[256];
// Structure example to send data
// Must match the receiver structure
void setup() {

  Serial.begin(115200);
  Serial.println("LVGL Widgets Demo");


  // initSPIFFS();

  //   if (loadSettings(deviceSettings)) {
  //       Serial.println("Settings loaded successfully.");
  //   } else {
  //       Serial.println("Default settings applied.");
  //   }

  // Serial.print("SSID: ");
  // Serial.println(deviceSettings.ssid);
  // Serial.print("Brightness: ");
  // Serial.println(deviceSettings.brightness);



  // while (!Serial);
  delay(300);

  // Init Display
  gfx->begin();
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  ledcSetup(0, 300, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 255); /* Screen brightness can be modified by adjusting this parameter. (0-255) */

#endif
  gfx->fillScreen(RED);
  delay(500);
  gfx->fillScreen(GREEN);
  delay(500);
  gfx->fillScreen(BLUE);
  delay(500);
  gfx->fillScreen(BLACK);
  delay(500);
  lv_init();

  // Init touch device
  pinMode(TOUCH_GT911_RST, OUTPUT);
  digitalWrite(TOUCH_GT911_RST, LOW);
  delay(10);
  digitalWrite(TOUCH_GT911_RST, HIGH);
  delay(10);

  touch_init();
  //  touch.setTouch( calData );

  screenWidth = gfx->width();
  screenHeight = gfx->height();
#ifdef ESP32
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
  disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 4);
#endif
  if (!disp_draw_buf) {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  } else {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 4);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();
    lv_timer_handler(); /* let the GUI do its work */


    EEPROM.begin(512);
    if (EEPROM.read(0) == 255) {
      Serial.println("EEPROM is not config");
      EEPROM.put(1, deviceSettings);
      EEPROM.write(0, 10);
    } else {
      EEPROM.get(1, deviceSettings);
      Serial.println("EEPROM  config");
    }
    EEPROM.commit();

    // Serial.println();
    // Serial.println("EEPROM  config:");
    Serial.println(deviceSettings.ssid);
    Serial.println(deviceSettings.password);

    WiFi.setAutoReconnect(true);
    WiFi.begin(deviceSettings.ssid, deviceSettings.password);
    long tim = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tim < 5000) {
      delay(1000);
      Serial.print(".");
    }

    String temp = "";
    temp = deviceSettings.ssid;
    Serial.println(temp);
    lv_textarea_set_text(ui_TextAreaSSID, temp.c_str());
    temp = deviceSettings.password;
    Serial.println(temp);
    lv_textarea_set_text(ui_TextAreaPassword, temp.c_str());
    temp = deviceSettings.countdown1;
    Serial.println(temp);
    lv_textarea_set_text(ui_TextAreaCDown1, temp.c_str());
    temp = deviceSettings.countdown2;
    Serial.println(temp);
    lv_textarea_set_text(ui_TextAreaCDown2, temp.c_str());
    temp = deviceSettings.mac1;
    Serial.println(temp);
    lv_textarea_set_text(ui_TextAreaMAC1, temp.c_str());
    temp = deviceSettings.mac2;
    Serial.println(temp);
    lv_textarea_set_text(ui_TextAreaMAC2, temp.c_str());
  }

  Serial.println("Try connection with the server.");
  // Send connection to the server
  // timeout = millis();
  // while (!client.connect(host, port)) {
  //   if (millis() - timeout > 5000) {
  //     Serial.println("Connection timeout..");
  //     return;
  //   }
  // }
  Serial.println("Succesfully connection!");
  lv_obj_set_style_bg_color(ui_PanelConnection, lv_color_hex(0x39F00B), LV_PART_MAIN | LV_STATE_DEFAULT);
  connected_flag = true;

  String message = "/CONNECT|01";
  Serial.print("Sending message: ");
  Serial.println(message);

  // lv_textarea_set_text(ui_TextAreaPOS, "01");
  //  if (connected_flag)
  //  {
  //      lv_obj_set_style_bg_color(ui_Panel2, lv_color_hex(0x39F00B), LV_PART_MAIN | LV_STATE_DEFAULT);
  //  }
  //  else
  //  {
  //      lv_obj_set_style_bg_color(ui_Panel2, lv_color_hex(0xff1100), LV_PART_MAIN | LV_STATE_DEFAULT);
  //  }

  pinMode(0, INPUT_PULLUP);
}

long tim = 0;
void loop() {

  //   if (millis() - tim > 2000) {
  //     if (digitalRead(0) == LOW) {
  //       _ui_screen_change(&ui_Screen2, LV_SCR_LOAD_ANIM_OVER_LEFT, 10, 0, &ui_Screen2_screen_init);
  //     }

  //     if (WiFi.status() == WL_CONNECTED) {
  //       int RSSI = 130 + WiFi.RSSI();
  //       lv_arc_set_value(ui_ArcWiFi, RSSI);
  //       lv_label_set_text(ui_LabelWiFi, "Connected");
  //       lv_obj_set_style_text_color(ui_LabelWiFi, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  //     } else {
  //       lv_label_set_text(ui_LabelWiFi, "Disconnected");
  //       lv_obj_set_style_text_color(ui_LabelWiFi, lv_color_hex(0xFF0101), LV_PART_MAIN | LV_STATE_DEFAULT);
  //     }

  //     tim = millis();
  //   }



  if (CHK_Flag_Func()) {

    int val = CHK_Val_Func();
    //     if (val == 3) {
    //       Serial.println("SAVE");

    // // Copy SSID
    //     strncpy(deviceSettings.ssid, lv_textarea_get_text(ui_TextAreaSSID), sizeof(deviceSettings.ssid) - 1);
    //     deviceSettings.ssid[sizeof(deviceSettings.ssid) - 1] = '\0'; // Ensure null termination

    //     // Copy Password
    //     strncpy(deviceSettings.password, lv_textarea_get_text(ui_TextAreaPassword), sizeof(deviceSettings.password) - 1);
    //     deviceSettings.password[sizeof(deviceSettings.password) - 1] = '\0';

    //     // Copy Countdown1
    //     strncpy(deviceSettings.countdown1, lv_textarea_get_text(ui_TextAreaCDown1), sizeof(deviceSettings.countdown1) - 1);
    //     deviceSettings.countdown1[sizeof(deviceSettings.countdown1) - 1] = '\0';

    //     // Copy Countdown2
    //     strncpy(deviceSettings.countdown2, lv_textarea_get_text(ui_TextAreaCDown2), sizeof(deviceSettings.countdown2) - 1);
    //     deviceSettings.countdown2[sizeof(deviceSettings.countdown2) - 1] = '\0';

    //     // Copy MAC1
    //     strncpy(deviceSettings.mac1, lv_textarea_get_text(ui_TextAreaMAC1), sizeof(deviceSettings.mac1) - 1);
    //     deviceSettings.mac1[sizeof(deviceSettings.mac1) - 1] = '\0';

    //     // Copy MAC2
    //     strncpy(deviceSettings.mac2, lv_textarea_get_text(ui_TextAreaMAC2), sizeof(deviceSettings.mac2) - 1);
    //     deviceSettings.mac2[sizeof(deviceSettings.mac2) - 1] = '\0';

    //     // Debug output to verify the updated settings
    //     Serial.println("Updated Device Settings:");
    //     Serial.printf("SSID: %s\n", deviceSettings.ssid);
    //     Serial.printf("Password: %s\n", deviceSettings.password);
    //     Serial.printf("Countdown1: %s\n", deviceSettings.countdown1);
    //     Serial.printf("Countdown2: %s\n", deviceSettings.countdown2);
    //     Serial.printf("MAC1: %s\n", deviceSettings.mac1);
    //     Serial.printf("MAC2: %s\n", deviceSettings.mac2);



    //       EEPROM.begin(512);
    //       EEPROM.put(1, deviceSettings);
    //       EEPROM.commit();
    //       WiFi.disconnect();
    //       delay(500);  // Wait for a second before reconnecting
    //       WiFi.begin(deviceSettings.ssid, deviceSettings.password);

    //     }

    if (val == 1) {
      Serial.println("Team A | Player 1");
    } else if (val == 2) {
      Serial.println("Team A | Player 2");
    } else if (val == 3) {
      Serial.println("Team A | Player 3");
    } else if (val == 4) {
      Serial.println("Team A | Player 4");
    } else if (val == 5) {
      Serial.println("Team A | Player 5");
    } else if (val == 6) {
      Serial.println("Team A | Player 6");
    } else if (val == 7) {
      Serial.println("Team A | Player 7");
    } else if (val == 8) {
      Serial.println("Team A | Player 8");
    } else if (val == 9) {
      Serial.println("Team A | Player 9");
    } else if (val == 10) {
      Serial.println("Team A | Player 10");
    } else if (val == 11) {
      Serial.println("Team A | Player 11");
    } else if (val == 12) {
      Serial.println("Team A | Player 12");

    } else if (val == 13) {
      Serial.println("Team B | Player 1");
    } else if (val == 14) {
      Serial.println("Team B | Player 2");
    } else if (val == 15) {
      Serial.println("Team B | Player 3");
    } else if (val == 16) {
      Serial.println("Team B | Player 4");
    } else if (val == 17) {
      Serial.println("Team B | Player 5");
    } else if (val == 18) {
      Serial.println("Team B | Player 6");
    } else if (val == 19) {
      Serial.println("Team B | Player 7");
    } else if (val == 20) {
      Serial.println("Team B | Player 8");
    } else if (val == 21) {
      Serial.println("Team B | Player 9");
    } else if (val == 22) {
      Serial.println("Team B | Player 10");
    } else if (val == 23) {
      Serial.println("Team B | Player 11");
    } else if (val == 24) {
      Serial.println("Team B | Player 12");
    } else if (val == 25) {
      Serial.println("CALLBACK_NEW_GAME");
    } else if (val == 26) {
      Serial.println("CB_BUZZER");
    } else if (val == 27) {
      Serial.println("CALLBACK_EXIT");
    } else if (val == 28) {
      Serial.println("CB_GameON");
    } else if (val == 29) {
      Serial.println("CB_TF_ENABLE");
    } else if (val == 30) {
      Serial.println("CB_TF1_P1");
    } else if (val == 31) {
      Serial.println("CB_TF1_M1");
    } else if (val == 32) {
      Serial.println("CB_TF2_P1");
    } else if (val == 33) {
      Serial.println("CB_TF2_M1");
    } else if (val == 34) {
      Serial.println("CB_TF3_P1");
    } else if (val == 35) {
      Serial.println("CB_TF3_M1");
    } else if (val == 36) {
      Serial.println("CB_TF4_P1");
    } else if (val == 37) {
      Serial.println("CB_TF4_M1");
    } else if (val == 38) {
      Serial.println("CALLBACK_PERIOD_P");
    } else if (val == 39) {
      Serial.println("CALLBACK_PERIOD_M");
    } else if (val == 40) {
      Serial.println("CALLBACK_SHOTCLK_24");
    } else if (val == 41) {
      Serial.println("CALLBACK_SHOTCLK_14");
    } else if (val == 42) {
      Serial.println("CALLBACK_SHOTCLK_SET");
    } else if (val == 43) {
      Serial.println("CALLBACK_START_STOP");
    } else if (val == 44) {
      Serial.println("CALLBACK_SET");
    } else if (val == 45) {
      Serial.println("CB_Home_P1");
    } else if (val == 46) {
      Serial.println("CB_Home_M1");
    } else if (val == 47) {
      Serial.println("CB_Guest_M1");
    } else if (val == 48) {
      Serial.println("CB_Home_P2");
    } else if (val == 49) {
      Serial.println("CB_Home_P3");
    } else if (val == 50) {
      Serial.println("CB_Guest_P1");
    } else if (val == 51) {
      Serial.println("CB_Guest_P2");
    } else if (val == 52) {
      Serial.println("CB_Guest_P3");
    }
  }

  if (connected_flag) {
    lv_obj_set_style_bg_color(ui_PanelConnection, lv_color_hex(0x39F00B), LV_PART_MAIN | LV_STATE_DEFAULT);

  } else {
    lv_obj_set_style_bg_color(ui_PanelConnection, lv_color_hex(0xff1100), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  lv_timer_handler(); /* let the GUI do its work */
  delay(10);
}

