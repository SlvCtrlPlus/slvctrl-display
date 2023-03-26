#include <Arduino.h>
#include <U8g2lib.h>
#include <SerialCommands.h>
#include <Base64.h>
#include <ArduinoJson.h>

const char* DEVICE_TYPE = "display";
const int FM_VERSION = 10000; // 1.00.00
const int PROTOCOL_VERSION = 10000; // 1.00.00

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

U8G2_ST7565_ERC12864_F_4W_SW_SPI u8g2(U8G2_R0,/* clock=*/ 2, /* data=*/ 1, /* cs=*/ 3, /* dc=*/ 0, /* reset=*/ 4);

char serial_command_buffer[2048];
SerialCommands serialCommands(&Serial, serial_command_buffer, sizeof(serial_command_buffer), "\n", " ");

String currentContent = "";
typedef struct font_map_entry { const char* name; const uint8_t* font_ref; } font_map_entry;

font_map_entry font_map[] = {
  { "impact11", u8g2_font_ImpactBits_tr },
  { "BBSesque9", u8g2_font_BBSesque_tf },
  { "ncenB08", u8g2_font_ncenB08_tr },
  { "Born2bSporty9", u8g2_font_Born2bSportyV2_tf },
  { "michaelmouse16", u8g2_font_michaelmouse_tu },
  { "waffle12", u8g2_font_waffle_t_all },
  // { "spleen8", u8g2_font_spleen5x8_mr }, u8g2 v2.34
  { "mono4x6", u8g2_font_4x6_tf },
  { "mono5x7", u8g2_font_5x7_tf },
  { "mono5x8", u8g2_font_5x8_tf },
  { "mono6x10", u8g2_font_6x10_tf },
  { "mono6x12", u8g2_font_6x12_tf },
  { "mono6x13", u8g2_font_6x13_tf },
  { "mono7x13", u8g2_font_7x13_tf },
  { "mono7x14", u8g2_font_7x14_tf },
  { "mono8x13", u8g2_font_8x13_tf },
  { "mono9x15", u8g2_font_9x15_tf },
  { "mono9x18", u8g2_font_9x18_tf },
  { "mono10x20", u8g2_font_10x20_tf },
  { "fub11", u8g2_font_fub11_tf },
  { "fub14", u8g2_font_fub14_tf },
  { "fub17", u8g2_font_fub17_tf },
  { "fub20", u8g2_font_fub20_tf },
  { "fub25", u8g2_font_fub25_tf },
  { "fub30", u8g2_font_fub30_tf },
  { "fub35", u8g2_font_fub35_tf },
  { "fub42", u8g2_font_fub42_tf },
  { "fur11", u8g2_font_fur11_tf },
  { "fur14", u8g2_font_fur14_tf },
  { "fur17", u8g2_font_fur17_tf },
  { "fur20", u8g2_font_fur20_tf },
  { "fur25", u8g2_font_fur25_tf },
  { "fur30", u8g2_font_fur30_tf },
  { "fur35", u8g2_font_fur35_tf },
  { "fur42", u8g2_font_fur42_tf }
};

int font_map_size = sizeof(font_map) / sizeof(struct font_map_entry);

void setup()
{
  u8g2.begin();
  u8g2.setBusClock(1500000);
  u8g2.setContrast(0);
  
  print_logo("booting...");

  // Sort the array so that binary search can be used
  qsort(font_map, font_map_size, sizeof(struct font_map_entry), font_entry_cmp);

  // Add commands
  serialCommands.SetDefaultHandler(commandUnrecognized);
  serialCommands.AddCommand(new SerialCommand("introduce", commandIntroduce));
  serialCommands.AddCommand(new SerialCommand("status", commandStatus));
  serialCommands.AddCommand(new SerialCommand("attributes", commandAttributes));
  serialCommands.AddCommand(new SerialCommand("set-content", commandSetContent));

  Serial.begin(9600);

  print_logo("");
}

void loop()
{
  serialCommands.ReadSerial();
}

void commandIntroduce(SerialCommands* sender)
{
  serial_printf(sender->GetSerial(), "introduce;%s,%d,%d\n", DEVICE_TYPE, FM_VERSION, PROTOCOL_VERSION);
}

void commandStatus(SerialCommands* sender)
{
  serial_printf(sender->GetSerial(), "status;content:%s,width:%d,height:%d\n", currentContent.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT);
}

void commandAttributes(SerialCommands* sender)
{
    serial_printf(sender->GetSerial(), "attributes;content:rw[str],width:ro[int],height:ro[int]\n");
}

void commandUnrecognized(SerialCommands* sender, const char* cmd)
{
  serial_printf(sender->GetSerial(), "Unrecognized command [%s]\n", cmd);
}

