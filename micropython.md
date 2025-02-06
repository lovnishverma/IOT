To upload this **MicroPython** code to your **ESP8266**, follow these steps:

---

## **Step 1: Install MicroPython Firmware on ESP8266**

1. **Download MicroPython Firmware**:
   - Visit the official MicroPython download page: [https://micropython.org/download/esp8266/](https://micropython.org/download/esp8266/)
   - Download the latest **.bin** file for ESP8266.

2. **Install esptool** (Python tool for flashing firmware):
   Open a terminal (or Command Prompt) and run:
   ```bash
   pip install esptool
   ```

3. **Erase the current firmware**:
   Put the ESP8266 in **flash mode** by holding down the **GPIO0 (FLASH)** button while connecting it to your computer.
   
   Then, execute the following command to erase the flash:
   ```bash
   esptool.py --port COMX erase_flash
   ```
   Replace `COMX` with the correct port (`COM3`, `/dev/ttyUSB0`, etc.).

4. **Flash MicroPython Firmware**:
   ```bash
   esptool.py --port COMX --baud 460800 write_flash --flash_size=detect 0 <firmware-file-name>.bin
   ```

   Example:
   ```bash
   esptool.py --port COM3 --baud 460800 write_flash --flash_size=detect 0 esp8266-20220117-v1.18.bin
   ```

---

## **Step 2: Install a MicroPython IDE or Tool**

### Option 1: Use **Thonny IDE** (Recommended)
1. **Download and install Thonny**: [https://thonny.org](https://thonny.org)
2. Open Thonny and select:
   ```
   Tools > Options > Interpreter
   ```
   - Choose **MicroPython (ESP8266)**.
   - Set the **Port** to the one your ESP8266 is connected to.

3. **Upload Code**:
   - Copy and paste the MicroPython code into Thonny.
   - Save the file as `main.py` or `boot.py` (these run automatically on startup).
   - Click the **Run** button to execute it.

### Option 2: Use **uPyCraft IDE**
1. **Download and install uPyCraft**: [uPyCraft](https://github.com/DFRobot/uPyCraft)
2. Connect to your ESP8266 by selecting the correct serial port.
3. Upload the `main.py` file:
   ```
   Tools > New File > Paste your code > Save as main.py
   ```
4. Click **Download and Run**.

---

## **Step 3: Verifying Your Code**
1. After uploading, your ESP8266 will restart.
2. Open the **Serial Monitor** in Thonny or uPyCraft to view debug messages.
3. Verify that:
   - It connects to Wi-Fi.
   - The MQTT broker connection is successful.
   - The web server is accessible at `http://<ESP8266-IP>`.

---

Let me know if you want to use a specific IDE, and I can guide you with detailed steps!
