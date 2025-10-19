========================
CODE SNIPPETS
========================
TITLE: Managed Component Light Example Setup
DESCRIPTION: This snippet outlines the setup for the Managed Component Light example, which automatically configures GPIOs for RGB LEDs and boot buttons based on the selected Devkit Board. It utilizes the esp_matter component and the Arduino API.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/idf_component_examples/esp_matter_light/README.md#_snippet_0

LANGUAGE: Arduino
CODE:
```
#include <Arduino.h>
#include <esp_matter.h>

// Example setup code would go here, including:
// - Automatic GPIO configuration for LED and Button
// - Initialization of esp_matter component
// - Commissioning process initiation

void setup() {
  Serial.begin(115200);
  // Initialize esp_matter and other necessary components
  // esp_matter_init();
  // ... other setup code ...
}

void loop() {
  // Main loop for device operation
  // esp_matter_loop();
  // ... other loop code ...
}
```

----------------------------------------

TITLE: Arduino Example Header
DESCRIPTION: Standard header for Arduino example source files, including example name, license, and optional description.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_0

LANGUAGE: arduino
CODE:
```
/* Wi-Fi FTM Initiator Arduino Example

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/
```

----------------------------------------

TITLE: CI Configuration for Example Testing
DESCRIPTION: JSON configuration file to specify hardware requirements for running examples in the CI system.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_2

LANGUAGE: json
CODE:
```
{
  "requires": [
    "CONFIG_SOC_WIFI_SUPPORTED=y"
  ]
}
```

----------------------------------------

TITLE: WiFiScanAsync Example Output
DESCRIPTION: This snippet shows the typical log output when running the WiFiScanAsync example. It includes setup messages, scan start and end indicators, the number of networks found, and details for each network such as SSID, RSSI, Channel, and Encryption type.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiScanAsync/README.md#_snippet_0

LANGUAGE: text
CODE:
```
Setup done
Scan start
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...

Scan done
17 networks found
Nr | SSID            | RSSI | CH | Encryption
 1 | IoTNetwork      |  -62 |  1 | WPA2
 2 | WiFiSSID        |  -62 |  1 | WPA2-EAP
 3 | B3A7992         |  -63 |  6 | WPA+WPA2
 4 | WiFi            |  -63 |  6 | WPA3
 5 | IoTNetwork2     |  -64 | 11 | WPA2+WPA3
...
```

----------------------------------------

TITLE: Pre-commit Hook Installation and Usage
DESCRIPTION: Instructions for installing and running pre-commit hooks to maintain code quality and style in the Arduino-ESP32 project. Covers dependency installation, running all hooks, running specific hooks, and automatic commit hook installation.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_17

LANGUAGE: bash
CODE:
```
pip install -U -r tools/pre-commit/requirements.txt
pre-commit run
pre-commit run codespell
pre-commit install
```

----------------------------------------

TITLE: Arduino setup() and loop() Example
DESCRIPTION: This C++ code demonstrates the basic structure for using the Arduino setup() and loop() functions within an ESP-IDF project. It includes initializing serial communication in setup().

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/esp-idf_component.rst#_snippet_5

LANGUAGE: cpp
CODE:
```
//file: main.cpp
#include "Arduino.h"

void setup(){
  Serial.begin(115200);
  while(!Serial){
    ; // wait for serial port to connect
  }
}
```

----------------------------------------

TITLE: Runtime Test Execution Output
DESCRIPTION: Example output from running a runtime test, showing the test being prepared, executed via pytest, and the initial setup for the test session.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_12

LANGUAGE: bash
CODE:
```
lucassvaz@Lucas--MacBook-Pro esp32 % ./.github/scripts/tests_run.sh -s uart -t esp32c3
Sketch uart test type: validation
Running test: uart -- Config: Default
pytest tests --build-dir /Users/lucassvaz/.arduino/tests/esp32c3/uart/build.tmp -k test_uart --junit-xml=/Users/lucassvaz/Espressif/Arduino/hardware/espressif/esp32/tests/validation/uart/esp32c3/uart.xml --embedded-services esp, arduino
=============================================================================================== test session starts =================================================================================================
```

----------------------------------------

TITLE: Create ESP-IDF Project from Example
DESCRIPTION: Command to create a new ESP-IDF project using the Arduino-ESP32 'hello_world' example. This command automatically downloads necessary dependencies from the component registry.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/idf_component_examples/hello_world/README.md#_snippet_0

LANGUAGE: bash
CODE:
```
idf.py create-project-from-example "espressif/arduino-esp32:hello_world"
```

----------------------------------------

TITLE: Build Arduino-ESP32 Hello World Example
DESCRIPTION: Instructions to build the 'hello_world' example directly when using a cloned Arduino-ESP32 repository. Requires commenting out a specific line in the idf_component.yml file.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/idf_component_examples/hello_world/README.md#_snippet_1

LANGUAGE: bash
CODE:
```
# Comment out pre_release: true in examples/main/idf_component.yml
idf.py build
```

----------------------------------------

TITLE: Manual Windows Installation Steps
DESCRIPTION: Outlines the manual installation process for Arduino-ESP32 on Windows, involving cloning the repository, updating submodules, and running an executable to download necessary tools.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/installing.rst#_snippet_1

