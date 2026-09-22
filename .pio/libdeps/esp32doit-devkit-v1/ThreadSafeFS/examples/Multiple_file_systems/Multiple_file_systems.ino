// 1️⃣ Include a file systems of your choice
#include <FFat.h>
#include <SD.h>


// 2️⃣ Include thread-safe wrapper
#include <threadSafeFS.h>


// 3️⃣ Crete thread-safe wrapper instances arround each base file sytems
threadSafeFS::FS tsFFat (FFat);
threadSafeFS::FS tsSD (SD);


void setup () {
  Serial.begin (115200);


  // 4️⃣ Start underlaying file systems
  tsFFat.begin (true);
  tsSD.begin (true);


  threadSafeFS::File f1 = tsFFat.open ("/test.txt", "r");
  if (f1) {
    threadSafeFS::File f2 = tsSD.open ("/test.txt", "w");
    if (f2) {

      // ...
    
      f2.close ();
    }
    f1.close ();
  } 
}

void loop () {

}
