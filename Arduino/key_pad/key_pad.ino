// 秋月電子4x3キーパッドと十字キーから、USBキー入力を生成する
// DCS World + X52PRO HOTAS 用
// Target Board: Arduino Pro Micro (Leonardo / ATmega32U4を選択)
//
#include <Keyboard.h>

// 配列の要素数を返す
template<typename T> constexpr int _countof(const T &x){ return sizeof(x)/sizeof(x[0]); }

//---------------------------------------------------------------
// 秋月電子4x3キーパッド
//---------------------------------------------------------------
// 制御線                    { X, Y, Z }
const uint8_t scan_port[] = { 8, 7, 6 };
// 入力線                    { A, B, C, D }
const uint8_t data_port[] = { 5, 4, 3, 2 };

// 初期化
void setupFuncKeys()
{
  for(int i = 0; i < _countof(scan_port); i++)
  {
    pinMode(scan_port[i], OUTPUT);
    digitalWrite(scan_port[i], HIGH);
  }
  for(int i = 0; i < _countof(data_port); i++)
  {
    pinMode(data_port[i], INPUT_PULLUP);
  }
}

// 全キースキャンして、押下されているキーのビットを1にしたビットマップを返す。
// numPressed [out] 押されているボタンの数
// MSB [ SW12(Z-A), SW11(Y-A), SW10(X-A),
//       SW9 (Z-B), SW8 (Y-B), SW7 (X-B), 
//       SW6 (Z-C), SW5 (Y-C), SW4 (X-C), 
//       SW3 (Z-D), SW2 (Y-D), SW1 (X-D) ] LSB
int funcKeyScan(int *num_pressed)
{
  *num_pressed = 0;
  int key_pressed = 0;
  for(int col = 0; col < _countof(scan_port); col++) // [X,Y,Z]
  {
    digitalWrite(scan_port[col], LOW);
    int row_data = 0;
    for(int row = 0; row < _countof(data_port); row++) // [A,B,C,D]
    {
      if(digitalRead(data_port[row]) == LOW)
      {
        row_data |= (0x0200 >> (3 * row));  // 0x0200:SW10(X-A)のbit
        (*num_pressed) += 1;
      }
    }
    key_pressed |= (row_data << col);
    digitalWrite(scan_port[col], HIGH);
  }
  return key_pressed;    
}

// 対応するキーコード
const uint8_t funcKeyCodes[] = { // LSB -> MSB
  KEY_F3,  KEY_F2,  KEY_F1,
  KEY_F6,  KEY_F5,  KEY_F4,
  KEY_F9,  KEY_F8,  KEY_F7,
  KEY_F12, KEY_F11, KEY_F10,
};
// 正面から見たキーの並び順に対応したビット
// 0x0800, 0x0400, 0x0200 / F10, F11, F12
// 0x0100, 0x0080, 0x0040 / F7,  F8,  F9
// 0x0020, 0x0010, 0x0008 / F4,  F5,  F6
// 0x0004, 0x0002, 0x0001 / F1,  F2,  F3

