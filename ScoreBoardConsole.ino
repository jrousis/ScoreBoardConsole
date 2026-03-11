#define LV_USE_PERF_MONITOR 0
#define MAX_FAULT 3

//#include "lv_demo_widgets.h"
#include <esp_now.h>
#include <WiFi.h>
#include <lvgl.h>
//#include <demos/lv_demos.h>
#include "ui.h"
#include "ui_events.h"
#include <Arduino_GFX_Library.h>
#define TFT_BL 2
#include "EEPROM.h"

struct Settings {
  char ssid[32] = "DefaultSSID";
  char password[32] = "DefaultPASS";
  uint8_t brightness = 25;
  char mac1[32] = "D8:BC:38:41:14:A0";
  char mac2[32] = "00:00:00:00:00:02";
  char countdown1[32] = "00:01:12";
  char countdown2[32] = "00:00:30";
  uint8_t teamA[12] = { 0 }; // 12 players for Team A
  uint8_t teamB[12] = { 0 }; // 12 players for Team B
};

Settings deviceSettings;

//Variables
bool Started = false;
bool ShotStarted = false;
bool connected_flag = false;

const String soft_version = "1.0.1";

uint8_t broadcastAddress1[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
uint8_t broadcastAddress2[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };
#define INSTR 0xf1 //Instruction for the old clock timer
String success; //10:97:BD:D4:59:C4
esp_err_t result;

typedef struct struct_packet {
    char insrtuction[8];
    char device[8] = "CONSOLE";
    uint8_t len;
    uint8_t data[32];

} struct_packet;

struct_packet packet_received;
struct_packet packet_sent;
esp_now_peer_info_t peerInfo;
volatile bool display_update_pending = false;
volatile bool shot_clk_on = false;
volatile bool timer_on = false;
volatile bool fauls_update_pending = false;
char display_queue_text[6] = { 0 }; 
char display_home_text[3] = { 0 };  
char display_guest_text[3] = { 0 };
char display_shotClk_text[3] = { 0 };

// Callback when data is sent
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == 0 ? "Delivery Success" : "Delivery Fail");
    if (status == 0) {
		connected_flag = true;
        lv_obj_set_style_bg_color(ui_PanelConnection, lv_color_hex(0x39F00B), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else {
		connected_flag = false;
        lv_obj_set_style_bg_color(ui_PanelConnection, lv_color_hex(0xff1100), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

// Callback when data is received
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
    memcpy(&packet_received, incomingData, sizeof(packet_received));

    /*Serial.print("Data received from: ");
    for (int i = 0; i < 6; i++) {
        Serial.print(mac[i], HEX);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
    Serial.print("Instruction: ");
    Serial.println(packet_received.insrtuction);
    Serial.print("ID: ");
    Serial.println(packet_received.device);
    Serial.print("Data Length: ");
    Serial.println(packet_received.len);
    Serial.print("Data: ");
    for (int i = 0; i < packet_received.len; i++) {
        Serial.print(packet_received.data[i], HEX);
        Serial.print(" ");
    }*/

    //Serial.println();

    //check if packet_received.insrtuction == "DISPLAY"
    if (strcmp(packet_received.insrtuction, "DISPLAY") == 0) {
        uint8_t safe_len = packet_received.len;
        if (safe_len > sizeof(packet_received.data)) {
            safe_len = sizeof(packet_received.data);
        }

        memset(display_queue_text, 0, sizeof(display_queue_text));
        if (safe_len >= 5) {
			if (packet_received.data[4] & 0x80) {
				timer_on = false;               
            }
            else {
                timer_on = true;
            }
            packet_received.data[4] &= 0x7f;
            memcpy(display_queue_text, packet_received.data, 5);
        }

        memset(display_home_text, 0, sizeof(display_home_text));
        if (safe_len >= 8) {
            display_home_text[0] = packet_received.data[6];
            display_home_text[1] = packet_received.data[7];
        }

        memset(display_guest_text, 0, sizeof(display_guest_text));
        if (safe_len >= 11) {
            display_guest_text[0] = packet_received.data[9];
            display_guest_text[1] = packet_received.data[10];
        }
        else {
            display_guest_text[0] = ' ';
            display_guest_text[0] = packet_received.data[9];
        }

        display_update_pending = true;
    }
    else if (strcmp(packet_received.insrtuction, "SHOTCLK") == 0) {
        uint8_t safe_len = packet_received.len;
        if (safe_len > sizeof(packet_received.data)) {
            safe_len = sizeof(packet_received.data);
        }

		memset(display_shotClk_text, 0, sizeof(display_shotClk_text));
        if (safe_len >= 2) {
            if (packet_received.data[1] & 0x80) { 
                shot_clk_on = false; }
			else {
				shot_clk_on = true;
            }
            display_shotClk_text[0] = packet_received.data[0];
			display_shotClk_text[1] = packet_received.data[1] & 0x7f;
		}

        display_update_pending = true;
    }
}
//===================================================================================
#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */


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

  pinMode(0, INPUT_PULLUP);
  while (!Serial);
  
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
      Serial.println("EEPROM  configured");
      Serial.println();
      // Convert mac1 and copy to broadcastAddress1
      Serial.print("MAC1 from EEPROM: ");
      Serial.println(deviceSettings.mac1);
      Serial.print("MAC2 from EEPROM: ");
      Serial.println(deviceSettings.mac2);
      sscanf(deviceSettings.mac1, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
          &broadcastAddress1[0], &broadcastAddress1[1], &broadcastAddress1[2],
          &broadcastAddress1[3], &broadcastAddress1[4], &broadcastAddress1[5]);
      sscanf(deviceSettings.mac2, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
          &broadcastAddress2[0], &broadcastAddress2[1], &broadcastAddress2[2],
          &broadcastAddress2[3], &broadcastAddress2[4], &broadcastAddress2[5]);
    }
    EEPROM.commit();

    
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

  //make the shape grey color
  lv_obj_set_style_bg_color(ui_PanelConnection, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
  // make the ui_Connected grey color               
  lv_obj_set_style_bg_color(ui_Connected, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);

  //connected_flag = false;


  // lv_textarea_set_text(ui_TextAreaPOS, "01");
  //  if (connected_flag)
  //  {
  //      lv_obj_set_style_bg_color(ui_Panel2, lv_color_hex(0x39F00B), LV_PART_MAIN | LV_STATE_DEFAULT);
  //  }
  //  else
  //  {
  //      lv_obj_set_style_bg_color(ui_Panel2, lv_color_hex(0xff1100), LV_PART_MAIN | LV_STATE_DEFAULT);
  //  }

  // Init ESP-NOW
  Serial.println();
  Serial.println("Initializing ESP-NOW...");
  // Initialize WiFi in Station mode FIRST
  WiFi.mode(WIFI_STA);
  delay(100);  // Give WiFi time to initialize

  if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");           
  }
  else {
      Serial.println("ESP-NOW initialized successfully");
      // Prnt device MAC
	  Serial.print("Device MAC: ");
	  Serial.println(WiFi.macAddress());
	  Serial.println();

      // Once ESPNow is successfully Init, we will register for Send CB to
      // get the status of Trasnmitted packet
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      //// Register peer
      memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
      peerInfo.channel = 0;
      peerInfo.encrypt = false;
      if (esp_now_add_peer(&peerInfo) != ESP_OK) {
          Serial.println("Failed to add peer");
      }
      else {
          Serial.println("Peer 1 added");
      }
  }
  //test sending
  strcpy(packet_sent.device, "CONSOLE");
  strcpy(packet_sent.insrtuction, "TEST");

  result = esp_now_send(broadcastAddress1, (uint8_t*)&packet_sent, sizeof(packet_sent));
  if (result == ESP_OK) {
      Serial.println("Send TEST");
  }
  else {
      Serial.println("Error sending");
  }

  sendTeamData(0); // Send initial team data for Team A
  sendTeamData(1); // Send initial team data for Team B
  update_screen_fauls();
}

long tim = 0;
void loop() {

  if (display_update_pending) {
    display_update_pending = false;
    Serial.print("Displaying on screen: ");
    Serial.println(display_queue_text);
    lv_textarea_set_text(ui_QueueText, display_queue_text);
    lv_textarea_set_text(ui_home, display_home_text);
    lv_textarea_set_text(ui_guest, display_guest_text);
    lv_textarea_set_text(ui_home1, display_shotClk_text);
    
    if (shot_clk_on) {
        // make display_shotClk_text font color green
		lv_obj_set_style_text_color(ui_home1, lv_color_hex(0x39F00B), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else {
        // make display_shotClk_text font color red
		lv_obj_set_style_text_color(ui_home1, lv_color_hex(0xff1100), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (timer_on) {
		// make display_queue_text font color green
        lv_obj_set_style_text_color(ui_QueueText, lv_color_hex(0x39F00B), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
	else { 
        // make display_queue_text font color red
        lv_obj_set_style_text_color(ui_QueueText, lv_color_hex(0xff1100), LV_PART_MAIN | LV_STATE_DEFAULT);
    }  
  }
  if (CHK_Flag_Func()) {

    int val = CHK_Val_Func();
    uint8_t playerNum;

    if (val == 1) {
      Serial.println("Team A | Player 1");	
	  playerNum = deviceSettings.teamA[0] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamA[0] = playerNum;
      lv_label_set_text(ui_LPlayer1, tempStr.c_str()); sendTeamData(0);
    } else if (val == 2) {
      Serial.println("Team A | Player 2");
      playerNum = deviceSettings.teamA[1] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamA[1] = playerNum;
      lv_label_set_text(ui_LPlayer2, tempStr.c_str()); sendTeamData(0);
    } else if (val == 3) {
      Serial.println("Team A | Player 3");
      playerNum = deviceSettings.teamA[2] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamA[2] = playerNum;
      lv_label_set_text(ui_LPlayer3, tempStr.c_str()); sendTeamData(0);
    } else if (val == 4) {
      Serial.println("Team A | Player 4");
	  playerNum = deviceSettings.teamA[3] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamA[3] = playerNum;
	  lv_label_set_text(ui_LPlayer4, tempStr.c_str()); sendTeamData(0);
    } else if (val == 5) {
      Serial.println("Team A | Player 5");
	  playerNum = deviceSettings.teamA[4] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamA[4] = playerNum;
	  lv_label_set_text(ui_LPlayer5, tempStr.c_str()); sendTeamData(0);
    } else if (val == 6) {
      Serial.println("Team A | Player 6");
	  playerNum = deviceSettings.teamA[5] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamA[5] = playerNum;
	  lv_label_set_text(ui_LPlayer6, tempStr.c_str()); sendTeamData(0);
    } else if (val == 7) {
      Serial.println("Team A | Player 7");
	  playerNum = deviceSettings.teamA[6] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamA[6] = playerNum;
	  lv_label_set_text(ui_LPlayer7, tempStr.c_str()); sendTeamData(0);
    } else if (val == 8) {
      Serial.println("Team A | Player 8");
	  playerNum = deviceSettings.teamA[7] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamA[7] = playerNum;
	  lv_label_set_text(ui_LPlayer8, tempStr.c_str()); sendTeamData(0);
    } else if (val == 9) {
      Serial.println("Team A | Player 9");
	  playerNum = deviceSettings.teamA[8] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamA[8] = playerNum;
	  lv_label_set_text(ui_LPlayer9, tempStr.c_str()); sendTeamData(0);
    } else if (val == 10) {
      Serial.println("Team A | Player 10");
      playerNum = deviceSettings.teamA[9] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamA[9] = playerNum;
      lv_label_set_text(ui_LPlayer10, tempStr.c_str()); sendTeamData(0);
    } else if (val == 11) {
      Serial.println("Team A | Player 11");
      playerNum = deviceSettings.teamA[10] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamA[10] = playerNum;
      lv_label_set_text(ui_LPlayer11, tempStr.c_str()); sendTeamData(0);
    } else if (val == 12) {
      Serial.println("Team A | Player 12");
      playerNum = deviceSettings.teamA[11] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamA[11] = playerNum;
      lv_label_set_text(ui_LPlayer12, tempStr.c_str()); sendTeamData(0);

    } else if (val == 13) {
      Serial.println("Team B | Player 1");
      playerNum = deviceSettings.teamB[0] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamB[0] = playerNum;
      lv_label_set_text(ui_LPlayer13, tempStr.c_str()); sendTeamData(1);
    } else if (val == 14) {
      Serial.println("Team B | Player 2");
	  playerNum = deviceSettings.teamB[1] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamB[1] = playerNum;
	  lv_label_set_text(ui_LPlayer14, tempStr.c_str()); sendTeamData(1);
    } else if (val == 15) {
      Serial.println("Team B | Player 3");
	  playerNum = deviceSettings.teamB[2] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamB[2] = playerNum;
	  lv_label_set_text(ui_LPlayer15, tempStr.c_str()); sendTeamData(1);
    } else if (val == 16) {
      Serial.println("Team B | Player 4");
	  playerNum = deviceSettings.teamB[3] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamB[3] = playerNum;
	  lv_label_set_text(ui_LPlayer16, tempStr.c_str()); sendTeamData(1);
    } else if (val == 17) {
      Serial.println("Team B | Player 5");
	  playerNum = deviceSettings.teamB[4] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamB[4] = playerNum;
	  lv_label_set_text(ui_LPlayer17, tempStr.c_str()); sendTeamData(1);
    } else if (val == 18) {
      Serial.println("Team B | Player 6");
	  playerNum = deviceSettings.teamB[5] + 1;
	  if (playerNum > MAX_FAULT) playerNum = 0;
	  String tempStr = String(playerNum); deviceSettings.teamB[5] = playerNum;
	  lv_label_set_text(ui_LPlayer18, tempStr.c_str()); sendTeamData(1);
    } else if (val == 19) {
      Serial.println("Team B | Player 7");
      playerNum = deviceSettings.teamB[6] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamB[6] = playerNum;
      lv_label_set_text(ui_LPlayer19, tempStr.c_str()); sendTeamData(1);
    } else if (val == 20) {
      Serial.println("Team B | Player 8");
      playerNum = deviceSettings.teamB[7] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamB[7] = playerNum;
      lv_label_set_text(ui_LPlayer20, tempStr.c_str()); sendTeamData(1);
    } else if (val == 21) {
      Serial.println("Team B | Player 9");
      playerNum = deviceSettings.teamB[8] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamB[8] = playerNum;
      lv_label_set_text(ui_LPlayer21, tempStr.c_str()); sendTeamData(1);
    } else if (val == 22) {
      Serial.println("Team B | Player 10");
      playerNum = deviceSettings.teamB[9] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamB[9] = playerNum;
      lv_label_set_text(ui_LPlayer22, tempStr.c_str()); sendTeamData(1);
    } else if (val == 23) {
      Serial.println("Team B | Player 11");
      playerNum = deviceSettings.teamB[10] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamB[10] = playerNum;
      lv_label_set_text(ui_LPlayer23, tempStr.c_str()); sendTeamData(1);
    } else if (val == 24) {
      Serial.println("Team B | Player 12");
      playerNum = deviceSettings.teamB[11] + 1;
      if (playerNum > MAX_FAULT) playerNum = 0;
      String tempStr = String(playerNum); deviceSettings.teamB[11] = playerNum;
      lv_label_set_text(ui_LPlayer24, tempStr.c_str()); sendTeamData(1);
    } else if (val == 25) {
      Serial.println("CALLBACK_NEW_GAME");
      sendInstructionToBoard("NEW_PER");
    } else if (val == 26) {
        sendInstructionToBoard("BUZZER");
        Serial.println("CB_BUZZER");             	    
    } else if (val == 27) {
		sendInstructionToBoard("EXIT");
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
	  sendInstructionToBoard("SHOT_1");
    } else if (val == 41) {
      Serial.println("CALLBACK_SHOTCLK_14");
	  sendInstructionToBoard("SHOT_2");
    } else if (val == 42) {
      Serial.println("CALLBACK_SHOTCLK_SET");

    } else if (val == 43) {
        sendInstructionToBoard("START");
      Serial.println("CALLBACK_START_STOP");    
    } else if (val == 44) {
        sendInstructionToBoard("STOP");
        Serial.println("CALLBACK_SET");
    } else if (val == 45) {
        sendInstructionToBoard("HOME+1");
        Serial.println("CB_Home_P1");
    } else if (val == 46) {
		sendInstructionToBoard("HOME-1");
        Serial.println("CB_Home_M1");
    } else if (val == 47) {
		sendInstructionToBoard("GUEST-1");
        Serial.println("CB_Guest_M1");
    } else if (val == 48) {
		sendInstructionToBoard("HOME+10");
        Serial.println("CB_Home_P10");
    } else if (val == 49) {
		sendInstructionToBoard("HOME-10");
        Serial.println("CB_Home_M10");
    } else if (val == 50) {
		sendInstructionToBoard("GUEST+1");
        Serial.println("CB_Guest_P1");
    } else if (val == 51) {
		sendInstructionToBoard("GUEST+10");
        Serial.println("CB_Guest_P10");
    } else if (val == 52) {
		sendInstructionToBoard("GUEST-10");
        Serial.println("CB_Guest_M10");
    } else if (val == 53) {
      Serial.println("CALLBACK_SAVE");
	  // Save settings to EEPROM
      //save mac1 and mac2 values to deviceSettings struct and EEPROM
      strncpy(deviceSettings.mac1, lv_textarea_get_text(ui_TextAreaMAC1), sizeof(deviceSettings.mac1) - 1);
      deviceSettings.mac1[sizeof(deviceSettings.mac1) - 1] = '\0'; // Ensure null termination
      strncpy(deviceSettings.mac2, lv_textarea_get_text(ui_TextAreaMAC2), sizeof(deviceSettings.mac2) - 1);
      deviceSettings.mac2[sizeof(deviceSettings.mac2) - 1] = '\0'; // Ensure null termination

      Serial.println("Updated Timer Values:");
      Serial.printf("Countdown1: %s\n", deviceSettings.countdown1);
      Serial.printf("Countdown2: %s\n", deviceSettings.countdown2);
      Serial.printf("MAC1: %s\n", deviceSettings.mac1);
      Serial.printf("MAC2: %s\n", deviceSettings.mac2);

      EEPROM.put(1, deviceSettings);
	  EEPROM.commit();
    } else {
        Serial.println("Unknown instruction");
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


// ===================================================================================
// function to send teamA or teamB all data
void sendTeamData(uint8_t team) {
    struct_packet packet;
    strcpy(packet.device, "CONSOLE");
    if (team == 0) {
        strcpy(packet.insrtuction, "TEAM_A");
        memcpy(packet.data, deviceSettings.teamA, sizeof(deviceSettings.teamA));
        packet.len = sizeof(deviceSettings.teamA);
    } else {
        strcpy(packet.insrtuction, "TEAM_B");
        memcpy(packet.data, deviceSettings.teamB, sizeof(deviceSettings.teamB));
        packet.len = sizeof(deviceSettings.teamB);
    }
    esp_err_t result = esp_now_send(broadcastAddress1, (uint8_t*)&packet, sizeof(packet));
    if (result == ESP_OK) {
        Serial.println("Team data sent successfully");
    } else {
        Serial.println("Error sending team data");
    }

	// Save deviceSettings.teamA and deviceSettings.teamB to EEPROM
    //EEPROM.put(1, deviceSettings);
    //EEPROM.commit();
}

void sendInstructionToBoard(const char* instr) {
    strncpy(packet_sent.insrtuction, instr, sizeof(packet_sent.insrtuction) - 1);
    packet_sent.insrtuction[sizeof(packet_sent.insrtuction) - 1] = '\0';
	esp_err_t result = esp_now_send(broadcastAddress1, (uint8_t*)&packet_sent, sizeof(packet_sent));
    if (result == ESP_OK) {
        Serial.printf("Instruction '%s' sent successfully\n", instr);
    } else {
        Serial.printf("Error sending instruction '%s'\n", instr);
	}
}

// function to update teamA or teamB player data to screen
void update_screen_fauls() {

    lv_label_set_text(ui_LPlayer1, deviceSettings.teamA[0] == 0 ? "0" : String(deviceSettings.teamA[0]).c_str());
	lv_label_set_text(ui_LPlayer2, deviceSettings.teamA[1] == 0 ? "0" : String(deviceSettings.teamA[1]).c_str());
	lv_label_set_text(ui_LPlayer3, deviceSettings.teamA[2] == 0 ? "0" : String(deviceSettings.teamA[2]).c_str());
	lv_label_set_text(ui_LPlayer4, deviceSettings.teamA[3] == 0 ? "0" : String(deviceSettings.teamA[3]).c_str());
	lv_label_set_text(ui_LPlayer5, deviceSettings.teamA[4] == 0 ? "0" : String(deviceSettings.teamA[4]).c_str());
	lv_label_set_text(ui_LPlayer6, deviceSettings.teamA[5] == 0 ? "0" : String(deviceSettings.teamA[5]).c_str());
	lv_label_set_text(ui_LPlayer7, deviceSettings.teamA[6] == 0 ? "0" : String(deviceSettings.teamA[6]).c_str());
	lv_label_set_text(ui_LPlayer8, deviceSettings.teamA[7] == 0 ? "0" : String(deviceSettings.teamA[7]).c_str());
	lv_label_set_text(ui_LPlayer9, deviceSettings.teamA[8] == 0 ? "0" : String(deviceSettings.teamA[8]).c_str());
	lv_label_set_text(ui_LPlayer10, deviceSettings.teamA[9] == 0 ? "0" : String(deviceSettings.teamA[9]).c_str());
	lv_label_set_text(ui_LPlayer11, deviceSettings.teamA[10] == 0 ? "0" : String(deviceSettings.teamA[10]).c_str());
	lv_label_set_text(ui_LPlayer12, deviceSettings.teamA[11] == 0 ? "0" : String(deviceSettings.teamA[11]).c_str());

	lv_label_set_text(ui_LPlayer13, deviceSettings.teamB[0] == 0 ? "0" : String(deviceSettings.teamB[0]).c_str());
	lv_label_set_text(ui_LPlayer14, deviceSettings.teamB[1] == 0 ? "0" : String(deviceSettings.teamB[1]).c_str());
	lv_label_set_text(ui_LPlayer15, deviceSettings.teamB[2] == 0 ? "0" : String(deviceSettings.teamB[2]).c_str());
	lv_label_set_text(ui_LPlayer16, deviceSettings.teamB[3] == 0 ? "0" : String(deviceSettings.teamB[3]).c_str());
	lv_label_set_text(ui_LPlayer17, deviceSettings.teamB[4] == 0 ? "0" : String(deviceSettings.teamB[4]).c_str());
	lv_label_set_text(ui_LPlayer18, deviceSettings.teamB[5] == 0 ? "0" : String(deviceSettings.teamB[5]).c_str());
	lv_label_set_text(ui_LPlayer19, deviceSettings.teamB[6] == 0 ? "0" : String(deviceSettings.teamB[6]).c_str());
	lv_label_set_text(ui_LPlayer20, deviceSettings.teamB[7] == 0 ? "0" : String(deviceSettings.teamB[7]).c_str());
	lv_label_set_text(ui_LPlayer21, deviceSettings.teamB[8] == 0 ? "0" : String(deviceSettings.teamB[8]).c_str());
	lv_label_set_text(ui_LPlayer22, deviceSettings.teamB[9] == 0 ? "0" : String(deviceSettings.teamB[9]).c_str());
	lv_label_set_text(ui_LPlayer23, deviceSettings.teamB[10] == 0 ? "0" : String(deviceSettings.teamB[10]).c_str());
	lv_label_set_text(ui_LPlayer24, deviceSettings.teamB[11] == 0 ? "0" : String(deviceSettings.teamB[11]).c_str());


}

