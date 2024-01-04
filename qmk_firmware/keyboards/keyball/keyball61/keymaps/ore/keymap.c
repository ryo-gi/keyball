/*
Copyright 2022 @Yowkees
Copyright 2022 MURAOKA Taro (aka KoRoN, @kaoriya)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H
#include "quantum.h"

//20230908 ANSI (US)配列用のキーコードをJIS (JP)配列用キーコードに変換するQMK Firmware用ライブラリ 追加
#include "a2j/translate_ansi_to_jis.h"

// コード表
// 【KBC_RST: 0x5DA5】Keyball 設定のリセット
// 【KBC_SAVE: 0x5DA6】現在の Keyball 設定を EEPROM に保存します
// 【CPI_I100: 0x5DA7】CPI を 100 増加させます(最大:12000)
// 【CPI_D100: 0x5DA8】CPI を 100 減少させます(最小:100)
// 【CPI_I1K: 0x5DA9】CPI を 1000 増加させます(最大:12000)
// 【CPI_D1K: 0x5DAA】CPI を 1000 減少させます(最小:100)
// 【SCRL_TO: 0x5DAB】タップごとにスクロールモードの ON/OFF を切り替えます
// 【SCRL_MO: 0x5DAC】キーを押している間、スクロールモードになります
// 【SCRL_DVI: 0x5DAD】スクロール除数を１つ上げます(max D7 = 1/128)← 最もスクロール遅い
// 【SCRL_DVD: 0x5DAE】スクロール除数を１つ下げます(min D0 = 1/1)← 最もスクロール速い

////////////////////////////////////
///
/// 自動マウスレイヤーの実装 ここから
/// 参考にさせていただいたページ
/// https://zenn.dev/takashicompany/articles/69b87160cda4b9
///
////////////////////////////////////

enum custom_keycodes
{
  KC_MY_BTN1 = KEYBALL_SAFE_RANGE, // Remap上では 0x5DAF
  KC_MY_BTN2,                      // Remap上では 0x5DB0
  KC_MY_BTN3                       // Remap上では 0x5DB1
};

enum click_state
{
  NONE = 0,
  WAITING,   // マウスレイヤーが有効になるのを待つ。 Wait for mouse layer to activate.
  CLICKABLE, // マウスレイヤー有効になりクリック入力が取れる。 Mouse layer is enabled to take click input.
  CLICKING,  // クリック中。 Clicking.
};

enum click_state state; // 現在のクリック入力受付の状態 Current click input reception status
uint16_t click_timer;   // タイマー。状態に応じて時間で判定する。 Timer. Time to determine the state of the system.

uint16_t to_reset_time = 800; // この秒数(千分の一秒)、CLICKABLE状態ならクリックレイヤーが無効になる。 For this number of seconds (milliseconds), the click layer is disabled if in CLICKABLE state.

const int16_t to_clickable_movement = 0; // クリックレイヤーが有効になるしきい値
const uint16_t click_layer = 7;          // マウス入力が可能になった際に有効になるレイヤー。Layers enabled when mouse input is enabled

int16_t mouse_record_threshold = 30; // ポインターの動きを一時的に記録するフレーム数。 Number of frames in which the pointer movement is temporarily recorded.
int16_t mouse_move_count_ratio = 5;  // ポインターの動きを再生する際の移動フレームの係数。 The coefficient of the moving frame when replaying the pointer movement.

int16_t mouse_movement;

// クリック用のレイヤーを有効にする。　Enable layers for clicks
void enable_click_layer(void)
{
  layer_on(click_layer);
  click_timer = timer_read();
  state = CLICKABLE;
}

// クリック用のレイヤーを無効にする。 Disable layers for clicks.
void disable_click_layer(void)
{
  state = NONE;
  layer_off(click_layer);
}

// 自前の絶対数を返す関数。 Functions that return absolute numbers.
int16_t my_abs(int16_t num)
{
  if (num < 0)
  {
    num = -num;
  }

  return num;
}

// 自前の符号を返す関数。 Function to return the sign.
int16_t mmouse_move_y_sign(int16_t num)
{
  if (num < 0)
  {
    return -1;
  }

  return 1;
}

// 現在クリックが可能な状態か。 Is it currently clickable?
bool is_clickable_mode(void)
{
  return state == CLICKABLE || state == CLICKING;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record)
{

  switch (keycode)
  {
  case KC_MY_BTN1:
  case KC_MY_BTN2:
  case KC_MY_BTN3:
  {
    report_mouse_t currentReport = pointing_device_get_report();

    // どこのビットを対象にするか。 Which bits are to be targeted?
    uint8_t btn = 1 << (keycode - KC_MY_BTN1);

    if (record->event.pressed)
    {
      // ビットORは演算子の左辺と右辺の同じ位置にあるビットを比較して、両方のビットのどちらかが「1」の場合に「1」にします。
      // Bit OR compares bits in the same position on the left and right sides of the operator and sets them to "1" if either of both bits is "1".
      currentReport.buttons |= btn;
      state = CLICKING;
    }
    else
    {
      // ビットANDは演算子の左辺と右辺の同じ位置にあるビットを比較して、両方のビットが共に「1」の場合だけ「1」にします。
      // Bit AND compares the bits in the same position on the left and right sides of the operator and sets them to "1" only if both bits are "1" together.
      currentReport.buttons &= ~btn;
      enable_click_layer();
    }

    pointing_device_set_report(currentReport);
    pointing_device_send();
    return false;
  }

  default:
    if (record->event.pressed)
    {
      disable_click_layer();
    }
  }

//  return true; 　←20230908　本来はこのコードで終了
// jis 対応に変更するcommand
  return process_record_user_a2j(keycode, record);
  
}







report_mouse_t pointing_device_task_user(report_mouse_t mouse_report)
{
  int16_t current_x = mouse_report.x;
  int16_t current_y = mouse_report.y;

  if (current_x != 0 || current_y != 0)
  {

    switch (state)
    {
    case CLICKABLE:
      click_timer = timer_read();
      break;

    case CLICKING:
      break;

    case WAITING:
      mouse_movement += my_abs(current_x) + my_abs(current_y);

      if (mouse_movement >= to_clickable_movement)
      {
        mouse_movement = 0;
        enable_click_layer();
      }
      break;

    default:
      click_timer = timer_read();
      state = WAITING;
      mouse_movement = 0;
    }
  }
  else
  {
    switch (state)
    {
    case CLICKING:
      break;

    case CLICKABLE:
      if (timer_elapsed(click_timer) > to_reset_time)
      {
        disable_click_layer();
      }
      break;

    case WAITING:
      if (timer_elapsed(click_timer) > 50)
      {
        mouse_movement = 0;
        state = NONE;
      }
      break;

    default:
      mouse_movement = 0;
      state = NONE;
    }
  }

  mouse_report.x = current_x;
  mouse_report.y = current_y;

  return mouse_report;
}

////////////////////////////////////
///

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  // keymap for default
  [0] = LAYOUT_universal(
    KC_ESC     , KC_1     , KC_2        , KC_3     , KC_4           , KC_5          ,                                       KC_6         , KC_7     , KC_8     , KC_9     , KC_0           , _______  ,
    KC_TAB     , KC_Q     , KC_W        , KC_E     , KC_R           , KC_T          ,                                       KC_Y         , KC_U     , KC_I     , KC_O     , KC_P           , _______  ,
    KC_LCTL    , KC_A     , KC_S        , KC_D     , KC_F           , KC_G          ,                                       KC_H         , KC_J     , KC_K     , KC_L     , SFT_T(KC_SCLN), KC_RSFT  ,
    KC_LSFT    , KC_Z     , CTL_T(KC_X) , KC_C     , KC_V           , KC_B          , KC_F15        ,              KC_F16 , KC_N         , KC_M     , KC_COMM  , KC_DOT   , KC_SLSH        , KC_TAB   ,
    SGUI(KC_S) , KC_LWIN  , KC_LALT     , _______  , LT(1,KC_LNG2)  , LT(2,KC_BSPC) , LT(3,KC_LNG1) ,              KC_ENT , LT(1,KC_SPC) , _______  , _______  , _______  , TG(4)  , LT(3,KC_VOLD)
  ),
  //kigyk
  [1] = LAYOUT_universal(
    _______  , _______  , _______  , _______  , _______  , _______  ,                                   KC_F6    , KC_F7    , KC_F8    , KC_F9    , KC_F10   , KC_F11   ,
    _______  , KC_PLUS  , KC_AMPR  , KC_TILD  , KC_DLR   , KC_CIRC  ,                                   KC_EXLM  , KC_BSLS  , KC_SCLN  , KC_AT    , KC_GRV   , _______  ,
    _______  , KC_MINS  , KC_HASH  , KC_LBRC  , KC_LPRN  , KC_SLSH  ,                                  	KC_LCBR  , KC_QUOT  , KC_EQL   , KC_UNDS  , KC_COLN  , _______  ,
    _______  , KC_PERC  , KC_PIPE  , KC_RBRC  , KC_RPRN  , KC_ASTR  ,  _______  ,             _______ , KC_RCBR  , KC_DQT   , KC_LT    , KC_GT    , KC_QUES  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,  _______  ,             _______ , _______  , _______  , _______  , _______  , _______  , _______
  ),
  //数字_idou
  [2] = LAYOUT_universal(
    _______  , KC_F1    , KC_F2    , KC_F3    , KC_F4    , KC_F5    ,                                  KC_F6    , KC_F7    , KC_F8    , KC_F9    , KC_F10   , KC_F11   ,
    _______  , KC_PLUS  , KC_7     , KC_8     , KC_9     , KC_CIRC  ,                                  _______  , _______  , KC_UP    , KC_TAB   , KC_RALT  , KC_F12   ,
    _______  , KC_MINS  , KC_4     , KC_5     , KC_6     , KC_SLSH  ,                                  KC_HOME  , KC_LEFT  , KC_DOWN  , KC_RGHT  , KC_END   , _______  ,
    _______  , KC_0     , KC_1     , KC_2     , KC_3     , KC_ASTR  , _______  ,             _______ , KC_PGDN  , KC_BTN1  , KC_BTN3  , KC_BTN2  , KC_PGUP  , _______  ,
    _______  , LCTL(KC_LWIN)  , _______  , _______  , _______  , _______  , _______  ,       _______ , _______  , _______  , _______  , _______  , _______  , _______
  ),

  [3] = LAYOUT_universal(
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______   , _______  , _______  , _______  , _______  ,
    _______  , _______  , KC_MSTP  , KC_BRIU  , KC_VOLU  , _______  ,                                  _______  , CPI_D1K   , CPI_I1K  , CPI_D100 , CPI_I100 , _______  ,
    _______  , _______  , KC_MPLY  , KC_BRID  , KC_VOLD  , _______  ,                                  _______  , KC_BTN1   , _______  , KC_BTN2  , KBC_SAVE , KBC_RST  ,
    _______  , _______  , _______  , _______  , KC_MUTE  , _______  , _______  ,            _______  , _______  , A(KC_TAB) , _______  , _______  , _______  , _______  ,
    _______  , _______  , SCRL_DVD , SCRL_DVI , SCRL_MO  , SCRL_TO  , _______  ,            _______  , KC_BSPC  , _______   , _______  , _______  , _______  , _______
  ),

  //ワカサギ配列
  [4] = LAYOUT_universal( 
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , KC_Q     , KC_P     , KC_R     , KC_D     , KC_C     ,                                  KC_B     , KC_K     , KC_U     , KC_Y     , KC_X     , _______  ,
    _______  , KC_A     , KC_T     , KC_N     , KC_S     , KC_W     ,                                  KC_M     , KC_H     , KC_E     , KC_I     , KC_O     , _______  ,
    _______  , KC_SLSH  , KC_COMM  , KC_L     , KC_G     , KC_J     , _______  ,            _______  , KC_F     , KC_V     , KC_MIN  , KC_Z     , KC_DOT   , _______  ,
    _______  , _______  , _______  , _______ , LT(1,KC_LNG2)  , LT(2,KC_BSPC) , LT(3,KC_LNG1) ,              KC_ENT , LT(1,KC_SPC)  , _______  , _______  , _______  , KC_TRNS  , _______
  ),

  [5] = LAYOUT_universal(
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______  , _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______  , _______  , _______  , _______  , _______  , _______  , _______
  ),

  [6] = LAYOUT_universal(
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______  , _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______  , _______  , _______  , _______  , _______  , _______  , _______
  ),
  
  [7] = LAYOUT_universal(
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  , _______  ,  _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  ,                                  _______  ,KC_MY_BTN1, _______  ,KC_MY_BTN2, _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______  , _______  , _______  , _______  , _______  , _______  , _______  ,
    _______  , _______  , _______  , _______  , _______  , _______  , _______  ,            _______  , _______  , _______  , _______  , _______  , _______  , _______
  )

};
// clang-format on

layer_state_t layer_state_set_user(layer_state_t state)
{
  // レイヤーが1または3の場合、スクロールモードが有効になる
  keyball_set_scroll_mode(get_highest_layer(state) == 3);
  // keyball_set_scroll_mode(get_highest_layer(state) == 1 || get_highest_layer(state) == 3);

  // レイヤーとLEDを連動させる
  /* 20230904 私はLED不要のためomit
  uint8_t layer = biton32(state);
  switch (layer)
  {
  case 4:
    rgblight_sethsv(HSV_WHITE);
    break;

  default:
    rgblight_sethsv(HSV_OFF);
  }
  */
  return state;
}

