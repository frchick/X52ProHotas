// 秋月電子4x3キーパッドと十字キーから、USBキー入力を生成する
// DCS World + X52PRO HOTAS 用
// Target Board: Arduino Pro Micro (Leonardo / ATmega32U4を選択)
//
#include <Keyboard.h>

//---------------------------------------------------------------
// 秋月電子4x3キーパッド
//---------------------------------------------------------------
// 制御線                    { X, Y, Z }
const uint8_t scan_port[] = { 8, 7, 6 };
// 入力線                    { A, B, C, D }
const uint8_t data_port[] = { 5, 4, 3, 2 };

void setupFuncKeys()
{
  for(int i = 0; i < sizeof(scan_port); i++)
  {
    pinMode(scan_port[i], OUTPUT);
    digitalWrite(scan_port[i], HIGH);
  }
  for(int i = 0; i < sizeof(data_port); i++)
  {
    pinMode(data_port[i], INPUT_PULLUP);
  }
}

// 全キースキャンして、押下されているキーのビットを1にしたビットマップを返す。
// MSB [ SW12(Z-A), SW11(Y-A), SW10(X-A),
//       SW9 (Z-B), SW8 (Y-B), SW7 (X-B), 
//       SW6 (Z-C), SW5 (Y-C), SW4 (X-C), 
//       SW3 (Z-D), SW2 (Y-D), SW1 (X-D) ] LSB
int funcKeyScan()
{
  int key_pressed = 0;
  for(int col = 0; col < sizeof(scan_port); col++) // [X,Y,Z]
  {
    digitalWrite(scan_port[col], LOW);
    int row_data = 0;
    for(int row = 0; row < sizeof(data_port); row++) // [A,B,C,D]
    {
      row_data |= (digitalRead(data_port[row]) ? 0 : (0x0200 >> (3 * row)));
    }
    key_pressed |= (row_data << col);
    digitalWrite(scan_port[col], HIGH);
  }
  return key_pressed;    
}

// 対応するキーコード
const uint8_t funcKeyCodes[] = {
  KEY_F3,  KEY_F2,  KEY_F1,
  KEY_F6,  KEY_F5,  KEY_F4,
  KEY_F9,  KEY_F8,  KEY_F7,
  KEY_F12, KEY_F11, KEY_F10,
};

void funcKeyProc()
{
  static int last_key_pressed = 0;
  int key_pressed = funcKeyScan();
  int change = key_pressed ^ last_key_pressed;
  if (change)
  {
    int mask = 1;
    uint8_t code;
    for(int i = 0; i < sizeof(funcKeyCodes); i++) {
      if (change & mask) {
        code = funcKeyCodes[i];
        if (key_pressed & mask){
          Keyboard.press(code);
        }
        else
        {
          Keyboard.release(code);
        }
      }
      mask = mask << 1;          
    }
    last_key_pressed = key_pressed;
  }
}

//---------------------------------------------------------------
// 十字キー
//---------------------------------------------------------------
//                           { Up, Left, Down, Right, Center }
const uint8_t hatKeyPort[] = { 15, 14, 16, 10, 9 };
const uint8_t hatKeyCode[] = { ';', '/', '.', ',', KEY_RETURN };
const uint8_t numKeyPort = sizeof(hatKeyPort);

void setupHatKeys()
{
  for(uint8_t i = 0; i < numKeyPort; i++)
  {
    pinMode(hatKeyPort[i], INPUT_PULLUP);
  }
}

uint8_t hatKeyScan()
{
  uint8_t bits = 0;
  for(uint8_t i = 0; i < numKeyPort; i++)
  {
    if(digitalRead(hatKeyPort[i]) == LOW)
    {
      bits |= (0x01 << i);
    }
  }
  // Center押し込みの場合には方向キーを無効化
  if(bits & 0x10)
  {
    bits = 0x10;
  }
  return bits;
}

void hatKeyProc()
{
  static uint8_t lastKeyPress = 0;
  const uint8_t keyPress = hatKeyScan();
  const uint8_t keyChange = (lastKeyPress ^ keyPress);
  uint8_t mask = 0x01;
  for(uint8_t i = 0; i < numKeyPort; i++)
  {
    if(keyChange & mask)
    {
      const uint8_t code = hatKeyCode[i];
      if(keyPress & mask)
      {
        Keyboard.press(code);
      }
      else
      {
        Keyboard.release(code);
      }
    }
    mask = mask << 1;
  }
  lastKeyPress = keyPress;
}

//---------------------------------------------------------------
void setup()
{
  setupFuncKeys();
  setupHatKeys();
  Keyboard.begin();
  Keyboard.releaseAll();
}
 

void loop()
{
  funcKeyProc();
  hatKeyProc();
  delay(50);
}