LANGUAGE: APIDOC
CODE:
```
Steps to install Arduino ESP32 support on Windows:

**Step 1**

1. Download and install the latest Arduino IDE ``Windows Installer`` from [arduino.cc](https://www.arduino.cc/en/Main/Software)
2. Download and install Git from [git-scm.com](https://git-scm.com/download/win)
3. Start ``Git GUI`` and do the following steps:

- Select ``Clone Existing Repository``

- Select source and destination
   - Sketchbook Directory: Usually ``C:/Users/[YOUR_USER_NAME]/Documents/Arduino`` and is listed underneath the "Sketchbook location" in Arduino preferences.
   - Source Location: ``https://github.com/espressif/arduino-esp32.git``
   - Target Directory: ``[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32``
   - Click ``Clone`` to start cloning the repository

**Step 3**

- open a `Git Bash` session pointing to ``[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32`` and execute ```git submodule update --init --recursive```
- Open ``[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32/tools`` and double-click ``get.exe``
```

----------------------------------------

TITLE: ESP32 Arduino Core Installation and Usage
DESCRIPTION: Guides for installing and using the Arduino core for ESP32 devices. Covers setup for Windows, Linux, and macOS, along with information on libraries and using the Arduino core as an ESP-IDF component.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/README.md#_snippet_0

LANGUAGE: APIDOC
CODE:
```
Installation:
  - Windows, Linux and macOS: https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html

Usage:
  - Arduino as an ESP-IDF component: https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html

Libraries:
  - Information on available libraries: https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html

Migration:
  - Migration guide from version 2.x to 3.x: https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html

Compatibility:
  - APIs compatibility with ESP8266 and Arduino-CORE (Arduino.cc): https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html#apis
```

----------------------------------------

TITLE: Install Test Dependencies
DESCRIPTION: Installs the necessary Python packages required for running the runtime tests locally. This is a prerequisite before building and running tests.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_8

LANGUAGE: bash
CODE:
```
pip install -U -r tests/requirements.txt
```

----------------------------------------

TITLE: Full Blink Example Code
DESCRIPTION: This is the complete code for the blink tutorial, combining the pin definition, setup, and loop functions for the ESP32.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/tutorials/blink.rst#_snippet_3

LANGUAGE: C++
CODE:
```
#define LED 2

void setup() {
    pinMode(LED, OUTPUT);
}

void loop() {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(100);
}
```

----------------------------------------

TITLE: Provisioning Example Output (Already Provisioned)
DESCRIPTION: Example log output showing the device connecting to previously provisioned Wi-Fi credentials.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiProv/examples/WiFiProv/README.md#_snippet_4

LANGUAGE: text
CODE:
```
[I][WiFiProv.cpp:146] beginProvision(): Already Provisioned, starting Wi-Fi STA
[I][WiFiProv.cpp:150] beginProvision(): SSID: Wce*****
[I][WiFiProv.cpp:152] beginProvision(): CONNECTING TO THE ACCESS POINT:
Connected IP address: 192.168.43.120
```

----------------------------------------

TITLE: Arduino Example Inline Comments
DESCRIPTION: Examples of inline comments for Arduino sketches, explaining complex algorithms or configurations.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_1

LANGUAGE: arduino
CODE:
```
// Number of FTM frames requested in terms of 4 or 8 bursts (allowed values - 0 (No pref), 16, 24, 32, 64)
```

LANGUAGE: arduino
CODE:
```
const char * WIFI_FTM_SSID = "WiFi_FTM_Responder"; // SSID of AP that has FTM Enabled
const char * WIFI_FTM_PASS = "ftm_responder"; // STA Password
```

----------------------------------------

TITLE: Arduino IDE Installation URLs
DESCRIPTION: Provides the JSON index URLs for installing the ESP32 platform in the Arduino IDE. Includes links for both stable and development releases, as well as mirrors for users in China.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/installing.rst#_snippet_0

LANGUAGE: APIDOC
CODE:
```
Stable release link::

   https://espressif.github.io/arduino-esp32/package_esp32_index.json

Development release link::

   https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json

Users in China might have troubles with connection and download speeds using the links above. Please use our Jihulab mirror:

Stable release link::

   https://jihulab.com/esp-mirror/espressif/arduino-esp32/-/raw/gh-pages/package_esp32_index_cn.json

Development release link::

   https://jihulab.com/esp-mirror/espressif/arduino-esp32/-/raw/gh-pages/package_esp32_dev_index_cn.json
```

----------------------------------------

TITLE: Example: UART Test on ESP32-C3
DESCRIPTION: Demonstrates the execution of the 'uart' runtime test on the ESP32-C3 target board, showing both the build and run commands.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_11

LANGUAGE: bash
CODE:
```
./.github/scripts/tests_build.sh -s uart -t esp32c3
```

LANGUAGE: bash
CODE:
```
./.github/scripts/tests_run.sh -s uart -t esp32c3
```

----------------------------------------

TITLE: Setup Function - Initialize LED Pin
DESCRIPTION: The setup function runs once when the ESP32 starts. It initializes the specified GPIO pin as an output to control the LED.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/tutorials/blink.rst#_snippet_1

LANGUAGE: C++
CODE:
```
void setup() {
    pinMode(LED, OUTPUT);
}
```

----------------------------------------

TITLE: Provisioning Example Output (SoftAP)
DESCRIPTION: Example log output demonstrating the provisioning process using the SoftAP mode, including connection details and received credentials.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiProv/examples/WiFiProv/README.md#_snippet_2

LANGUAGE: text
CODE:
```
[I][WiFiProv.cpp:117] beginProvision(): Starting AP using SOFTAP
 service_name: PROV_XXX
 password: 123456789
 pop: abcd1234

Provisioning started
Give Credentials of your access point using "Android app"

Received Wi-Fi credentials
    SSID: GIONEE M2
    Password: 123456789

Connected IP address: 192.168.43.120
Provisioning Successful
Provisioning Ends
```

