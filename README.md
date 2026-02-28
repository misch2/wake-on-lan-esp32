# Wake-on-LAN web interface for ESP32-C3

A small gadget that lets anyone on the local network wake up computers via a simple browser interface, hosted on minimal hardware.

<img width="592" height="579" alt="image" src="https://github.com/user-attachments/assets/e744f7fb-c296-4658-bd8b-2727e69dcc5a" />

## Features

- **Web UI** - one-click Wake-on-LAN from any device on the LAN (phone, tablet, PC)
- **Live status** - each device shows an online/offline badge updated by continuous background ICMP ping
- **Admin interface** - add/remove devices, manage admin accounts, change passwords; protected by session cookie auth
- **Multiple admin accounts** - add as many admin users as needed; each has their own login

---

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32-C3 Super Mini (or compatible) |

I used a $4 board with integrated mini 72x40 pixels OLED from https://www.aliexpress.com/item/1005007929382296.html

---

## Build & Flash

The project uses [PlatformIO](https://platformio.org/). Open the folder in VS Code with the PlatformIO extension installed, then:

```
# Build
pio run

# Flash firmware
pio run -t upload

# Monitor serial output
pio device monitor
```
---

## First-time WiFi setup

1. Power on the ESP32. The OLED (if connected) shows **"Connect to WOL-Setup"**.
2. On your phone or laptop, open WiFi settings and connect to the open network **`WOL-Setup`**.
3. A captive portal page opens automatically (if it doesn't, navigate to **192.168.4.1**).
4. Select your home/office WiFi network, enter the password, and tap **Save**.
5. The ESP32 connects, the display shows the assigned IP address (e.g. `192.168.1.42`), and the portal closes.

WiFi credentials are stored in flash. On every subsequent boot the device connects automatically without showing the portal.

---

## Accessing the interface

| URL | Description |
|-----|-------------|
| `http://<IP>/` | Main page - lists devices, shows online status, wake buttons |
| `http://<IP>/admin` | Admin page - requires login |

The IP address is shown on the OLED display after connecting. You can also find it in the serial monitor output (`[WOL] Open http://...`).

---

## Admin setup

### Default credentials

On first boot an admin account is created automatically:

| Username | Password |
|----------|----------|
| `admin`  | `admin`  |

**Change the password immediately** via the Admin page → *Change Password*.

### Adding a device

You need two pieces of information per PC: its **MAC address** and optionally its **IP address** (required for the online/offline ping indicator).

#### Finding the MAC and IP address on Windows 11

1. Press **Win + R**, type `cmd`, press **Enter**
2. Run:
   ```
   ipconfig /all
   ```
3. Find the section for your network adapter (usually *Ethernet adapter* or *Wi-Fi*).  
   The **Physical Address** line shows the MAC address, e.g. `A1-B2-C3-D4-E5-F6`.  
   Enter it in the admin form as `A1:B2:C3:D4:E5:F6` (colons instead of dashes).
4. Look for the **IPv4 Address** line in the same adapter section, e.g. `192.168.1.55`.

Alternatively:
1. Open **Settings → Network & Internet → (Ethernet or Wi-Fi) → Properties**
2. Scroll down to the **IPv4 address** and **Physical address (MAC)** fields.

> **Tip:** assign a static IP (or a DHCP reservation in your router) to each PC so the address never changes.

---

## Wake-on-LAN requirements

For WOL to work on the target PC:

1. **Enable Wake-on-LAN in BIOS/UEFI** - look for an option under *Power Management* named "Wake on LAN", "Resume by LAN", or similar; set it to **Enabled**.
2. **Enable in Windows 11:**
   - Open *Device Manager → Network Adapters → [your adapter] → Properties*
   - *Power Management* tab → check **Allow this device to wake the computer**
   - *Advanced* tab → set **Wake on Magic Packet** to **Enabled**
3. **Keep the PC plugged in** of course, WOL does not work on battery-only laptops when power is disconnected.
4. **Router/subnet** - the ESP32 and the target PC must be on the same subnet, as the magic packet is sent as a UDP broadcast to `255.255.255.255` on port 9.