// 4x3キーパッドの処理
// 同時押しによる、別キーエイリアス対応版
void funcKeyProc()
{
  // 今のキー状態をスキャン
  int num_pressed;
  const int key_pressed = funcKeyScan(&num_pressed);

  // 同時押し検出
  // 二つのボタンを押すタイミングが微妙にずれでも、同時押しを検出できるように工夫。
  // 同時押しとした場合、個々のボタンが押されたことにならないように。
  // 最初の押し込み検出まで、数フレームのディレイを持たせることで実現する。
  // NOTE: 現状、ディレイより早くボタンを放すと検出されない。
  static int key_pressed_buf[3];
  pushToBuffer(key_pressed_buf, key_pressed);
  // 同時押しモード
  static enum ESimultaneousPress {
    Waiting,            // 最初の押し込み検出までのディレイ中(もしくは何も押されていない)
    NormalPress,        // 通常のボタン押し(同時押しではない)
    SimultaneousPress,  // 同時押し中
  } mode = Waiting;
 
  // 同時押し検出の処理
  const ESimultaneousPress last_mode = mode;
  if(mode == Waiting)
  {
    // 同時押しが検出されたら最優先
    if(2 <= num_pressed)
    {
      mode = SimultaneousPress;
    }
    // 3fにわたって同じボタンが押されていたら、通常のボタン押し処理に入る
    else if(isSamePressed(key_pressed_buf))
    {
      clearBuffer(key_pressed_buf);
      mode = NormalPress;
    }
  }

  // 通常のボタン押し処理
  if(mode == NormalPress)
  {
    // 押されたり放されたりしたキーのコードを送信
    const int change = key_pressed ^ key_pressed_buf[1];
    if (change)
    {
      int mask = 1;
      uint8_t code;
      for(int i = 0; i < _countof(funcKeyCodes); i++)
      {
        if (change & mask)
        {
          code = funcKeyCodes[i];
          if (key_pressed & mask)
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
    }
    // 全てのボタンが放されたらモードをリセット
    if(key_pressed == 0)
    {
      mode = Waiting;
    }
  }
  // 同時押し処理
  else if(mode == SimultaneousPress)
  {
    // 同時押しによるエイリアスキーの押し込み
    if(last_mode == Waiting)
    {
      uint8_t *codes;
      const int n = getAliasKey(key_pressed, &codes);
      for(int i = 0; i < n; i++)
      {
        Keyboard.press(codes[i]);
      }
    }
    // 全てのキーが放されたら、エイリアスキーを放してモードをリセット
    else if(key_pressed == 0)
    {
      Keyboard.releaseAll();
      mode = Waiting;
    }
  }
}

//---------------------------------------------------------------
// サイズ3のバッファにデータを追加する
void pushToBuffer(int *buf, const int val)
{
  buf[0] = buf[1];
  buf[1] = buf[2];
  buf[2] = val;
}

// 同じキーが3f押され続けたか？
bool isSamePressed(const int *buf)
{
  return (buf[0] != 0) && (buf[0] == buf[1]) && (buf[0] == buf[2]);
}

// バッファをリセット
void clearBuffer(int *buf)
{
  buf[0] = buf[1] = 0;
  // NOTE: buf[2] には最新のビットが入っているので、クリアしてはいけない。
}

//---------------------------------------------------------------
// 同時押しのエイリアスキー
// 正面から見たキーの並び順に対応したビット
// 0x0800, 0x0400, 0x0200 / F10, F11, F12
// 0x0100, 0x0080, 0x0040 / F7,  F8,  F9
// 0x0020, 0x0010, 0x0008 / F4,  F5,  F6
// 0x0004, 0x0002, 0x0001 / F1,  F2,  F3
struct AliasKey {
  int     bits;     // ボタン押しbit
  int     ret;      // キーコード数
  uint8_t code[4];  // キーコード
};
const AliasKey aliasKeys[] = {
  { 0x0800 | 0x0400, 1, { KEY_ESC } },              // DCS: ESC
  { 0x0400 | 0x0200, 1, { KEY_PAUSE } },            // DCS: Pause
  { 0x0800 | 0x0200, 3, { KEY_LEFT_SHIFT, KEY_LEFT_CTRL, KEY_PAUSE } }, // DCS: Active Pause
  { 0x0004 | 0x0002, 2, { KEY_LEFT_CTRL, 'z' } },   // DCS: Accelerate time speed
  { 0x0002 | 0x0001, 2, { KEY_LEFT_SHIFT, 'z' } },  // DCS: Reset time acceleration
};

// 同時押しに割り当てられたエイリアスを検索
int getAliasKey(const int pressed, const uint8_t **code)
{
  for(int i = 0; i < _countof(aliasKeys); i++)
  {
    const auto &alias = aliasKeys[i];
    if(alias.bits == pressed)
    {
      *code = alias.code;
      return alias.ret;
    }
  }
  // 該当なし
  return 0;
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
  delay(33);
}