----------------------------------------

TITLE: Provisioning Example Output (BLE)
DESCRIPTION: Example log output demonstrating the provisioning process using the BLE mode, including connection details and received credentials.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiProv/examples/WiFiProv/README.md#_snippet_3

LANGUAGE: text
CODE:
```
[I][WiFiProv.cpp:115] beginProvision(): Starting AP using BLE
 service_name: PROV_XXX
 pop: abcd1234

Provisioning started
Give Credentials of your access point using "Android app"

Received Wi-Fi credentials
    SSID: GIONEE M2
    Password: 123456789

Connected IP address: 192.168.43.120
Provisioning Successful
Provisioning Ends
```

----------------------------------------

TITLE: Basic Switch Implementation Example
DESCRIPTION: An example demonstrating the basic usage of the ZigbeeSwitch for controlling lights in a simple setup.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/zigbee/ep_switch.rst#_snippet_3

LANGUAGE: arduino
CODE:
```
#include "../libraries/Zigbee/examples/Zigbee_On_Off_Switch/Zigbee_On_Off_Switch.ino"
```

----------------------------------------

TITLE: WebServer Example Navigation
DESCRIPTION: Provides links to different HTML pages within the WebServer Example, allowing users to interact with the ESP32 device's web server.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/WebServer/data/index.htm#_snippet_0

LANGUAGE: html
CODE:
```
<p>The following pages are available:</p>
<ul>
<li><a href="/index.htm">/index.htm</a> - This page</li>
<li><a href="/files.htm">/files.htm</a> - Manage files on the server</li>
<li><a href="/$upload.htm">/$upload.htm</a> - Built-in upload utility</li>
<li><a href="/none.htm">/none.htm</a> - See the default response when files are not found.</li>
</ul>
```

----------------------------------------

TITLE: Install ESP32 Arduino Core on Debian/Ubuntu
DESCRIPTION: Installs the ESP32 Arduino core on Debian/Ubuntu systems. This involves setting up user permissions, installing Git and pip, cloning the repository, and running the get.py script.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/installing.rst#_snippet_3

LANGUAGE: bash
CODE:
```
sudo usermod -a -G dialout $USER && \
    sudo apt-get install git && \
    wget https://bootstrap.pypa.io/get-pip.py && \
    sudo python3 get-pip.py && \
    sudo pip3 install pyserial && \
    mkdir -p ~/Arduino/hardware/espressif && \
    cd ~/Arduino/hardware/espressif && \
    git clone https://github.com/espressif/arduino-esp32.git esp32 && \
    cd esp32/tools && \
    python3 get.py
```

----------------------------------------

TITLE: NetworkClientSecureEnterprise Example Description
DESCRIPTION: Documentation for the NetworkClientSecureEnterprise example, detailing its purpose and functionality.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/NetworkClientSecure/README.md#_snippet_9

LANGUAGE: APIDOC
CODE:
```
NetworkClientSecureEnterprise:
  This example demonstrates a secure connection to a Wi-Fi network using WPA/WPA2 Enterprise (for example eduroam),
  and establishing a secure HTTPS connection with an external server (for example arduino.php5.sk) using the defined anonymous identity, user identity, and password.

  Note: This example is outdated and might not work. For more examples see https://github.com/martinius96/ESP32-eduroam
```

----------------------------------------

TITLE: Install Documentation Dependencies
DESCRIPTION: Installs the necessary Python packages required to build the documentation. It's recommended to use a virtual environment for this.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/guides/docs_contributing.rst#_snippet_0

LANGUAGE: bash
CODE:
```
pip install -r requirements.txt
```

----------------------------------------

TITLE: Install ESP32 Arduino Core on openSUSE
DESCRIPTION: Installs the ESP32 Arduino core on openSUSE systems. This handles different Python versions for installing Git, pip, and pyserial, then clones the repository and runs the get.py script.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/installing.rst#_snippet_5

LANGUAGE: bash
CODE:
```
sudo usermod -a -G dialout $USER && \
   if [ `python --version 2>&1 | grep '2.7' | wc -l` = "1" ]; then \
   sudo zypper install git python-pip python-pyserial; \
   else \
   sudo zypper install git python3-pip python3-pyserial; \
   fi && \
   mkdir -p ~/Arduino/hardware/espressif && \
   cd ~/Arduino/hardware/espressif && \
   git clone https://github.com/espressif/arduino-esp32.git esp32 && \
   cd esp32/tools && \
   python get.py
```

----------------------------------------

TITLE: README Markdown for Supported Targets
DESCRIPTION: A Markdown table in a README file to clearly list the targets supported by an example or library, aiding users in understanding compatibility.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_5

LANGUAGE: markdown
CODE:
```
Currently, this example requires Wi-Fi and supports the following targets.

| Supported Targets | ESP32 | ESP32-S3 | ESP32-C3 | ESP32-C6 |
| ----------------- | ----- | -------- | -------- | -------- |

```

----------------------------------------

TITLE: Illuminance Sensor Implementation Example
DESCRIPTION: An example Arduino sketch demonstrating the implementation of a Zigbee illuminance sensor using the ZigbeeIlluminanceSensor class.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/zigbee/ep_illuminance_sensor.rst#_snippet_2

