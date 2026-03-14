# ProjectionBall Firmware Design

## 1. 目的と全体像

ProjectionBall は RP2040 (Raspberry Pi Pico) 上で動作する、2軸ミラー駆動 + レーザー描画のファームウェアである。
本ソフトは以下を同時に実現する。

- 高速モータ制御ループ (約80us周期)
- 低速パターン生成/ユーザインタフェース処理 (約320ms周期)
- UARTコマンドによる運用・調整
- RTCとFlashを使った設定保持

アーキテクチャはマルチコア分離構成。

- Core1: 制御系 (リアルタイム優先)
- Core0: パス生成/UI系 (機能優先)

## 2. ビルド構成

ビルドは CMake + Pico SDK を使用。
対象は `ProjectionBall` 実行ファイルで、主なリンク先は以下。

- `pico_stdlib`, `pico_multicore`
- `hardware_spi`, `hardware_i2c`, `hardware_timer`
- `hardware_pwm`, `hardware_flash`, `hardware_rtc`, `hardware_sync`, `hardware_watchdog`

ソースは `ProjectionBall.c` をエントリとして、制御/パス/RTC/Flash/コンソール関連モジュールをリンクする。

## 3. モジュール責務

### 3.1 メイン

- `ProjectionBall.c`
  - GPIO/SPI/I2C/UART/PWM 初期化
  - Core0/Core1 の起動と周期タイマ設定
  - 起動時のデータ復元と状態表示

- `ProjectionBall.h`
  - ピン定義
  - 各種機能フラグ (`USE_MA732`, `USE_RTC_SD30XX` など)

### 3.2 モータ制御

- `motor_ctrl.c/.h`
  - エンコーダ取得
  - 8サンプル平均で位置応答算出
  - P + D (+I) 制御でトルク算出
  - トルク制限/暴走検出
  - PWM出力とモータ方向制御
  - レーザー ON/OFF 出力

公開API例:

- `MotorCtrlInit()`
- `MotorCtrlLoop()`
- `SetPause(bool)`
- `SetGain(...)`, `GetGain(...)`
- `MotorCtrSetCenterPos(...)`
- `SetProjectionAngle(...)`, `GetProjectionAngle(...)`

### 3.3 パス制御

- `path_ctrl.c/.h`
  - 表示モード管理
  - 図形/文字/日時パスの進行管理
  - ボタン入力によるモード/パターン切替
  - 一時停止タイマ判定
  - 設定の保存/復元

モード:

- `MODE_PATTERN_ALWAYS_ON`
- `MODE_PATTERN_ONE_STROKE`
- `MODE_PATTERN_ROTATION`
- `MODE_WATCH`
- `MODE_DATE`
- `MODE_MSG`

パターン:

- STAR, ARROW, MAIL, SMILE, SUN, CLOUD, RAIN, SNOW, THUNDER, HEART

### 3.4 図形/文字生成

- `path_const.c/.h`
  - 三角関数 LUT (`TRIG_FUNCTION_LEN=120`)
  - 各図形の座標パス

- `path_font.c/.h`
  - 数字/英字/記号の文字パス
  - 文字列オフセット・遷移フラグ

### 3.5 エンコーダ

- `encoder_ma732.c/.h` (既定)
- `encoder_as5048a.c/.h` (代替)

SPIフォーマットはセンサ種別で切替。

### 3.6 RTC

- `rtc_sd30XX.c/.h` (既定)
- `rtc_rv8803.c/.h` (代替)

主用途:

- 日時の読み書き
- RAMレジスタを使ったモード/パターン/角度などの保持

### 3.7 Flash

- `flash_ctrl.c/.h`
  - ブロック30: ユーザ設定
  - ブロック31: キャリブレーション
  - 消去/書込時は割込み禁止で実行

### 3.8 コンソール

- `console.c/.h`
  - UART受信IRQ (`OnUartRx`)
  - コマンド解釈 (`ConsoleGetString`)
  - 日時/モード/パターン/角度/ゲイン/文字列/保存/再起動系コマンド

## 4. 起動シーケンス

`main()` の代表フロー:

