// Please note that directories on SPIFFS are only simulated

#include <SPIFFS.h>         // Use SPIFFS as base file system (or LittleFS or FFat or SD)// Or LittleFS.h or FFat.h or SD.h ...
#include <threadSafeFS.h>   // Create thread safe wrapper arround base file system


void setup () {
  Serial.begin (115200);

  tsfs.begin (true);  // Start LittleFS (or SPIFFS or FFat or SD ...)


  // 1️⃣ Create directory structure
  tsfs.mkdir ("/var");
  tsfs.mkdir ("/var/www");
  tsfs.mkdir ("/var/www/html");

  File f;
  if ((f = tsfs.open ("/var/www/html/index.html", "w")))
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

  tsfs.mkdir ("/var/www/html/css");

  if ((f = tsfs.open ("/var/www/html/css/style.css", "w")))
    f.print ( "body { font-family: sans-serif; background: #f5f5f5; margin: 2rem; }\n"
              "h1 { color: #333; }\n"
              "#message { padding: 0.5rem 1rem; background: #ffffff; border: 1px solid #ccc; display: inline-block; margin-right: 1rem; }\n"
              "#btn { padding: 0.5rem 1rem; border: none; background: #007acc; color: #fff; cursor: pointer; }\n"
              "#btn:hover { background: #005f99; }\n" );

  tsfs.mkdir ("/var/www/html/js");

  if ((f = tsfs.open ("/var/www/html/js/app.js", "w")))
    f.print ( "document.addEventListener('DOMContentLoaded', () => {\n"
              "   const msg = document.getElementById('message');\n"
              "   const btn = document.getElementById('btn');\n"
              "\n"
              "   btn.addEventListener('click', () => {\n"
              "      msg.textContent = 'Message changed from JavaScript.';\n"
              "   });\n"
              "});" );


  // 2️⃣ List directory content
  for (auto f : tsfs.open ("/var/www/html"))
    Serial.printf ("   %s   %u bytes   [%s]\n", f.path ().c_str (), f.size (), f.isDirectory () ? "directory" : "file");
}

void loop () {

}
