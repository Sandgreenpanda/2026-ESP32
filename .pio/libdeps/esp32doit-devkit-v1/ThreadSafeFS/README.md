# ThreadSafeFS - Thread-Safe Filesystem Wrapper for ESP32


**ThreadSafeFS** is a thread-safe wrapper around ESP32 filesystems (SPIFFS, LittleFS, FFat, SD), designed for multitasking environments. It ensures safe concurrent access to files by protecting all filesystem operations with a global mutex and by preventing incompatible simultaneous file openings.


## ✨ Features


- Uses the same programming model as ESP32 file systems, like SPIFFS, LittleFS, FFat, SD ... so only a few changes are required to the source code to switch to thread-safe wrapper.


## Compatibility
ESP32
