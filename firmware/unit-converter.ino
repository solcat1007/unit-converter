/* ============================================================
 * 单位换算 · ESP32 固件
 * ------------------------------------------------------------
 * 功能：
 *   1. 自建 WiFi 热点（AP 模式），手机连上即可访问
 *   2. 用 LittleFS 存放网页文件，WebServer 对外提供静态服务
 *   3. OLED 实时显示热点名与访问地址
 *   4. 常用单位互转
 *
 * 接线（ESP32 DevKitC + SSD1306 128x64 OLED，I2C）：
 *   OLED VCC → 3.3V
 *   OLED GND → GND
 *   OLED SDA → GPIO21
 *   OLED SCL → GPIO22
 *
 *   
 *
 * 烧录步骤：
 *   1. Arduino IDE 安装 ESP32 开发板支持
 *   2. 安装库：U8g2（oliver）、WebServer 已内置于 ESP32 包
 *   3. 安装 ESP32 LittleFS 上传插件，把 firmware/data/ 整个目录上传为 LittleFS
 *   4. 再上传本 .ino  sketch
 *   5. 手机连上 WiFi 热点，浏览器访问 OLED 上显示的地址
 * ========================================================== */

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <U8g2lib.h>

/* ---------- 硬件引脚定义 ----------
 * 集中在这里，方便按实际接线修改 */
#define PIN_SDA       21      /* OLED I2C 数据线 */
#define PIN_SCL       22      /* OLED I2C 时钟线 */
#define PIN_BUTTON    4       /* 按键，内部上拉，按下为低 */
#define PIN_LED       2       /* 板载 LED（呼吸训练时做渐亮渐灭） */

/* ---------- 热点配置 ---------- */
const char* AP_SSID = "unit-converter";
const char* AP_PASS = "12345678";      /* 至少 8 位，否则 ESP32 会拒绝启动 AP */

WebServer server(80);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);



/* ============================================================
 * 网页文件服务
 * ------------------------------------------------------------
 * 用 LittleFS 读文件再回给浏览器。
 * 注意：不能用 SPIFFS 的流式接口直接 write 大文件，
 * 这里一次性读入内存 —— 单个 HTML 约 10KB，ESP32 内存够用。
 * ========================================================== */

/** 根据 URL 路径推断 Content-Type */
String mimeOf(const String& path){
  if (path.endsWith(".html")) return "text/html; charset=utf-8";
  if (path.endsWith(".css"))  return "text/css; charset=utf-8";
  if (path.endsWith(".js"))   return "application/javascript; charset=utf-8";
  if (path.endsWith(".svg"))  return "image/svg+xml";
  if (path.endsWith(".json")) return "application/json; charset=utf-8";
  if (path.endsWith(".webmanifest")) return "application/manifest+json; charset=utf-8";
  return "text/plain; charset=utf-8";
}

/** 从 LittleFS 读取文件并返回给客户端 */
bool serveFile(const String& path){
  if (!LittleFS.exists(path)) return false;

  File f = LittleFS.open(path, "r");
  if (!f) return false;

  server.streamFile(f, mimeOf(path));
  f.close();
  return true;
}

/** 处理所有 GET 请求：根路径给 index.html，其余按路径找文件 */
void handleRequest(){
  String p = server.uri();
  if (p == "/" || p == "/index.html"){
    if (serveFile("/index.html")) return;
    server.send(200, "text/html; charset=utf-8", F("<h1>404</h1><p>index.html not found. Please upload the data folder to LittleFS.</p>"));
    return;
  }
  if (serveFile(p)) return;
  server.send(404, "text/plain; charset=utf-8", "Not Found");
}

/* ============================================================
 * OLED 显示
 * ========================================================== */

/** 刷新屏幕：显示热点名与访问地址 */
void refreshScreen(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(0, 12, "单位换算");
  u8g2.drawStr(0, 26, AP_SSID);

  String ip = "http://" + WiFi.softAPIP().toString();
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 42, ip.c_str());

  u8g2.drawStr(0, 58, "open in browser");

  u8g2.sendBuffer();
}

/* ============================================================
 * 生命周期
 * ========================================================== */

void setup(){
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("单位换算 · ESP32 启动中"));

  // ---- 文件系统 ----
  // formatOnFail=true：首次使用或分区损坏时自动格式化，避免手动干预
  if (!LittleFS.begin(true)){
    Serial.println(F("[错误] LittleFS 挂载失败，网页文件无法提供"));
  } else {
    Serial.println(F("[OK] LittleFS 已挂载"));
  }

  // ---- 硬件初始化 ----
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  

  // ---- OLED ----
  u8g2.begin();
  u8g2.enableUTF8Print();

  // ---- WiFi 热点 ----
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  if (!ok){
    Serial.println(F("[错误] 热点启动失败"));
  } else {
    Serial.print(F("[OK] 热点已启动: ")); Serial.println(AP_SSID);
    Serial.print(F("     访问地址: http://")); Serial.println(WiFi.softAPIP());
  }

  // ---- Web 服务 ----
  // onNotFound 兜底：任意路径都返回 index.html，配合 PWA 路由更稳
  server.onNotFound(handleRequest);
  server.begin();
  Serial.println(F("[OK] Web 服务器已监听 80 端口"));

  refreshScreen();
}

/** 主循环：喂 webserver + 跑硬件交互 */
void loop(){
  server.handleClient();
  
  delay(20);
}
