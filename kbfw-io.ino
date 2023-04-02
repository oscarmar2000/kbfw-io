// Simple demo of three threads.
// LED blink thread, count thread, and main thread.

#include "ChRt.h"
#include <Keyboard.h>

//#include "HID-Project.h"

#include "HID_Buttons.h"

 
#if 0
#include <SPI.h>
#include <WiFi101.h>
#include <driver/source/nmasic.h>
#endif

#include <map>


class MyKeyboardKey : public KeyboardButton
{
    int mK;
  public:
    MyKeyboardKey(int key)
      : KeyboardButton(key)
      , mK(key)
    {
    }

    int getKey()
    {
      return mK;
    }

};


class MacroAbs {

  public:
    virtual void call() = 0;

};

template<typename L>
class Macro : public MacroAbs
{
    L mFunc;
  public:
    Macro(L f) : mFunc(f)
    {}

    void call() override
    {
      mFunc();
    }
};

std::map<int, MacroAbs*> macros;

bool m0_trig = false;
bool m1_trig = false;
bool m2_trig = false;
bool m3_trig = false;
bool m4_trig = false;
bool m5_trig = false;
bool m6_trig = false;
bool m7_trig = false;
bool m8_trig = false;
bool m9_trig = false;

// LED_BUILTIN pin on Arduino is usually pin 13.
// #define KEY_MENU      0xED


MyKeyboardKey *keyboard[15][5];
MyKeyboardKey *functions[12];
MyKeyboardKey *special[10];

const uint8_t gray_code[16] = { 0,  1,  3,  2,
                                6,  7,  5,  4,
                                12, 13, 15, 14,
                                10, 11,  9,  8
                              };

//------------------------------------------------------------------------------
// thread 1 - high priority for blinking LED.
// 64 byte stack beyond task switch and interrupt needs.
static THD_WORKING_AREA(waThread1, 64);

static THD_FUNCTION(Thread1 , arg) {
  (void)arg;

  pinMode(LED_BUILTIN, OUTPUT);

  // Flash led every 200 ms.
  while (true) {
    // Turn LED on.
    digitalWrite(LED_BUILTIN, HIGH);

    // Sleep for 50 milliseconds.
    chThdSleepMilliseconds(500);

    // Turn LED off.
    digitalWrite(LED_BUILTIN, LOW);

    // Sleep for 150 milliseconds.
    chThdSleepMilliseconds(500);
  }
}
//------------------------------------------------------------------------------
// thread 2 - count when higher priority threads sleep.
// 64 byte stack beyond task switch and interrupt needs.
static THD_WORKING_AREA(waThread2, 64);

static THD_FUNCTION(Thread2, arg) {
  (void)arg;

  while (true)
  {

    // Check if Fn Key is pressed
    int c = 11;
    digitalWrite(5,  ((c & 1) == 0) ? LOW : HIGH);
    digitalWrite(6,  ((c & 2) == 0) ? LOW : HIGH);
    digitalWrite(10, ((c & 4) == 0) ? LOW : HIGH);
    digitalWrite(11, ((c & 8) == 0) ? LOW : HIGH);

    bool fn = (digitalRead(12) == HIGH);

    for (int i = 1; i < 16; i++)
    {
      int m = gray_code[i];

      noInterrupts();

      digitalWrite(5,  ((m & 1) == 0) ? LOW : HIGH);
      digitalWrite(6,  ((m & 2) == 0) ? LOW : HIGH);
      digitalWrite(10, ((m & 4) == 0) ? LOW : HIGH);
      digitalWrite(11, ((m & 8) == 0) ? LOW : HIGH);


      int j = m - 1;
      if (fn)
      {
        if (j >= 1 && j <= 12) //set fX keys
          functions[j - 1]->set(digitalRead(15) == HIGH);

        if (digitalRead(16) == HIGH)
        {
          if (keyboard[j][1] != nullptr)
            if (macros.find(keyboard[j][1]->getKey()) != macros.end())
            {

              macros[keyboard[j][1]->getKey()]->call();
              interrupts();
              chThdSleepMilliseconds(50);
              continue;
            }
        }

        if (keyboard[j][4] != nullptr && keyboard[j][4]->getKey() == KEY_MENU)
        {
          special[0]->set(digitalRead(12) == HIGH);
        }

      }
      else
      {
        if (keyboard[j][0] != nullptr)
        {
          keyboard[j][0]->set(digitalRead(15) == HIGH);
        }
        if (j >= 1 && j <= 12)
          functions[j - 1]->set(false);
      }


      if (keyboard[j][1] != nullptr)
        keyboard[j][1]->set(digitalRead(16) == HIGH);
      if (keyboard[j][2] != nullptr)
        keyboard[j][2]->set(digitalRead(18) == HIGH);
      if (keyboard[j][3] != nullptr)
        keyboard[j][3]->set(digitalRead(19) == HIGH);
      if (keyboard[j][4] != nullptr)
        keyboard[j][4]->set(digitalRead(12) == HIGH);

      interrupts();
    }
    chThdSleepMilliseconds(50);
  }

}


