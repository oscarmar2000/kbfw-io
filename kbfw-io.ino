
#include <map>
#include <array>
#include <memory>
#include <cassert>
#include <algorithm>

#include <Keyboard.h>
#include "ChRt.h"
#include "HID_Buttons.h"

#define WIFI_EN 0

#if WIFI_EN
#include <SPI.h>
#include <WiFi101.h>
#include <driver/source/nmasic.h>
#endif

// #define KEY_MENU      0xED

//------------------------------------------------------------------------------
// Classes and templates
//------------------------------------------------------------------------------

template<typename Type, int N, int M>
class matrix {
    std::array<std::array<Type, M>, N> mat{};

  public:

    static constexpr int w = M;
    static constexpr int h = N;

    constexpr matrix() = default;

    constexpr matrix(std::initializer_list<std::array<Type, M>> m)
    {
      int i = 0;
      for (auto &r : m)
      {
        mat[i] = r;
        i++;
      }
    }

    const std::array<Type, M> &operator[](int R) const
    {
      return mat[R];
    }

    constexpr Type get(int x, int y)
    {
      return mat.at(x).at(y);
    }

    constexpr void set(int x, int y, Type v)
    {
      mat.at(x).at(y) = v;
    }
};


class MyKeyboardKey
{
    std::unique_ptr<KeyboardButton> mBtn{};
    int mK{};

  public:
    MyKeyboardKey() = default;

    MyKeyboardKey& operator=(const MyKeyboardKey &other)
    {
      mK = other.mK;
      mBtn = std::make_unique<KeyboardButton>(mK);

      return *this;
    }

    MyKeyboardKey& operator=(MyKeyboardKey &&other)
    {
      mK = other.mK;
      mBtn = std::move(other.mBtn);

      return *this;
    }

    MyKeyboardKey(MyKeyboardKey && other)
      : mBtn(std::move(other.mBtn))
      , mK(other.mK) {}

    MyKeyboardKey(int key)
      : mBtn(std::make_unique<KeyboardButton>(key))
      , mK(key) {}

    void set(bool lv) const
    {
      mBtn.get()->set(lv);
    }

    int getKey() const
    {
      return mK;
    }
};


class MacroAbs
{
  public:
    virtual bool call() = 0;

};

template<typename L>
class Macro : public MacroAbs
{
    L mFunc;
  public:
    Macro(L &&f) : mFunc(std::move(f))
    {}

    bool call() override
    {
      return mFunc();
    }
};

class MacroHdlr
{
    std::map<int, std::unique_ptr<MacroAbs>> macros{};
    std::map<int, bool> triggers{};

  public:

    template<char K, typename Fn>
    void addMacro(Fn macro)
    {
      macros[K] = std::make_unique<Macro<Fn>>((std::move(macro)));
      triggers[K] = false;
    }

    bool trigger(int ch)
    {
      if (triggers.count(ch) > 0)
      {
        triggers[ch] = true;
        return true;
      }
      return false;
    }

    void execMacros()
    {
      std::find_if(triggers.begin(), triggers.end(), [this](auto & m_p) {
        if (m_p.second)
        { 
          m_p.second = macros[m_p.first]->call();
          return true;
        }
        return false;
      });
    }
};

//------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------

static MacroHdlr macroHandler;

static const matrix<MyKeyboardKey, 15, 5> keyboardM
{ // row0            row1          row2             row3               row4
  { (KEY_ESC)      , (KEY_TAB)   , (KEY_CAPS_LOCK), (KEY_LEFT_SHIFT) , (KEY_LEFT_CTRL)},
  { ('1')          , ('q')       , ('a')          , ('z')            , (KEY_LEFT_GUI)},
  { ('2')          , ('w')       , ('s')          , ('x')            , (KEY_LEFT_ALT)},
  { ('3')          , ('e')       , ('d')          , ('c')            , (0)},
  { ('4')          , ('r')       , ('f')          , ('v')            , (0)},
  { ('5')          , ('t')       , ('g')          , ('b')            , (' ')},
  { ('6')          , ('y')       , ('h')          , ('n')            , (0)},
  { ('7')          , ('u')       , ('j')          , ('m')            , (0)},
  { ('8')          , ('i')       , ('k')          , (',')            , (0)},
  { ('9')          , ('o')       , ('l')          , ('.')            , (KEY_RIGHT_ALT)},
  { ('0')          , ('p')       , (';')          , ('/')            , (0)},
  { ('-')          , ('[')       , ('\'')         , (0)              , (KEY_MENU)},
  { ('=')          , (']')       , (0)            , (KEY_RIGHT_SHIFT), (KEY_LEFT_ARROW)},
  { (KEY_BACKSPACE), ('\\')      , (KEY_RETURN)   , (KEY_UP_ARROW)   , (KEY_DOWN_ARROW)},
  { ('`')          , (KEY_DELETE), (KEY_PAGE_UP)  , (KEY_PAGE_DOWN)  , (KEY_RIGHT_ARROW)},
};