LANGUAGE: arduino
CODE:
```
#include "../../../libraries/Zigbee/examples/Zigbee_Illuminance_Sensor/Zigbee_Illuminance_Sensor.ino"
```

----------------------------------------

TITLE: ESP RainMaker Provisioning Configuration
DESCRIPTION: This snippet details the configuration steps required to set up ESP RainMaker examples. It includes selecting the correct partition scheme in the Arduino IDE, calling the WiFi.beginProvision() function, and choosing the appropriate provisioning method based on the specific ESP32 board being used.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/RainMaker/examples/README.md#_snippet_0

LANGUAGE: Arduino
CODE:
```
// 1. Change partition scheme (Tools -> Partition Scheme -> RainMaker)
// Example: Tools -> Partition Scheme -> RainMaker 4MB

// 2. Compulsorily call WiFi.beginProvision() after ESP RainMaker starts
// WiFi.beginProvision();

// 3. Use appropriate provisioning scheme per board:
// ESP32, ESP32-C3, ESP32-S3, ESP32-C6, ESP32-H2: BLE Provisioning
// ESP32-S2: SoftAP Provisioning

// 4. Set debug level to Info (Tools -> Core Debug Level -> Info) - Recommended
```

----------------------------------------

TITLE: NetworkClientPSK Example Description
DESCRIPTION: Documentation for the NetworkClientPSK example, detailing its purpose and functionality.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/NetworkClientSecure/README.md#_snippet_7

LANGUAGE: APIDOC
CODE:
```
NetworkClientPSK:
  Wi-Fi secure connection example for ESP32 using a pre-shared key (PSK)
  This is useful with MQTT servers instead of using a self-signed cert, tested with mosquitto.
  Running on TLS 1.2 using mbedTLS
```

----------------------------------------

TITLE: NetworkClientShowPeerCredentials Example Description
DESCRIPTION: Documentation for the NetworkClientShowPeerCredentials example, detailing its purpose and functionality.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/NetworkClientSecure/README.md#_snippet_10

LANGUAGE: APIDOC
CODE:
```
NetworkClientShowPeerCredentials:
  Example of a establishing a secure connection and then showing the fingerprint of the certificate.
  This can be useful in an IoT setting to know for sure that you are connecting to the right server.
  Especially in situations where you cannot hardcode a trusted root certificate for long
  periods of time (as they tend to get replaced more often than the lifecycle of IoT hardware).
```

----------------------------------------

TITLE: Install Pre-commit Hooks for Automatic Execution
DESCRIPTION: Installs pre-commit hooks into the Git repository, configuring them to automatically run against changed files before every `git commit`. This ensures continuous code quality enforcement.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_20

LANGUAGE: bash
CODE:
```
pre-commit install
```

----------------------------------------

TITLE: Install ESP32 Arduino Core on Fedora
DESCRIPTION: Installs the ESP32 Arduino core on Fedora systems. This involves setting up user permissions, installing Git, pip, and pyserial, cloning the repository, and running the get.py script.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/installing.rst#_snippet_4

LANGUAGE: bash
CODE:
```
sudo usermod -a -G dialout $USER && \
    sudo dnf install git python3-pip python3-pyserial && \
    mkdir -p ~/Arduino/hardware/espressif && \
    cd ~/Arduino/hardware/espressif && \
    git clone https://github.com/espressif/arduino-esp32.git esp32 && \
    cd esp32/tools && \
    python get.py
```

----------------------------------------

TITLE: Example: Check and Initialize Namespace
DESCRIPTION: An example demonstrating how to open a namespace, check for the existence of a key, and execute different code blocks based on whether the key exists, indicating a first-time run or a subsequent launch.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/tutorials/preferences.rst#_snippet_4

LANGUAGE: arduino
CODE:
```
Preferences mySketchPrefs;
String doesExist;

mySketchPrefs.begin("myPrefs", false);   // open (or create and then open if it does not
                                            //  yet exist) the namespace "myPrefs" in RW mode.

bool doesExist = mySketchPrefs.isKey("myTestKey");

if (doesExist == false) {
    /*
       If doesExist is false, we will need to create our
        namespace key(s) and store a value into them.
   */

   // Insert your "first time run" code to create your keys & assign their values below here.
}
else {
   /*
       If doesExist is true, the key(s) we need have been created before
        and so we can access their values as needed during startup.
   */

   // Insert your "we've been here before" startup code below here.
}
```

----------------------------------------

TITLE: NetworkClientSecure Example Description
DESCRIPTION: Documentation for the NetworkClientSecure example, detailing its purpose and functionality.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/NetworkClientSecure/README.md#_snippet_8

LANGUAGE: APIDOC
CODE:
```
NetworkClientSecure:
  Wi-Fi secure connection example for ESP32
  Running on TLS 1.2 using mbedTLS
```

----------------------------------------

TITLE: Handling setup() and loop() in Autostart Mode
DESCRIPTION: In autostart mode, this configuration ensures that the Arduino setup() and loop() functions are linked correctly by adding their C++ mangled names to the undefined symbols list, preventing circular dependencies with the main component.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/CMakeLists.txt#_snippet_49

LANGUAGE: cmake
CODE:
```
if(CONFIG_AUTOSTART_ARDUINO)
    # in autostart mode, arduino-esp32 contains app_main() function and needs to
    # reference setup() and loop() in the main component. If we add main
    # component to priv_requires then we create a large circular dependency
    # (arduino-esp32 -> main -> arduino-esp32) and can get linker errors, so
    # instead we add setup() and loop() to the undefined symbols list so the
    # linker will always include them.
    #
    # (As they are C++ symbol, we need to add the C++ mangled names.)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _Z5setupv -u _Z4loopv")
endif()
```

