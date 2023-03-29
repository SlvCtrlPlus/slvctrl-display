#include <Arduino.h>
#include <U8g2lib.h>
#include <SerialCommands.h>
#include <Base64.h>
#include <ArduinoJson.h>

const char* DEVICE_TYPE = "display";
const int FM_VERSION = 10000; // 1.00.00
const int PROTOCOL_VERSION = 10000; // 1.00.00

U8G2_ST7565_ERC12864_F_4W_SW_SPI u8g2(U8G2_R0,/* clock=*/ 2, /* data=*/ 1, /* cs=*/ 3, /* dc=*/ 0, /* reset=*/ 4);

const int DISPLAY_HEIGHT = u8g2.getDisplayHeight();
const int DISPLAY_WIDTH = u8g2.getDisplayWidth();

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
  serial_printf(sender->GetSerial(), "status;content:%s,width:%d,height:%d\n", currentContent.c_str(), DISPLAY_WIDTH, DISPLAY_HEIGHT);
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
  u8g2.setFontPosTop();

  for (JsonVariant value : arr) {
      JsonObject root = value.as<JsonObject>();

      const char* type = root["type"];

      if (strcmp(type, "text") == 0) {
        writeText(root);
      } else if (strcmp(type, "box") == 0 || strcmp(type, "frame") == 0) {
        drawBox(root, type);
      } else if (strcmp(type, "triangle") == 0) {
        drawTriangle(root);
      }
  }

  u8g2.sendBuffer();
}

void drawTriangle(JsonObject &root)
{
  JsonArray points = root["points"].as<JsonArray>();

  if (points.size() != 3) {
    return;
  }
  
  JsonArray a = points[0].as<JsonArray>();
  JsonArray b = points[1].as<JsonArray>();
  JsonArray c = points[2].as<JsonArray>();

  if (a.size() != 2 || b.size() != 2 || c.size() != 2) { return; }

  u8g2.drawTriangle(
    get_pos_x_in_px(a[0], 0),
    get_pos_y_in_px(a[1], 0),
    
    get_pos_x_in_px(b[0], 0),
    get_pos_y_in_px(b[1], 0),
    
    get_pos_x_in_px(c[0], 0),
    get_pos_y_in_px(c[1], 0)
  );
}

void drawBox(JsonObject &root, const char* type) 
{
  const char* widthStr = root["width"];
  const char* heightStr = root["height"];
  
  if (widthStr == NULL || heightStr == NULL) { return; }

  const char* posXStr = root["posX"];
  const char* posYStr = root["posY"];

  int width = atoi(widthStr);
  int height = atoi(heightStr);
  int posX = (posXStr == NULL) ? 0 : get_pos_x_in_px(posXStr, width);
  int posY = (posYStr == NULL) ? 0 : get_pos_y_in_px(posYStr, height);

  if (strcmp(type, "box") == 0) {
    u8g2.drawBox(posX, posY, width, height);
  } else if (strcmp(type, "frame") == 0) {
    u8g2.drawFrame(posX, posY, width, height);
  }
}

void writeText(JsonObject &root)
{
  const char* font = root["font"];
  const char* posXStr = root["posX"];
  const char* posYStr = root["posY"];
  const char* text = root["text"];

  if (text == NULL) { return; }
  if (posYStr == NULL) { posYStr = "0"; }
  if (posXStr == NULL) { posXStr = "0"; }

  const uint8_t* u8g2_font = find_font(font);

  u8g2.setFont(u8g2_font);

  int strWidth = u8g2.getStrWidth(text);
  int strHeight = u8g2.getAscent();

  int posX = get_pos_x_in_px(posXStr, strWidth);
  int posY = get_pos_y_in_px(posYStr, strHeight);

  u8g2.setDrawColor(1);
  u8g2.drawStr(posX, posY, text);
}

int get_pos_x_in_px(const char* posStr, int width) 
{
  int posX;
  
  if (strcmp(posStr, "center") == 0) {
    posX = (DISPLAY_WIDTH/2)-(width/2);
  } else if (strcmp(posStr, "left") == 0) {
    posX = 0;
  } else if (strcmp(posStr, "right") == 0) {
    posX = DISPLAY_WIDTH-width;
  } else {
    posX = atoi(posStr);

    if (posX < 0) {
      // Support negative placement
      posX += DISPLAY_WIDTH-width;
    }
  }

  return posX;
}

int get_pos_y_in_px(const char* posStr, int height) 
{
  int posY;
  
  if (strcmp(posStr, "top") == 0) {
    posY = 0;
  } else if (strcmp(posStr, "center") == 0) {
    posY = (DISPLAY_HEIGHT/2)-(height/2);
  } else if (strcmp(posStr, "bottom") == 0) {
    posY = DISPLAY_HEIGHT-height;
  } else {
    posY = atoi(posStr);

    if (posY < 0) {
      // Support negative placement
      posY += DISPLAY_HEIGHT-height;
    }
  }

  return posY;
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
  u8g2.drawBox((DISPLAY_WIDTH/2)-(boxWidth/2), (DISPLAY_HEIGHT/2)-(boxHeight/2)-1, boxWidth, boxHeight);
  u8g2.setDrawColor(0);
  u8g2.drawStr((DISPLAY_WIDTH/2)-strWidthHalf, (DISPLAY_HEIGHT/2)+strHeightHalf, logoStr);

  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setDrawColor(1);
  u8g2.drawStr((DISPLAY_WIDTH/2)-(u8g2.getStrWidth(message) / 2), DISPLAY_HEIGHT+u8g2.getDescent(), message);

  u8g2.sendBuffer();
}