1. `stdio_init_all()`
2. `ioInit()`
3. `MotorCtrlInit()`
4. RTC情報取得/表示
5. `UpdateHwRtc()`
6. `RestoreUserData()`
7. `multicore_launch_core1(core1_main)`
8. `core0_main()` に入る

## 5. 実行時スケジューリング

### 5.1 Core1 (制御コア)

- `core1_main()`
- 80us周期タイマで `CtrlEventFlg` をセット
- メインループでフラグ検知時に:
  - `MotorCtrlLoop()`
  - `UpdateUserButton()`
  - watchdog更新 (条件付き)

### 5.2 Core0 (機能コア)

- `core0_main()`
- 320ms周期タイマで `PathEventFlg` をセット
- メインループでフラグ検知時に:
  - `PathCtrlLoop()`
  - `ConsoleGetString()`
  - `DebugMotorCtrl()` (有効時)

## 6. 制御データフロー

1. `GetPathCmd()` が目標軌道 (`p_cmd`) とレーザー状態を生成
2. `MotorCtrlLoop()` がエンコーダ応答 (`x_res`) を取得
3. 誤差 (`x_err`) と速度誤差 (`dx_err`) を計算
4. `trq_out = kp*x_err + kd*dx_err + x_sum/iki` (I有効時)
5. トルク制限・飽和後にPWM/GPIOへ反映

補足:

- `ENABLE_PROJECTION_ANGLE` 有効時は座標回転を適用
- `ENABLE_IN_POS` 有効時は位置誤差が範囲内のときのみステップ進行を加速

## 7. 永続化設計

保持対象:

- ユーザ文字列
- pause/resume 時刻
- センタオフセット
- モード/パターン/投影角

保存先:

- Flash: ユーザ文字列、センタ、時刻設定
- RTC RAM: モード、パターン、タイマ有効、投影角インデックス

`SaveUserData()` では Core1 をリセットしてから Flash 書込みを行う設計。

## 8. 主要フラグ設計

既定で有効:

- `USE_MA732`
- `USE_RTC_SD30XX`
- `ENABLE_RUNAWAY_DETECTION`
- `ENABLE_IN_POS`
- `ENABLE_DEBUG_OUTPUT`
- `ENABLE_PROJECTION_ANGLE`
- `ENABLE_I`

主なデバッグ/試験フラグ (通常無効):

- `ENABLE_STEP_CMD`
- `ENABLE_SIN_CMD`
- `ENABLE_ENCODER_CHECK_MODE`
- `ENABLE_FLASH_TEST`
- `ENABLE_CALIBRATION_MODE`

## 9. コマンド体系 (概要)

日時:

- `tim=`, `tim?`, `day=`, `day?`, `wek=`, `wek?`

表示/制御:

- `mod=`, `mod?`, `ptn=`, `ptn?`, `deg=`, `deg?`
- `pus!`, `rsm!`, `rst!`

設定:

- `str=`, `str?`, `stg=`, `stg?`
- `cen=`, `cen?`
- `pus=`, `pus?`, `rsm=`, `rsm?`, `ten!`, `tds!`, `tst?`
- `gai=`, `gai?`

## 10. 設計上の注意点

- Core間で共有される状態に明示的ロックはほぼ無い。
  - 現状は「単一書き込み側 + 読み取り側」の前提で成立。
- Flash書込みは割込み禁止区間を含むため、実行タイミングに注意。
- `OnUartRx()` は受信長チェックが薄く、入力過大時の境界管理は改善余地あり。
- 制御ループは高頻度のため、重い処理を Core1 に入れないこと。
- `control_timer_callback` はイベント取りこぼし時にSTBYを落とす安全設計。

## 11. 保守・拡張指針

- 新しい表示パターン追加:
  - `path_const` に座標列API追加
  - `path_ctrl` のパターン分岐を拡張

- 新しいコマンド追加:
  - `console.c` にパーサを追加
  - 必要なら `path_ctrl` または `motor_ctrl` APIを拡張

- 新しいセンサ/RTC追加:
  - 既存の `USE_*` 切替方式に合わせてモジュールを追加

- 制御性能改善:
  - エンコーダ取得時間と80us周期の余裕を測定
  - I項とトルク制限定数の調整を定量化

---

この文書は、現行実装を読むための設計ガイドであり、実装変更時は関連章を同時更新すること。
