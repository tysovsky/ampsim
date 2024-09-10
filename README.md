CAD Designs and Software for a small dedicated PC used for hosting audio plugins live.

**Bill of materials:**
- **1x Gigabyte B450 I AORUS PRO WIFI + Compatible CPU + Compatible RAM**. Any ITX motherboard will work, however if you want to have a working HDMI port in the back you need a board with two HDMI outs (alternatively you can get HDMI -> Display Port adapter). For the CPU, choose one that can be cooled by a low profile cooler.
- **1x Noctua NH-L9a**: or any other low profile CPU cooler
- **1x Waveshare 7.9inch Capacitive Touch Screen**
- **1x ESI Maya44 EX Audio Interface**
- **1x HDPLEX 250W Power Supply**
- **1x PCIE Riser**
- **7x 90mm 10k Linear Potentiometers**
- **Seeed Studio XIAO SAMD21**: this is used to power the front panel with knobs and a switch
- **6x 20mm Potentiometer Knobs**
- **1x 40mm Potentiometer Knobs**
- **1x QIANRENON HDMI Panel Mount**
- **1x QIANRENON USB C Panel Mount**
- **2x Flat HDMI to HDMI Cable**
- **1x Flat USB A to USB C Cable**
- **1x AC Receptacle**
- **1x Power Button**
- **Various M3 Screws**
- **M3 Standoffs for the Motherboard**


**Manufacturing the case:**

The case is designed to be manufactured on SendCutSend using 0.125" thick aluminum with annodization. The DXF files are setup to be cut, bet, tapped and countersunk on SendCutSend without issues.
The audio interface holder and display holder need to be 3D printed. I recommend using PETG, ABS or other plastic with relatively high glass transition temperature. I suspect that PLA might eventually sag in the case. 


**Software**

The software is a lightweight VST plugin host written in C++ and JUCE. The UI is optimized for small waveshare touchscreen. Build instructions will come later. 