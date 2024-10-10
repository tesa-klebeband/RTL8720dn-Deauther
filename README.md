# RTL8720dn-Deauther
![GitHub Issues or Pull Requests](https://img.shields.io/github/issues/tesa-klebeband/RTL8720dn-Deauther)
![GitHub License](https://img.shields.io/github/license/tesa-klebeband/RTL8720dn-Deauther)
![GitHub Repo stars](https://img.shields.io/github/stars/tesa-klebeband/RTL8720dn-Deauther?style=flat)
![GitHub forks](https://img.shields.io/github/forks/tesa-klebeband/RTL8720dn-Deauther?style=flat)
![logo](https://github.com/user-attachments/assets/ab8ebf84-eee2-4298-8975-2e8dad13c1b3)


My ESP32-Deauther ported to the RTL8720dn, allowing users to deauthenticate on 5Ghz now!
# DISCLAIMER
This tool has been made for educational and testing purposes only. Any misuse or illegal activities conducted with the tool are strictly prohibited. I am **not** responsible for any consequences arising from the use of the tool, which is done at your own risk.
## Building
Building is done using the ArduinoIDE. Make sure you have added the board manager URL for Realtek MCUs and installed them in the board manager.
1) Clone this repo using `git clone https://github.com/tesa-klebeband/RTL8720dn-Deauther.git`
2) Open the cloned folder in the Arduino IDE
3) Connect your board and hit upload
### Notes
Most RTL8720dn devboards have the upload and log serial ports isolated. Make sure to bridge **LOG_UART_TX** and **LP_UART_TX**, as well as **LOG_UART_RX** and **LP_UART_RX** during uploading.
## Using RTL8720dn-Deauther
The RTL8720dn hosts a WiFi network with the name of `RTL8720dn-Deauther` and a password of `0123456789`. Connect to this network and type the IP of your RTL8720dn (typically **192.168.1.1**) into a webbrowser of a choice. You will see the following options:
* Rescan networks: Rescan and detect all WiFi networks in your area. After a successful scan, the networks are listed in the above table.
* Launch Deauth-Attack: Deauthenticates all clients connected to a network. Enter the network number from the table at the top and a reason code from the table at the bottom of the page. After that, click **Launch Deauth-Attack**.
* Stop Deauth-Attack: Stops and ongoing deauth attack. You probably have to reconnect to the Deauthers WiFi after starting an attack.
### Leds
The RTL8720dn-Deauther utilizes the RGB led that most devboards have. This is what the different colors indicate:
* Red: The system state. Lights up when the system is usable.
* Green: Lights up when a HTTP communication between a device and the Deauther is happening.
* Blue: Flashes when a deauth frame is being sent.
## License
All files within this repo are released under the GNU GPL V3 License as per the LICENSE file stored in the root of this repo.
