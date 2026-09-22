/*

  threadSafeFS.h

    This file is part of ThreadSafeFS - Thread-Safe Filesystem Wrapper for ESP32: https://github.com/BojanJurca/Thread-safe-file-sytem-wrapper-Arduino-library-for-ESP32


  A FS wrapper with mutex for multitasking.

  Aug 12, 2026, Bojan Jurca

*/


#pragma once
#ifndef __THREAD_SAFE_FS__
    #define __THREAD_SAFE_FS__


    #include <FS.h>
    #include <ostream.hpp>
    #include <Cstring.hpp>
    #include <list.hpp>


    SemaphoreHandle_t getFsMutex ();

    namespace threadSafeFS {

        class FS;   // forward declaration

        class File {
            friend FS;

            private:
                fs::File* __file__ = NULL;
                FS* __threadSafeFileSystem__ = NULL;

                const char * __path__ ();
                const char * __name__ ();

            public:
                File () = default;
                File (FS& fs, fs::File&& f);
                File (const File&) = delete;
                File (File&& other) noexcept;
                File& operator = (File&& other) noexcept;
                File& operator = (const File&) = delete;

                ~File ();

                operator bool () const { return __file__ && __threadSafeFileSystem__; }

                // basic wrappers
                Cstring<255> path ();
                Cstring<255> name ();
                time_t getLastWrite ();
                size_t write (const uint8_t* buf, size_t len);
                size_t write (uint8_t b);


                size_t read (uint8_t* buf, size_t len);
                int read ();
                int available ();
                void flush ();
                bool seek (uint32_t pos, SeekMode mode);
                size_t position ();
                size_t size ();
                void close ();
                bool isDirectory ();
                // File openNextFile (const char* mode = FILE_READ);

                // helpers
                size_t write (const char* buf);
                size_t write (String& s);

                size_t print (const char* buf);
                size_t print (String& s);
                size_t print (const int16_t& value);
                size_t print (const uint16_t& value);
                size_t print (const int32_t& value);
                size_t print (const uint32_t& value);
                size_t print (const int64_t& value);
                size_t print (const uint64_t& value);
                size_t print (const float& value);
                size_t print (const double& value);
                size_t print (const long double& value);

                template<typename T>
                size_t println (T value) {
                    return print (value) + print ("\r\n");
                }

                size_t println () {
                    return println ();
                }

                // iterate through a directory
                class Iterator {
                    public:
                        Iterator ();
                        Iterator (FS* fs, fs::File* dir);
                        bool operator != (const Iterator& other) const;
                        File operator *();
                        Iterator& operator ++();
                    private:
                        fs::File* __dir__;
                        fs::File __current__;
                        FS* __fs__;
                        bool __end__ = false;

                        // for SPIFFS only
                        list<Cstring<31>> subDirectories;
                };

                Iterator begin ();
                Iterator end ();
        };


        class FS {
            friend class File;

        private:
            fs::FS& __fileSystem__;

        public:
            list<const char *> readOpenedFiles;
            list<const char *> writeOpenedFiles;

            FS (fs::FS& fileSystem); // for file systems

            // begin, end and format functions not supported by FS
            bool begin (bool formatOnFail = false) {
                #if defined(_LITTLEFS_H_)
                    if (&__fileSystem__ == &LittleFS)
                        return LittleFS.begin (formatOnFail);
                #endif
                #if defined(_SPIFFS_H_)
                    if (&__fileSystem__ == &SPIFFS)
                        return SPIFFS.begin (formatOnFail);
                #endif
                #if defined(_FFAT_H_)
                    if (&__fileSystem__ == &FFat)
                        return FFat.begin (formatOnFail);
                #endif
                #if defined(_SD_H_)
                    if (&__fileSystem__ == &SD)
                        return SD.begin ();
                #endif

                Serial.println ("FATAL ERROR: Unsupported filesystem. Include LittleFS, SPIFFS or FFat or SD.");
                abort (); // or esp_restart()

                return false; // unreachable
            }

            void end () {
                #if defined(_LITTLEFS_H_)
                    if (&__fileSystem__ == &LittleFS) {
                        LittleFS.end ();
                        return;
                    }
                #endif
                #if defined(_SPIFFS_H_)
                    if (&__fileSystem__ == &SPIFFS) {
                        SPIFFS.end ();
                        return;
                    }
                #endif
                #if defined(_FFAT_H_)
                    if (&__fileSystem__ == &FFat) {
                        FFat.end ();
                        return;
                    }
                #endif
                #if defined(_SD_H_)
                    if (&__fileSystem__ == &SD) {
                        SD.end ();
                        return;
                    }
                #endif

                Serial.println ("FATAL ERROR: Unsupported filesystem. Include LittleFS, SPIFFS or FFat or SD.");
                abort (); // or esp_restart ()

                return; // unreachable
            }

            bool format () {
                #if defined(_LITTLEFS_H_)
                    if (&__fileSystem__ == &LittleFS)
                        return LittleFS.format ();
                #endif

                #if defined(_SPIFFS_H_)
                    if (&__fileSystem__ == &SPIFFS)
                        return SPIFFS.format ();
                #endif

                #if defined(_FFAT_H_)
                    if (&__fileSystem__ == &FFat)
                        return FFat.format ();
                #endif

                #if defined(_SD_H_)
                    if (&__fileSystem__ == &SD) {
                        Serial.println ("ERROR: SD filesystem does not support format().");
                        return false;
                    }
                #endif

                Serial.println ("FATAL ERROR: Unsupported filesystem. Include LittleFS, SPIFFS, FFat or SD.");
                abort (); // ali esp_restart ()

                return false; // unreachable
            }

            // report what underlaying file sistem really is
            const char *name () {
                #if defined(_LITTLEFS_H_)
                    if (&__fileSystem__ == &LittleFS)
                        return "LittleFS";
                #endif
                #if defined(_SPIFFS_H_)
                    if (&__fileSystem__ == &SPIFFS)
                        return "SPIFFS";
                #endif
                #if defined(_FFAT_H_)
                    if (&__fileSystem__ == &FFat)
                        return "FFat";
                #endif
                #if defined(_SD_H_)
                    if (&__fileSystem__ == &SD)
                        return "SD";
                #endif
                return "unknown";
            }

            constexpr int fileNameMaxLen () {
                #if defined(_SPIFFS_H_)
                    if (&__fileSystem__ == &SPIFFS)
                        return 31;
                #endif
                return 255;
            }
            

            // diske sizes
            size_t totalBytes () {
                #if defined(_LITTLEFS_H_)
                    if (&__fileSystem__ == &LittleFS)
                        return LittleFS.totalBytes ();
                #endif
                #if defined(_SPIFFS_H_)
                    if (&__fileSystem__ == &SPIFFS)
                        return SPIFFS.totalBytes ();
                #endif
                #if defined(_FFAT_H_)
                    if (&__fileSystem__ == &FFat)
                        return FFat.totalBytes ();
                #endif
                #if defined(_SD_H_)
                    if (&__fileSystem__ == &SD)
                        return SD.cardSize ();
                #endif
                Serial.println ("FATAL ERROR: Unsupported filesystem. Include LittleFS, SPIFFS, FFat or SD.");
                abort ();
                return 0; // unreachable
            }

            size_t usedBytes () {
                #if defined(_LITTLEFS_H_)
                    if (&__fileSystem__ == &LittleFS)
                        return LittleFS.usedBytes ();
                #endif
                #if defined(_SPIFFS_H_)
                    if (&__fileSystem__ == &SPIFFS)
                        return SPIFFS.usedBytes ();
                #endif
                #if defined(_FFAT_H_)
                    if (&__fileSystem__ == &FFat)
                        return FFat.usedBytes ();
                #endif
                #if defined(_SD_H_)
                    if (&__fileSystem__ == &SD) {
                        return 0; // not supported for SD
                    }
                #endif
                Serial.println ("FATAL ERROR: Unsupported filesystem. Include LittleFS, SPIFFS, FFat or SD.");
                abort ();
                return 0; // unreachable
            }

            size_t freeBytes () {
                #if defined(_LITTLEFS_H_)
                    if (&__fileSystem__ == &LittleFS)
                        return LittleFS.totalBytes () - LittleFS.usedBytes ();
                #endif
                #if defined(_SPIFFS_H_)
                    if (&__fileSystem__ == &SPIFFS)
                        return SPIFFS.totalBytes () - SPIFFS.usedBytes ();
                #endif
                #if defined(_FFAT_H_)
                    if (&__fileSystem__ == &FFat)
                        return FFat.totalBytes () - FFat.usedBytes ();
                #endif
                #if defined(_SD_H_)
                    if (&__fileSystem__ == &SD) {
                        uint64_t cardSize = SD.cardSize();
                        return cardSize > 0 ? cardSize : 0; // not supported for SD
                    }
                #endif
                Serial.println ("FATAL ERROR: Unsupported filesystem. Include LittleFS, SPIFFS, FFat or SD.");
                abort ();
                return 0;
            }


            File open (const char* path, const char* mode = FILE_READ);
            File open (const String& path, const char* mode = FILE_READ);

            bool exists (const char* path);
            bool exists (const String& path);

            bool remove (const char* path);
            bool remove (const String& path);

            bool rename (const char* from, const char* to);
            bool rename (const String& from, const String& to);

            bool mkdir (const char* path);
            bool mkdir (const String& path);

            bool rmdir (const char* path);
            bool rmdir (const String& path);

            bool mounted ();

            Cstring<255> makeFullPath (const char* relativePath, const char* workingDirectory);

            bool isFile (const char *fullPath);
            bool isFile (Cstring<255>& fullPath);
            bool isDirectory (const char *fullPath);
            bool isDirectory (Cstring<255>& fullPath);

            bool userHasRightToAccessFile (const char* fullPath, const char* homeDirectory);
            bool userHasRightToAccessDirectory (Cstring<255> fullPath, Cstring<255> homeDirectory);

            Cstring<300> fileInformation (const char* fileOrDirectory, bool showFullPath = false);

            bool readConfiguration (char* buffer, size_t bufferSize, const char* fileName);
        };


        // create working singleton TSFS instance
        inline FS& instance () {
            static FS* singleton = NULL;
            if (!singleton) {
                #if defined(_LITTLEFS_H_)
                    singleton = new FS (LittleFS);
                #elif defined(_SPIFFS_H_)
                    singleton = new FS (SPIFFS);
                #elif defined(_FFAT_H_)
                    singleton = new FS (FFat);
                #elif defined(_SD_H_)
                    singleton = new FS (SD);
                #endif
            }
            return *singleton;
        }
    

    }; // namespace


    // sigleton definition of tsfs
    #ifdef _LITTLEFS_H_
        #define TSFS_LITTLEFS 1
    #else
        #define TSFS_LITTLEFS 0
    #endif
    #ifdef _SPIFFS_H_
        #define TSFS_SPIFFS 1
    #else
        #define TSFS_SPIFFS 0
    #endif
    #ifdef _FFAT_H_
        #define TSFS_FFAT 1
    #else
        #define TSFS_FFAT 0
    #endif
    #ifdef _SD_H_
        #define TSFS_SD 1
    #else
        #define TSFS_SD 0
    #endif
    #define TSFS_FS_COUNT (TSFS_LITTLEFS + TSFS_SPIFFS + TSFS_FFAT + TSFS_SD)    
    #if TSFS_FS_COUNT == 1
        static threadSafeFS::FS& tsfs = threadSafeFS::instance ();
    #endif


    // Use thread-safe wrapper for all file operations form now on in your code
    using File = threadSafeFS::File;  


    // standard C compatibility
    // threadSafeFS::File fopen (const char *path, const char *mode);
    // int fclose (threadSafeFS::File &f);
    size_t fprintf (threadSafeFS::File &f, const char *fmt, ...);


#endif