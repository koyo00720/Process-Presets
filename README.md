# Process Presets

## 概要

**各 `.exe` が使用するコア（スレッド）をプリセット管理する軽量 GUI ソフト**です。  
ゲーム、解析ソフト、レンダラーなど、起動済みの Windows プロセスに対して「どの CPU スレッドを使わせるか」を表のチェックだけで設定し、必要なときに適用できます。

常時監視や自動での再適用は行いません。意図しない設定変更を避け、ユーザーが `Apply` を押したときだけ CPU の割り当てを変更します。

## 使い方

1. `ProcessPresets.exe` を起動します。
2. メニューの **Presets > New** を押し、対象の `.exe` 名を入力します。例: `example.exe`
3. 追加された行で、使わせたい CPU スレッドの列にチェックを入れます。
4. 対象のソフトを起動してから、その行の **Apply** を押します。
5. 同じ `.exe` が複数動いている場合は、表示された PID の一覧から適用先を1つ選びます。

入力やチェックの変更、プリセットの追加・削除はすぐに保存されます。次回起動時も同じ内容を利用できます。

## 画面の見方

- **Apply 列**: その行の CPU 設定を、現在実行中の対象 `.exe` に適用します。
- **.exe 列**: 対象プログラムのファイル名です。パスではなくファイル名を指定します。
- **CPU 列**: チェックしたスレッドだけを使用対象にします。上の見出しは CPU の階層です。

設定画面では、列幅や表の文字サイズも変更できます。

## メニュー

- **Presets**: プロセスごとの設定を追加・削除します。
- **CPU Structure**: 表の CPU 見出しを、実機に合わせて編集します。ラベル、階層数、順序を自由に設定できます。`Ctrl` と `Shift` による複数行選択も可能です。
- **Settings**: テーマ、表示言語、列幅、表の文字サイズを設定します。

初回起動時は Windows が検出した CPU 構造をもとに、見出しを自動作成します。すべてのコアが1スレッドなら `P1`、`E1` のような1階層表示になります。SMT を含む場合は `Core → Thread`、複数 CCX を検出した場合は `CCX → Core → Thread`（または `CCX → Core`）の形になります。

## 保存場所

設定、CPU構造、表の内容、各ウインドウのサイズは `ProcessPresets.conf` に保存されます。通常は実行ファイルと同じフォルダです。そのフォルダに書き込めない場合は、Windows のアプリ設定フォルダへ保存します。

CPU構造として「現在のPC（自動検出）」を選んでいる場合、この設定ファイルを別のPCで開くと、そのPCの CPU 構成に合わせて見出しを自動で再検出します。手動で作成した CPU 構造は変更されません。

## 注意点

- このソフトは Windows 専用です。
- 適用対象の `.exe` は、先に起動しておく必要があります。
- CPUスレッドを1つも選択していないプリセットは適用できません。
- 管理者として起動しているプロセスを変更するには、このソフトも同等以上の権限で起動してください。
- Windows の制約により、複数の Processor Group をまたぐ CPU 選択は適用できません。

## 開発者向け: ビルド

Qt 6 と MinGW が必要です。この PC の Qt MinGW キットでは、次の例で Debug / Release をビルドできます。

```powershell
$env:Path = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\mingw1310_64\x86_64-w64-mingw32\bin;C:\Qt\6.10.2\mingw_64\bin;$env:Path"
cmake -S . -B build/debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.10.2/mingw_64 `
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build/debug

cmake -S . -B build/release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.10.2/mingw_64 `
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build/release

& C:/Qt/6.10.2/mingw_64/bin/windeployqt.exe --release --compiler-runtime `
  --no-translations --force build/release/ProcessPresets.exe
```