----------------------------------------

TITLE: Project Folder Structure with Arduino Libraries
DESCRIPTION: Shows the updated directory structure of an ESP-IDF project after integrating custom Arduino libraries. It highlights the `components/` folder where libraries are cloned and the necessary `CMakeLists.txt` file within each library.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/idf_component_examples/hello_world/README.md#_snippet_4

LANGUAGE: Text
CODE:
```
├── CMakeLists.txt
├── components
│   ├── user_library
│   │   ├── CMakeLists.txt     This needs to be added
│   │   ├── ...
├── main
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.cpp
└── README.md                  This is the file you are currently reading
```

----------------------------------------

TITLE: Already Provisioned Device Log Output
DESCRIPTION: Example console output showing the device automatically connecting to a previously provisioned Wi-Fi network upon startup, indicating that credentials are already available.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiProv/examples/WiFiProv/README.md#_snippet_5

LANGUAGE: Log
CODE:
```
[I][WiFiProv.cpp:146] beginProvision(): Already Provisioned, starting Wi-Fi STA
[I][WiFiProv.cpp:150] beginProvision(): SSID: Wce*****
[I][WiFiProv.cpp:152] beginProvision(): CONNECTING TO THE ACCESS POINT:
Connected IP address: 192.168.43.120
```

----------------------------------------

TITLE: Install ESP32 Board Support on openSUSE for Arduino IDE
DESCRIPTION: This command sequence installs Git and Python dependencies (checking for Python 2.7 or 3.x), sets user permissions for serial port access, then clones the `arduino-esp32` repository and runs its `get.py` script to fetch necessary tools for ESP32 board support in Arduino IDE on openSUSE.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/installing.rst#_snippet_9

LANGUAGE: bash
CODE:
```
sudo usermod -a -G dialout $USER && \
if [ \`python --version 2>&1 | grep '2.7' | wc -l\` = "1" ]; then \
sudo zypper install git python-pip python-pyserial; \
else \
sudo zypper install git python3-pip python3-pyserial; \
fi && \
mkdir -p ~/Arduino/hardware/espressif && \
cd ~/Arduino/hardware/espressif && \
git clone https://github.com/espressif/arduino-esp32.git esp32 && \
cd esp32/tools && \
python get.py
```

----------------------------------------

TITLE: FTM Responder Log Output
DESCRIPTION: This is the expected log output when the FTM Responder example is running on an ESP32 device. It indicates the boot process and the start of the FTM Responder service.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/FTM/FTM_Responder/README.md#_snippet_1

LANGUAGE: Log
CODE:
```
Build:Oct 25 2019
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3ffe6100,len:0x4b0
load:0x4004c000,len:0xa6c
load:0x40050000,len:0x25c4
entry 0x4004c198
Starting SoftAP with FTM Responder support
```

----------------------------------------

TITLE: Setup with Preferences
DESCRIPTION: Demonstrates a setup function that initializes Preferences, checks for an existing configuration, and sets factory defaults if it's the first run. It handles opening and closing namespaces in different modes (read-only and read-write) and storing various data types.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/tutorials/preferences.rst#_snippet_8

LANGUAGE: arduino
CODE:
```
#include <Preferences.h>

#define RW_MODE false
#define RO_MODE true

Preferences stcPrefs;

void setup() {

   // not the complete setup(), but in setup(), include this...

   stcPrefs.begin("STCPrefs", RO_MODE);           // Open our namespace (or create it
                                                     //  if it doesn't exist) in RO mode.

   bool tpInit = stcPrefs.isKey("nvsInit");       // Test for the existence
                                                     // of the "already initialized" key.

   if (tpInit == false) {
      // If tpInit is 'false', the key "nvsInit" does not yet exist therefore this
      //  must be our first-time run. We need to set up our Preferences namespace keys. So...
      stcPrefs.end();                             // close the namespace in RO mode and...
      stcPrefs.begin("STCPrefs", RW_MODE);        //  reopen it in RW mode.


      // The .begin() method created the "STCPrefs" namespace and since this is our
      //  first-time run we will create
      //  our keys and store the initial "factory default" values.
      stcPrefs.putUChar("curBright", 10);
      stcPrefs.putString("talChan", "one");
      stcPrefs.putLong("talMax", -220226);
      stcPrefs.putBool("ctMde", true);

      stcPrefs.putBool("nvsInit", true);          // Create the "already initialized"
                                                     //  key and store a value.

      // The "factory defaults" are created and stored so...
      stcPrefs.end();                             // Close the namespace in RW mode and...
      stcPrefs.begin("STCPrefs", RO_MODE);        //  reopen it in RO mode so the setup code
                                                     //  outside this first-time run 'if' block
                                                     //  can retrieve the run-time values
                                                     //  from the "STCPrefs" namespace.
   }

   // Retrieve the operational parameters from the namespace
   //  and save them into their run-time variables.
   currentBrightness = stcPrefs.getUChar("curBright");  //
   tChannel = stcPrefs.getString("talChan");            //  The LHS variables were defined
   tChanMax = stcPrefs.getLong("talMax");               //   earlier in the sketch.
   ctMode = stcPrefs.getBool("ctMde");                  //

   // All done. Last run state (or the factory default) is now restored.
   stcPrefs.end();                                      // Close our preferences namespace.

   // Carry on with the rest of your setup code...

   // When the sketch is running, it updates any changes to an operational parameter
   //  to the appropriate key-value pair in the namespace.

}
```