void commandSetContent(SerialCommands* sender) {
  char* base64ContentStr = sender->Next();

  if (base64ContentStr == NULL) {
      sender->GetSerial()->println("set-content;;status:failed,reason:content_param_missing\n");
      return;
  }

  int base64ContentStrLength = strlen(base64ContentStr);

  int decodedLength = Base64.decodedLength(base64ContentStr, base64ContentStrLength);
  char decodedString[decodedLength + 1];
  Base64.decode(decodedString, base64ContentStr, base64ContentStrLength);

  // Clear boot logo from display
  if (currentContent.length() == 0) {
    u8g2.clearDisplay();
  }

  // Render display
  render(sender, decodedString);

  currentContent = base64ContentStr;
  
  serial_printf(sender->GetSerial(), "set-content;%s;status:successful\n\n", base64ContentStr);
}

void render(SerialCommands* sender, const char* content) 
{
  StaticJsonDocument<2048> doc;

  DeserializationError error = deserializeJson(doc, content, strlen(content));
  
  if (error) {
    serial_printf(sender->GetSerial(), "set-content;;status:failed,reason:deserialization_failed;reason:%s\n", error.c_str());
    return;
  }

  JsonArray arr = doc.as<JsonArray>();

  u8g2.clearBuffer();

  for (JsonVariant value : arr) {
      JsonObject root = value.as<JsonObject>();

      const char* font = root["font"];
      const char* posX = root["posX"];
      const char* posY = root["posY"];
      const char* text = root["text"];

      if (text == NULL) { continue; }
      if (posY == NULL) { posY = 0; }
      if (posX == NULL) { posX = 0; }
    
      const uint8_t* u8g2_font = find_font(font);

      u8g2.setFont(u8g2_font);

      int strWidth = u8g2.getStrWidth(text);
      int strHeight = u8g2.getAscent();

      int screenX;
      int screenY;

      if (strcmp(posX, "center") == 0) {
        screenX = (SCREEN_WIDTH/2)-(strWidth/2);
      } else if (strcmp(posX, "left") == 0) {
        screenX = 0;
      } else if (strcmp(posX, "right") == 0) {
        screenX = SCREEN_WIDTH-strWidth;
      } else {
        screenX = atoi(posX);

        if (screenX < 0) {
          // Support negative placement
          screenX += SCREEN_WIDTH-strWidth;
        }
      }

      if (strcmp(posY, "top") == 0) {
        screenY = u8g2.getAscent();
      } else if (strcmp(posY, "center") == 0) {
        screenY = (SCREEN_HEIGHT/2)+(strHeight/2);
      } else if (strcmp(posY, "bottom") == 0) {
        screenY = SCREEN_HEIGHT+u8g2.getDescent();
      } else {
        screenY = atoi(posY);

        if (screenY < 0) {
          // Support negative placement
          screenY += SCREEN_HEIGHT;
        } else {
          screenY += strHeight;
        }
      }

      u8g2.setDrawColor(1);
      u8g2.drawStr(screenX,screenY,text);
  }

  u8g2.sendBuffer();
}

const uint8_t* find_font(const char* input)
{
  font_map_entry* result = (font_map_entry*) bsearch(&input, font_map, font_map_size, sizeof(struct font_map_entry), font_entry_cmp);

  return (result != NULL) ? result->font_ref : u8g2_font_ncenB08_tr;
}

int font_entry_cmp(const void *v1, const void *v2)
{
  const font_map_entry* entry1 = (const font_map_entry*)v1;
  const font_map_entry* entry2 = (const font_map_entry*)v2;
  return strcmp(entry1->name, entry2->name);
}

void print_logo(const char* message)
{
  u8g2.clearBuffer(); // clear the internal memory

  // Print logo
  u8g2.setFont(u8g2_font_ImpactBits_tr);

  const char* logoStr = "SlvCtrl+";

  int strWidthHalf = u8g2.getStrWidth(logoStr) / 2;
  int strHeightHalf = u8g2.getAscent()/2;

  int boxWidth = u8g2.getStrWidth(logoStr)+10;
  int boxHeight = u8g2.getAscent()+10;

  u8g2.setDrawColor(1);
  u8g2.drawBox((SCREEN_WIDTH/2)-(boxWidth/2), (SCREEN_HEIGHT/2)-(boxHeight/2)-1, boxWidth, boxHeight);
  u8g2.setDrawColor(0);
  u8g2.drawStr((SCREEN_WIDTH/2)-strWidthHalf, (SCREEN_HEIGHT/2)+strHeightHalf, logoStr);

  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setDrawColor(1);
  u8g2.drawStr((SCREEN_WIDTH/2)-(u8g2.getStrWidth(message) / 2), SCREEN_HEIGHT+u8g2.getDescent(), message);

  u8g2.sendBuffer();
}