//------------------------------------------------------------------------------
// Name chSetup is not special - must be same as used in chBegin() call.
void chSetup() {
  // Start blink thread. Priority one more than loop().
  chThdCreateStatic(waThread1, sizeof(waThread1),
                    NORMALPRIO + 1, Thread1, NULL);
  // Start count thread.  Priority one less than loop().
  chThdCreateStatic(waThread2, sizeof(waThread2),
                    NORMALPRIO - 1, Thread2, NULL);
}
//------------------------------------------------------------------------------
void setup() {
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);

  pinMode(12, INPUT_PULLDOWN);
  pinMode(15, INPUT_PULLDOWN);
  pinMode(16, INPUT_PULLDOWN);
  pinMode(18, INPUT_PULLDOWN);
  pinMode(19, INPUT_PULLDOWN);

  Keyboard.begin();

  delay(2000);
  Keyboard.print("ohayou!");


#if 0
  //Configure pins for Adafruit ATWINC1500 Feather
  WiFi.setPins(8, 7, 4, 2);

  Keyboard.println();
  Keyboard.print("WiFi101 shield: ");
  if (WiFi.status() == WL_NO_SHIELD) {
    Keyboard.println("NOT PRESENT");
    //return; // don't continue
  }
  else
  {
    Keyboard.println("DETECTED");

    // Print firmware version on the shield
    String fv = WiFi.firmwareVersion();
    String latestFv;
    Keyboard.print("Firmware version installed: ");
    Keyboard.println(fv);

    if (REV(GET_CHIPID()) >= REV_3A0) {
      // model B
      latestFv = WIFI_FIRMWARE_LATEST_MODEL_B;
    } else {
      // model A
      latestFv = WIFI_FIRMWARE_LATEST_MODEL_A;
    }

    // Print required firmware version
    Keyboard.print("Latest firmware version available : ");
    Keyboard.println(latestFv);

    // Check if the latest version is installed
    Keyboard.println();
    if (fv >= latestFv) {
      Keyboard.println("Check result: PASSED");
    } else {
      Keyboard.println("Check result: NOT PASSED");
    }

    // Print WiFi MAC address:
    printMacAddress();

    // scan for existing networks:
    Keyboard.println("Scanning available networks...");
    listNetworks();

  }