----------------------------------------

TITLE: Register Arduino Library in ESP-IDF Project
DESCRIPTION: Example CMakeLists.txt content for registering a custom Arduino library within an ESP-IDF project's 'components' directory. It specifies source files, include directories, and dependencies.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/idf_component_examples/hello_world/README.md#_snippet_2

LANGUAGE: cmake
CODE:
```
idf_component_register(SRCS "user_library.cpp" "another_source.c"
                      INCLUDE_DIRS "."
                      REQUIRES arduino-esp32
                      )
```

----------------------------------------

TITLE: WiFi Scan Example Output
DESCRIPTION: This snippet shows the typical log output when running the WiFiScan example, including network scan results and device boot information.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiScanDualAntenna/README.md#_snippet_0

LANGUAGE: Shell
CODE:
```
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:1412
load:0x40078000,len:13400
load:0x40080400,len:3672
entry 0x400805f8
Setup done
scan start
scan done
17 networks found
1: IoTNetwork (-62)*
2: WiFiSSID (-62)*
3: B3A7992 (-63)*
4: WiFi (-63)
5: IoTNetwork2 (-64)*
...
```

----------------------------------------

TITLE: SPI Multiple Buses Example (Arduino)
DESCRIPTION: Demonstrates how to utilize multiple SPI buses on the ESP32 with the Arduino framework. This example showcases the setup and usage of different SPI interfaces.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/api/spi.rst#_snippet_0

LANGUAGE: arduino
CODE:
```
// SPI Multiple Buses Example
// This example demonstrates how to use multiple SPI buses on the ESP32.

#include <SPI.h>

// Define pins for SPI bus 1
#define PIN_NUM_MISO   19
#define PIN_NUM_MOSI   23
#define PIN_NUM_CLK    18
#define PIN_NUM_CS1    5
#define PIN_NUM_CS2    17

// Define pins for SPI bus 2 (if available and configured)
// #define PIN_NUM_MISO2  25
// #define PIN_NUM_MOSI2  22
// #define PIN_NUM_CLK2   21
// #define PIN_NUM_CS2_2  4

SPIClass * spi_bus1 = NULL;
// SPIClass * spi_bus2 = NULL;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting SPI Multiple Buses Example");

  // Initialize SPI bus 1
  spi_bus1 = new SPIClass(VSPI);
  spi_bus1->begin(PIN_NUM_CLK, PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CS1);
  Serial.println("SPI Bus 1 initialized");

  // Initialize SPI bus 2 (if using a second bus)
  // spi_bus2 = new SPIClass(HSPI);
  // spi_bus2->begin(PIN_NUM_CLK2, PIN_NUM_MISO2, PIN_NUM_MOSI2, PIN_NUM_CS2_2);
  // Serial.println("SPI Bus 2 initialized");

  // Example: Set SPI parameters for bus 1
  spi_bus1->setFrequency(1000000); // 1 MHz
  spi_bus1->setBitOrder(MSBFIRST);
  spi_bus1->setDataMode(SPI_MODE0);

  // Example: Set SPI parameters for bus 2 (if used)
  // spi_bus2->setFrequency(500000); // 500 KHz
  // spi_bus2->setBitOrder(LSBFIRST);
  // spi_bus2->setDataMode(SPI_MODE3);

  Serial.println("SPI setup complete.");
}

void loop() {
  // Example: Transmit data on SPI bus 1
  digitalWrite(PIN_NUM_CS1, LOW);
  spi_bus1->transfer(0xAA);
  spi_bus1->transfer(0x55);
  digitalWrite(PIN_NUM_CS1, HIGH);
  Serial.println("Data sent on SPI Bus 1");

  // Example: Transmit data on SPI bus 2 (if used)
  // digitalWrite(PIN_NUM_CS2_2, LOW);
  // spi_bus2->transfer(0xFF);
  // digitalWrite(PIN_NUM_CS2_2, HIGH);
  // Serial.println("Data sent on SPI Bus 2");

  delay(2000);
}

```

----------------------------------------

TITLE: Hostname and Timezone Customization
DESCRIPTION: Provides examples of how to customize the web server's hostname and the system's timezone using preprocessor directives.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/WebServer/README.md#_snippet_12

LANGUAGE: cpp
CODE:
```
#define HOSTNAME "webserver"
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
```

----------------------------------------

TITLE: Build Application with Wi-Fi and Matter
DESCRIPTION: Instructions for building an application using Wi-Fi and Matter. This involves setting the target SoC, configuring default SDK settings, and flashing the monitor. It also includes commands to clean build artifacts.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/idf_component_examples/esp_matter_light/README.md#_snippet_3

LANGUAGE: bash
CODE:
```
rm -rf build
idf.py set-target esp32s3
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" -p /dev/ttyACM0 flash monitor
```

LANGUAGE: batch
CODE:
```
rmdir /s/q build
idf.py set-target esp32c3
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" -p com3 flash monitor
```

LANGUAGE: bash
CODE:
```
rm -rf build managed_components sdkconfig dependencies.lock
```

LANGUAGE: batch
CODE:
```
rmdir /s/q build managed_components && del sdkconfig dependencies.lock
```

----------------------------------------

TITLE: Create Project from Example with Arduino Component
DESCRIPTION: This command creates a new ESP-IDF project from a template that includes the Arduino component. It specifies the component and an example to use.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/esp-idf_component.rst#_snippet_1