#ifdef OLED_ENABLE

#include "lib/oledkit/oledkit.h"

void oledkit_render_info_user(void)
{
  keyball_oled_render_keyinfo();
  keyball_oled_render_ballinfo();

  oled_write_P(PSTR("Layer:"), false);
  oled_write(get_u8_str(get_highest_layer(layer_state), ' '), false);
}
#endif

  
/* us(ansi)
 * ,------------------------------------------------------------------------------------.
 * |      |   +  |   &  |   ~  |   $  |   ^  ||   !  |   \  |   ;  |   @  |   `  |      |
 * |------+------+------+------+------+------++------+------+------+------+------+------|
 * |      |   -  |   #  |   [  |   (  |   /  ||   {  |   '  |   =  |   _  |   :  |      |
 * |------+------+------+------+------+------++------+------+------+------+------+------|
 * |      |   %  |   |  |   ]  |   )  |   *  ||   }  |   "  |   <  |   >  |   ?  |      |
 * |------+------+------+------+------+------++------+------+------+------+------+------|
 * `------------------------------------------------------------------------------------'
 *
 *  [1] = LAYOUT_universal(
 *    _______  , _______  , _______  , _______  , _______  , _______  ,                                   KC_F6    , KC_F7    , KC_F8    , KC_F9    , KC_F10   , KC_F11   ,
 *    _______  , KC_PLUS  , KC_AMPR  , KC_TILD  , KC_DLR   , KC_CIRC  ,                                   KC_EXLM  , KC_BSLS  , KC_SCLN  , KC_AT    , KC_GRV   , _______  ,
 *    _______  , KC_CIRC  , KC_HASH  , KC_LBRC  , KC_LPRN  , KC_SLSH  ,                                  	KC_LCBR  , KC_QUOT  , KC_EQL   , KC_UNDS  , KC_COLN  , _______  ,
 *    _______  , KC_PERC  , KC_PIPE  , KC_RBRC  , KC_RPRN  , KC_ASTR  ,  _______  ,             _______ , KC_RCBR  , KC_DQT   , KC_LT    , KC_GT    , KC_QUES  , _______  ,
 *    _______  , _______  , _______  , _______  , _______  , _______  ,  _______  ,             _______ , _______  , _______  , _______  , _______  , _______  , _______
 *  ),
 *
 */

/* us(ansi) to jis : -が^に変化しただけ
 * よって差分のみ修正する。
 * 20230908
 * ,------------------------------------------------------------------------------------.
 * |      |   +  |   &  |   ~  |   $  |   ^  ||   !  |   \  |   ;  |   @  |   `  |      |
 * |------+------+------+------+------+------++------+------+------+------+------+------|
 * |      | ^→-  |   #  |   [  |   (  |   /  ||   {  |   '  |   =  |   _  |   :  |      |
 * |------+------+------+------+------+------++------+------+------+------+------+------| 
 * |      |   %  |   |  |   ]  |   )  |   *  ||   }  |   "  |   <  |   >  |   ?  |      |
 * |------+------+------+------+------+------++------+------+------+------+------+------|
 * `------------------------------------------------------------------------------------'
 */