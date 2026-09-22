// 1️⃣ Include a file system of your choice
#include <LittleFS.h>         // Use LittleFS as base file system (or SPIFFS or FFat or SD)


// 2️⃣ Include thread-safe wrapper
#include <threadSafeFS.h>     // Create thread safe wrapper arround base file system


void setup () {
  Serial.begin (115200);


  // 3️⃣ Start underlaying file system: LittleFS (or SPIFFS or FFat or SD ...)
  tsfs.begin (true);


  // 6️⃣ Use thread-safe wrapper instance from now on

  // Create test file
  if (!tsfs.exists ("/test.txt")) {
    File f = tsfs.open ("/test.txt", "w");
    if (f) {
      f.print ("This is a test file.");
      f.close ();
      Serial.printf ("/test.txt created\n");
    } else {
      Serial.printf ("/test.txt could not be created\n");
    }
  }

  // Read it periodically in two separate tasks, one of them is setup - loop, the other one is lambda function below
  xTaskCreate ([] (void *param) {
                                  while (true) {
                                    delay (900);

                                    File f = tsfs.open ("/test.txt", "r");
                                    if (f) {

                                      // read file content
                                      char buf [100];
                                      int i = f.read ((uint8_t *) buf, sizeof (buf) - 1);
                                      buf [i] = 0;
                                      f.close ();

                                      Serial.printf ("                                    %s\n", buf);

                                    } else {
                                      Serial.printf ("                                    /test.txt could not be read\n");
                                    }

                                  }
                                  vTaskDelete (NULL);
                                }, 
                                "the_other_task", 4068, NULL, 1, NULL);

}

void loop () {
  delay (1000);

  File f = tsfs.open ("/test.txt", "r");
  if (f) {

    // read file content
    char buf [100];
    int i = 0;
    while (i < sizeof (buf) - 1 && f.available ())
      buf [i ++] = f.read ();
    buf [i] = 0;

    f.close ();

    Serial.printf ("%s\n", buf);

  } else {
    Serial.printf ("/test.txt could not be read\n");
  }

}