LANGUAGE: bash
CODE:
```
idf.py create-project-from-example "espressif/arduino-esp32^|version|:hello_world"
```

----------------------------------------

TITLE: TensorFlow Lite for Microcontrollers Sine Example
DESCRIPTION: This example demonstrates the end-to-end workflow of training a model to replicate a sine function and converting it for inference on microcontrollers. The generated data can be used to blink LEDs or control animations.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/TFLiteMicro/examples/hello_world/README.md#_snippet_0

LANGUAGE: C++
CODE:
```
// Placeholder for C++ code related to TensorFlow Lite inference on ESP32
// This would typically involve setting up the interpreter, allocating tensors,
// and running inference with the trained model.

// Example structure:
// #include "tensorflow/lite/micro/all_ops_resolver.h"
// #include "tensorflow/lite/micro/micro_error_reporter.h"
// #include "tensorflow/lite/micro/micro_interpreter.h"
// #include "tensorflow/lite/schema/schema_generated.h"

// const unsigned char* model_data = nullptr; // Pointer to the TFLite model
// tflite::MicroErrorReporter* error_reporter = nullptr;
// tflite::AllOpsResolver* resolver = nullptr;
// tflite::MicroInterpreter* interpreter = nullptr;
// TfLiteTensor* input = nullptr;
// TfLiteTensor* output = nullptr;

// void setup() {
//   // Initialize Serial, Error Reporter, Model, Interpreter, etc.
// }

// void loop() {
//   // Run inference and process output
// }
```

----------------------------------------

TITLE: Window Covering Example Implementation
DESCRIPTION: An example implementation of the Zigbee Window Covering library, demonstrating the usage of the callback functions.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/zigbee/ep_window_covering.rst#_snippet_8

LANGUAGE: arduino
CODE:
```
#include "Zigbee_Window_Covering.h"

// Define callback functions
void liftCallback(uint8_t percentage) {
  // Handle lift percentage change
}

void tiltCallback(uint8_t percentage) {
  // Handle tilt percentage change
}

void stopCallback() {
  // Handle stop event
}

void setup() {
  // Initialize window covering
  // ...

  // Register callbacks
  onGoToLiftPercentage(liftCallback);
  onGoToTiltPercentage(tiltCallback);
  onStop(stopCallback);
}

void loop() {
  // Main loop
  // ...
}
```

----------------------------------------

TITLE: WiFiAccessPoint Example
DESCRIPTION: An example application demonstrating the use of WiFi Access Point functionality.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/guides/docs_contributing.rst#_snippet_5

LANGUAGE: arduino
CODE:
```
.. literalinclude:: ../../../libraries/WiFi/examples/WiFiAccessPoint/WiFiAccessPoint.ino
    :language: arduino
```

----------------------------------------

TITLE: NetworkClientInsecure Example Description
DESCRIPTION: Documentation for the NetworkClientInsecure example, detailing its purpose and functionality.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/NetworkClientSecure/README.md#_snippet_6

LANGUAGE: APIDOC
CODE:
```
NetworkClientInsecure:
  Demonstrates usage of insecure connection using `NetworkClientSecure::setInsecure()`
```

----------------------------------------

TITLE: Install ESP32 Board Support on macOS for Arduino IDE
DESCRIPTION: This command sequence creates the necessary directory structure under the Arduino sketchbook location on macOS, clones the `arduino-esp32` repository, and then executes the `get.py` script to install the ESP32 board support tools. Users should verify and adjust the sketchbook path if it differs from the default.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/installing.rst#_snippet_10

LANGUAGE: bash
CODE:
```
mkdir -p ~/Documents/Arduino/hardware/espressif && \
cd ~/Documents/Arduino/hardware/espressif && \
git clone https://github.com/espressif/arduino-esp32.git esp32 && \
cd esp32/tools && \
python get.py
```

----------------------------------------

TITLE: Python Pre-commit Hook Example (Conceptual)
DESCRIPTION: Illustrates a conceptual Python script that might be used as a pre-commit hook for formatting or linting. This is a placeholder as the actual hooks are defined in configuration files.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/contributing.rst#_snippet_18

LANGUAGE: python
CODE:
```
# Example of a Python pre-commit hook script (conceptual)
import sys

def main():
    # Simulate checking a file for style issues
    file_to_check = sys.argv[1] if len(sys.argv) > 1 else 'example.py'
    print(f"Running style check on {file_to_check}...")
    # In a real hook, this would involve linters like flake8 or black
    if file_to_check.endswith('.py'):
        print("Python file style check passed.")
        sys.exit(0)
    else:
        print("Unsupported file type for this hook.")
        sys.exit(1)

if __name__ == '__main__':
    main()
```

----------------------------------------

TITLE: Install Dependencies for Ubuntu
DESCRIPTION: Installs essential development dependencies on Ubuntu systems required for building the ESP32 Arduino libraries. Includes git, wget, curl, and build tools.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/lib_builder.rst#_snippet_1

LANGUAGE: bash
CODE:
```
sudo apt-get update
sudo apt-get install git wget curl libssl-dev libncurses-dev flex bison gperf cmake ninja-build ccache jq
```

----------------------------------------

TITLE: SDMMC Test Example (Arduino)
DESCRIPTION: This example demonstrates how to use the SD_MMC library to interact with SD cards using the SDMMC interface on ESP32 boards. It requires the SD_MMC library to be installed.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/api/sdmmc.rst#_snippet_0