static const std::array<MyKeyboardKey, 12> functions =
{
  KEY_F1, KEY_F2, KEY_F3, KEY_F4,  KEY_F5,  KEY_F6,
  KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12
};

static const std::array<MyKeyboardKey, 10> special = {KEY_PRINTSCR};

static constexpr std::array<uint8_t, 16> gray_code =
{  0,  1,  3,  2,
   6,  7,  5,  4,
  12, 13, 15, 14,
  10, 11,  9,  8
};

static constexpr std::array ROW_IN{15, 16, 18, 19, 12};
static constexpr std::array MUX_OUT{5, 6, 10, 11};
static constexpr int FN_COL = 11;
static constexpr int FN_ROW = 4;

//------------------------------------------------------------------------------
// Local functions
//------------------------------------------------------------------------------

void setScanline(const char col)
{
  digitalWrite(MUX_OUT[0], ((col & 1) == 0) ? LOW : HIGH);
  digitalWrite(MUX_OUT[1], ((col & 2) == 0) ? LOW : HIGH);
  digitalWrite(MUX_OUT[2], ((col & 4) == 0) ? LOW : HIGH);
  digitalWrite(MUX_OUT[3], ((col & 8) == 0) ? LOW : HIGH);
}

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
    setScanline(FN_COL);

    bool fn = (digitalRead(ROW_IN[FN_ROW]) == HIGH);

    for (int i = 1; i < 16; i++)
    {
      const int m = gray_code[i]; // 1 - 15

      noInterrupts();

      setScanline(m);

      int j = m - 1;  // 0 - 14
      if (fn) // function key pressed
      {
        if (j >= 1 && j <= 12) //set fX keys
          functions[j - 1].set(digitalRead(ROW_IN[0]) == HIGH);

        //check macro keys
        if (digitalRead(ROW_IN[1]) == HIGH)
        {
          if (keyboardM[j][1].getKey() != 0)
            if (macroHandler.trigger(keyboardM[j][1].getKey()))
            {
              interrupts();
              chThdSleepMilliseconds(50);
              continue;
            }
        }

        //special keys
        if (keyboardM[j][4].getKey() != 0 && keyboardM[j][4].getKey() == KEY_MENU)
        {
          // print screen
          special[0].set(digitalRead(ROW_IN[4]) == HIGH);
          interrupts();
          chThdSleepMilliseconds(50);
          continue;
        }
      }
      else
      {
        // check first row
        if (keyboardM[j][0].getKey() != 0)
        {
          keyboardM[j][0].set(digitalRead(ROW_IN[0]) == HIGH);
        }
        // clear fX keys
        if (j >= 1 && j <= 12)
          functions[j - 1].set(false);
      }

      // check other rows
      for (int i = 1; i<=4; i++)
      {
        if (keyboardM[j][i].getKey() != 0)
          keyboardM[j][i].set(digitalRead(ROW_IN[i]) == HIGH);
      }

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

  for (auto &o : MUX_OUT)
  {
    pinMode(o, OUTPUT);
  }

  for (auto &in : ROW_IN)
  {
    pinMode(in, INPUT_PULLDOWN);
  }

  Keyboard.begin();

  delay(2000);
  Keyboard.print("ohayou!");
  

#if WIFI_EN
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

  // example macros
  macroHandler.addMacro<'p'>([] {
    Keyboard.print("macro0");
    return false;
  });
  macroHandler.addMacro<'q'>([] {
    Keyboard.print("macro1");
    return false;
  });

  // multi state macro
  macroHandler.addMacro<'w'>([cnt = 0]() mutable {
    Keyboard.print("macro2");
    Keyboard.write(KEY_TAB);
    if (cnt++ == 2)
    {
      cnt = 0;
      return false;
    }
    else
      return true;
  });
  
  macroHandler.addMacro<'e'>([] {
    Keyboard.print("macro3");
    return false;
  });
  macroHandler.addMacro<'r'>([] {
    Keyboard.print("macro4");
    return false;
  });
  macroHandler.addMacro<'t'>([] {
    Keyboard.print("macro5");
    return false;
  });

  // initialize Chibi OS
  chBegin(chSetup);
  // chBegin() resets stacks and should never return.
  while (true) {}
}
//------------------------------------------------------------------------------

// Runs at NORMALPRIO.
void loop() {
  // exec active macros
  macroHandler.execMacros();

  // sleep thread
  chThdSleepMilliseconds(200);
}


#if WIFI_EN
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
