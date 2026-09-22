#include <SPIFFS.h>               // Or LittleFS.h or FFat.h or SD.h ...
#include <threadSafeFS.h>         // Include thread-safe wrapper since LittleFS, FFat and SD file systems are not thread safe
threadSafeFS::FS TSFS (SPIFFS);   // Create thread-safe wrapper working instance
using File = threadSafeFS::File;  // Use thread-safe wrapper for all file operations form now on in your code


void setup () {
  Serial.begin (115200);

  SPIFFS.begin (true);  // Start LittleFS (or SPIFFS or FFat or SD ...)


  // 1️⃣ Create directory structure
  TSFS.mkdir ("/var");
  TSFS.mkdir ("/var/www");
  TSFS.mkdir ("/var/www/html");

  File f;
  if ((f = TSFS.open ("/var/www/html/index.html", "w")))
    f.print ( "<!DOCTYPE html>\n"
              "<html lang='en'>\n"
              "<head>\n"
              "   <meta charset='UTF-8'>\n"
              "   <title>Example</title>\n"
              "   <link rel='stylesheet' href='/css/style.css'>\n"
              "</head>\n"
              "<body>\n"
              "   <h1>Example</h1>\n"
              "   <p id='message'>Hello world!</p>\n"
              "   <button id='btn'>Change the message</button>\n"
              "   <script src='/js/app.js'></script>\n"
              "</body>\n"
              "</html>" );

  TSFS.mkdir ("/var/www/html/css");

  if ((f = TSFS.open ("/var/www/html/css/style.css", "w")))
    f.print ( "body { font-family: sans-serif; background: #f5f5f5; margin: 2rem; }\n"
              "h1 { color: #333; }\n"
              "#message { padding: 0.5rem 1rem; background: #ffffff; border: 1px solid #ccc; display: inline-block; margin-right: 1rem; }\n"
              "#btn { padding: 0.5rem 1rem; border: none; background: #007acc; color: #fff; cursor: pointer; }\n"
              "#btn:hover { background: #005f99; }\n" );

  TSFS.mkdir ("/var/www/html/js");

  if ((f = TSFS.open ("/var/www/html/js/app.js", "w")))
    f.print ( "document.addEventListener('DOMContentLoaded', () => {\n"
              "   const msg = document.getElementById('message');\n"
              "   const btn = document.getElementById('btn');\n"
              "\n"
              "   btn.addEventListener('click', () => {\n"
              "      msg.textContent = 'Message changed from JavaScript.';\n"
              "   });\n"
              "});" );


  // 2️⃣ List directory content
  for (auto f : TSFS.open ("/var/www/html"))
    Serial.printf ("   %s   %u bytes   [%s]\n", f.path ().c_str (), f.size (), f.isDirectory () ? "directory" : "file");
}

void loop () {

}