LANGUAGE: arduino
CODE:
```
// SDMMC Test Example for ESP32 Arduino Core
// This code is intended to be used with the SD_MMC library.
// It demonstrates basic SD card operations using the SDMMC interface.

#include "FS.h"
#include "SD.h"
#include "SD_MMC.h"

void setup() {
  Serial.begin(115200);

  if(!SD_MMC.begin()) {
    Serial.println("Initialization failed!");
    return;
  }

  // Further SD card operations can be performed here
  // For example, listing files:
  File root = SD.open("/");
  File file = root.openNextFile();
  while(file){
    Serial.print("  FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }
}

void loop() {
  // Your loop code here
}
```

----------------------------------------

TITLE: Example Log Output
DESCRIPTION: This log output shows the successful initialization of multi-homed servers on an ESP32. It displays the connection status to the Wi-Fi network, the IP address of the Soft AP, and the accessible URLs for each server instance, including the mDNS resolvable address 'esp32.local'.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/MultiHomedServers/README.md#_snippet_1

LANGUAGE: text
CODE:
```
Multi-homed Servers example starting
Connecting ...
Connected to "WiFi_SSID", IP address: "192.168.42.24
Soft AP SSID: "ESP32", IP address: 192.168.4.1
MDNS responder started
SSID: WiFi_SSID
	http://192.168.42.24:8080
	http://192.168.42.24:8081
SSID: ESP32
	http://192.168.4.1:8080
	http://192.168.4.1:8081
Any of the above SSIDs
	http://esp32.local:8080
	http://esp32.local:8081
HTTP server0 started
HTTP server1 started
HTTP server2 started

```

----------------------------------------

TITLE: Example Docker Build Command
DESCRIPTION: Demonstrates how to build a Docker image using custom build arguments for the Arduino ESP32 Lib Builder, including shallow cloning from a specific repository and branch.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/lib_builder.rst#_snippet_23

LANGUAGE: bash
CODE:
```
docker buildx build -t lib-builder-custom:master \
    --build-arg LIBBUILDER_CLONE_BRANCH_OR_TAG=master \
    --build-arg LIBBUILDER_CLONE_SHALLOW=1 \
    --build-arg LIBBUILDER_CLONE_URL=https://github.com/espressif/esp32-arduino-lib-builder \
    tools/docker
```

----------------------------------------

TITLE: ESP32 Timer Initialization
DESCRIPTION: Configures a hardware timer. After successful setup, the timer automatically starts. It takes the desired frequency in Hz as input.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/api/timer.rst#_snippet_0

LANGUAGE: arduino
CODE:
```
hw_timer_t * timerBegin(uint32_t frequency);

/*
 * ``frequency`` select timer frequency in Hz. Sets how quickly the timer counter is “ticking”.
 */
```

----------------------------------------

TITLE: ESP32 HTTP Read Log Example
DESCRIPTION: This log details an HTTP GET request response from the ESP32, returning JSON data. It includes standard HTTP headers and a chunked transfer encoding, followed by the JSON payload containing channel information and feed data.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClient/README.md#_snippet_4

LANGUAGE: http
CODE:
```
HTTP/1.1 200 OK
Date: Fri, 13 Jan 2023 13:12:32 GMT
Content-Type: application/json; charset=utf-8
Transfer-Encoding: chunked
Connection: close
Status: 200 OK
Cache-Control: max-age=7, private
Access-Control-Allow-Origin: *
Access-Control-Max-Age: 1800
X-Request-Id: 91b97016-7625-44f6-9797-1b2973aa57b7
Access-Control-Allow-Headers: origin, content-type, X-Requested-With
Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS, DELETE, PATCH
ETag: W/"8e9c308fe2c50309f991586be1aff28d"
X-Frame-Options: SAMEORIGIN

1e3
{"channel":{"id":2005329,"name":"WiFiCLient example","description":"Default setup for Arduino ESP32 NetworkClient example","latitude":"0.0","longitude":"0.0","field1":"data0","created_at":"2023-01-11T15:56:08Z","updated_at":"2023-01-13T08:13:58Z","last_entry_id":2871},"feeds":[{"created_at":"2023-01-13T13:11:30Z","entry_id":2869,"field1":"359"},{"created_at":"2023-01-13T13:11:57Z","entry_id":2870,"field1":"361"},{"created_at":"2023-01-13T13:12:23Z","entry_id":2871,"field1":"363"}]}
0


Closing connection
```

----------------------------------------

TITLE: Default ESP32 Partition Table Example
DESCRIPTION: An example of a common partition table configuration for ESP32 devices, including NVS, OTA data, application partitions, and SPIFFS.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/tutorials/partition_table.rst#_snippet_6

LANGUAGE: csv
CODE:
```
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x330000,
app1,     app,  ota_1,   0x340000,0x330000,
spiffs,   data, spiffs,  0x670000,0x190000,
```

----------------------------------------

TITLE: BLE Scan Example
DESCRIPTION: Demonstrates how to perform a BLE scan using the ESP32 Arduino core. This example initializes the BLE controller and scans for nearby BLE devices.

SOURCE: https://github.com/espressif/arduino-esp32/blob/master/docs/en/api/ble.rst#_snippet_0

LANGUAGE: arduino
CODE:
```
#include "BLEDevice.h"
#include "BLEScan.h"

BLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice* advertisedDevice) {
        Serial.printf("Found advertised device: %s \n", advertisedDevice->toString().c_str());
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  BLEDevice::init("ESP32_BLE_SCAN");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5);
}

void loop() {
  delay(2000);
}
```