#endif

  macros['p'] = new Macro([]() {
    m0_trig = true;
  });
  macros['q'] = new Macro([]() {
    m1_trig = true;
  });
  macros['w'] = new Macro([]() {
    m2_trig = true;
  });
  macros['e'] = new Macro([]() {
    m3_trig = true;
  });
  macros['r'] = new Macro([]() {
    m4_trig = true;
  });

  memset(keyboard, 0, sizeof(keyboard));
  memset(functions, 0, sizeof(functions));
  memset(special, 0, sizeof(special));

  keyboard[0][0] = new MyKeyboardKey(KEY_ESC);
  keyboard[0][1] = new MyKeyboardKey(KEY_TAB);
  keyboard[0][2] = new MyKeyboardKey(KEY_CAPS_LOCK); // hyper
  keyboard[0][3] = new MyKeyboardKey(KEY_LEFT_SHIFT);
  keyboard[0][4] = new MyKeyboardKey(KEY_LEFT_CTRL);

  keyboard[1][0] = new MyKeyboardKey('1');
  keyboard[1][1] = new MyKeyboardKey('q');
  keyboard[1][2] = new MyKeyboardKey('a');
  keyboard[1][3] = new MyKeyboardKey('z');
  keyboard[1][4] = new MyKeyboardKey(KEY_LEFT_GUI); // super

  keyboard[2][0] = new MyKeyboardKey('2');
  keyboard[2][1] = new MyKeyboardKey('w');
  keyboard[2][2] = new MyKeyboardKey('s');
  keyboard[2][3] = new MyKeyboardKey('x');
  keyboard[2][4] = new MyKeyboardKey(KEY_LEFT_ALT);

  keyboard[3][0] = new MyKeyboardKey('3');
  keyboard[3][1] = new MyKeyboardKey('e');
  keyboard[3][2] = new MyKeyboardKey('d');
  keyboard[3][3] = new MyKeyboardKey('c');
  //  keyboard[3][4] = new MyKeyboardKey();

  keyboard[4][0] = new MyKeyboardKey('4');
  keyboard[4][1] = new MyKeyboardKey('r');
  keyboard[4][2] = new MyKeyboardKey('f');
  keyboard[4][3] = new MyKeyboardKey('v');
  //  keyboard[4][4] = new MyKeyboardKey();

  keyboard[5][0] = new MyKeyboardKey('5');
  keyboard[5][1] = new MyKeyboardKey('t');
  keyboard[5][2] = new MyKeyboardKey('g');
  keyboard[5][3] = new MyKeyboardKey('b');
  keyboard[5][4] = new MyKeyboardKey(' ');

  keyboard[6][0] = new MyKeyboardKey('6');
  keyboard[6][1] = new MyKeyboardKey('y');
  keyboard[6][2] = new MyKeyboardKey('h');
  keyboard[6][3] = new MyKeyboardKey('n');
  //  keyboard[6][4] = new MyKeyboardKey();

  keyboard[7][0] = new MyKeyboardKey('7');
  keyboard[7][1] = new MyKeyboardKey('u');
  keyboard[7][2] = new MyKeyboardKey('j');
  keyboard[7][3] = new MyKeyboardKey('m');
  //  keyboard[7][4] = new MyKeyboardKey();

  keyboard[8][0] = new MyKeyboardKey('8');
  keyboard[8][1] = new MyKeyboardKey('i');
  keyboard[8][2] = new MyKeyboardKey('k');
  keyboard[8][3] = new MyKeyboardKey(',');
  //  keyboard[8][4] = new MyKeyboardKey();

  keyboard[9][0] = new MyKeyboardKey('9');
  keyboard[9][1] = new MyKeyboardKey('o');
  keyboard[9][2] = new MyKeyboardKey('l');
  keyboard[9][3] = new MyKeyboardKey('.');
  keyboard[9][4] = new MyKeyboardKey(KEY_RIGHT_ALT);

  keyboard[10][0] = new MyKeyboardKey('0');
  keyboard[10][1] = new MyKeyboardKey('p');
  keyboard[10][2] = new MyKeyboardKey(';');
  keyboard[10][3] = new MyKeyboardKey('/');
  //  keyboard[10][4] = new MyKeyboardKey(KEY_RIGHT_GUI); //// FN

  keyboard[11][0] = new MyKeyboardKey('-');
  keyboard[11][1] = new MyKeyboardKey('[');
  keyboard[11][2] = new MyKeyboardKey('\'');
  //  keyboard[11][3] = new MyKeyboardKey();
  keyboard[11][4] = new MyKeyboardKey(KEY_MENU); /// WIN

  keyboard[12][0] = new MyKeyboardKey('=');
  keyboard[12][1] = new MyKeyboardKey(']');
  //  keyboard[12][2] = new MyKeyboardKey();
  keyboard[12][3] = new MyKeyboardKey(KEY_RIGHT_SHIFT);
  keyboard[12][4] = new MyKeyboardKey(KEY_LEFT_ARROW);

  keyboard[13][0] = new MyKeyboardKey(KEY_BACKSPACE);
  keyboard[13][1] = new MyKeyboardKey('\\');
  keyboard[13][2] = new MyKeyboardKey(KEY_RETURN);
  keyboard[13][3] = new MyKeyboardKey(KEY_UP_ARROW);
  keyboard[13][4] = new MyKeyboardKey(KEY_DOWN_ARROW);

  keyboard[14][0] = new MyKeyboardKey('`');
  keyboard[14][1] = new MyKeyboardKey(KEY_DELETE);
  keyboard[14][2] = new MyKeyboardKey(KEY_PAGE_UP);
  keyboard[14][3] = new MyKeyboardKey(KEY_PAGE_DOWN);
  keyboard[14][4] = new MyKeyboardKey(KEY_RIGHT_ARROW);

  functions[0] = new MyKeyboardKey(KEY_F1);
  functions[1] = new MyKeyboardKey(KEY_F2);
  functions[2] = new MyKeyboardKey(KEY_F3);
  functions[3] = new MyKeyboardKey(KEY_F4);
  functions[4] = new MyKeyboardKey(KEY_F5);
  functions[5] = new MyKeyboardKey(KEY_F6);
  functions[6] = new MyKeyboardKey(KEY_F7);
  functions[7] = new MyKeyboardKey(KEY_F8);
  functions[8] = new MyKeyboardKey(KEY_F9);
  functions[9] = new MyKeyboardKey(KEY_F10);
  functions[10] = new MyKeyboardKey(KEY_F11);
  functions[11] = new MyKeyboardKey(KEY_F12);

  special[0] = new MyKeyboardKey(KEY_PRINTSCR);

  chBegin(chSetup);
  // chBegin() resets stacks and should never return.
  while (true) {}
}
//------------------------------------------------------------------------------

// Runs at NORMALPRIO.
void loop() {


  //   Keyboard.begin(9600);
  //// Wait for USB Keyboard.
  //  while (!Keyboard) {}
  //
  //  // Sleep for one second.
  //  chThdSleepMilliseconds(200);
  //

  if (m0_trig)
  {
    m0_trig = false;
    Keyboard.print("macro0");
  }
  if (m1_trig)
  {
    m1_trig = false;
    Keyboard.print("macro1");
  }

  chThdSleepMilliseconds(200);

}


#if 0
void printMacAddress() {
  // the MAC address of your WiFi shield
  byte mac[6];

  // print your MAC address:
  WiFi.macAddress(mac);
  Keyboard.print("MAC: ");
  printMacAddress(mac);
}

void listNetworks() {
  // scan for nearby networks:
  Keyboard.println("** Scan Networks **");
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1)
  {
    Keyboard.println("Couldn't get a wifi connection");
    while (true);
  }

  // print the list of networks seen:
  Keyboard.print("number of available networks:");
  Keyboard.println(numSsid);

  // print the network number and name for each network found:
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Keyboard.print(thisNet);
    Keyboard.print(") ");
    Keyboard.print(WiFi.SSID(thisNet));
    Keyboard.print("\tSignal: ");
    Keyboard.print(WiFi.RSSI(thisNet));
    Keyboard.print(" dBm");
    Keyboard.print("\tEncryption: ");
    printEncryptionType(WiFi.encryptionType(thisNet));
    Keyboard.flush();
  }
}

void printEncryptionType(int thisType) {
  // read the encryption type and print out the name:
  switch (thisType) {
    case ENC_TYPE_WEP:
      Keyboard.println("WEP");
      break;
    case ENC_TYPE_TKIP:
      Keyboard.println("WPA");
      break;
    case ENC_TYPE_CCMP:
      Keyboard.println("WPA2");
      break;
    case ENC_TYPE_NONE:
      Keyboard.println("None");
      break;
    case ENC_TYPE_AUTO:
      Keyboard.println("Auto");
      break;
  }
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Keyboard.print("0");
    }
    Keyboard.print(mac[i], HEX);
    if (i > 0) {
      Keyboard.print(":");
    }
  }
  Keyboard.println();
}
#endif
