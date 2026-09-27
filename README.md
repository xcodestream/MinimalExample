# MinimalExample

Минимальный пример использования игрового движка [CrossRender](https://github.com/xcodestream/CrossRender) (подключён как submodule в `crossrender/`).

Приложение открывает окно и рисует латинский алфавит, по которому бежит волна яркости, а точка свечения следует за курсором мыши: буквы под курсором вспыхивают белым и плавно угасают к тёмно-серому вдали от него, свечение слегка пульсирует. До первого движения мыши (и в headless-режиме) точка свечения находится в центре окна. Шрифт — Ubuntu Bold (бесплатный, Ubuntu Font Licence 1.0), растеризуется собственным шрифтовым стеком движка в атлас 128 px.

Управление: **Esc / Q** — выход, **F1** — отладочный оверлей движка.

## Структура

```
CMakeLists.txt        корневая сборка: MinimalExample + crossrender как библиотека
build.sh              сборка под все поддерживаемые платформы
src/main.cpp          единственный исходник: сцена с радужным алфавитом
assets/fonts/         бесплатные шрифты (Ubuntu Regular/Bold + лицензия)
crossrender/          git submodule с движком
```

## Сборка и запуск

```bash
git submodule update --init --recursive   # подтянуть движок при первом клоне
./build.sh run                            # собрать под текущую ОС и запустить
```

Проверить или восстановить submodule `crossrender` можно отдельной целью (запускается только вручную, автоматически перед сборкой не вызывается):

```bash
./build.sh deps               # проверить submodule; если его нет — добавить и рекурсивно подтянуть
```

Скрипт поддерживает все платформы движка:

| Команда | Платформа | Результат |
|---|---|---|
| `./build.sh` | текущая ОС (macOS / Linux / Windows) | `build/host/bin/MinimalExample` |
| `./build.sh wasm` | WebAssembly + WebGL 2 (нужен emsdk) | `build/wasm/bin/MinimalExample.html` |
| `./build.sh ios` | Xcode-проект iOS (только macOS) | `build/ios/MinimalExample.xcodeproj` |
| `./build.sh android` | Android arm64-v8a (нужен `ANDROID_NDK_HOME`) | `build/android/bin/MinimalExample` |
| `./build.sh windows` | MSVC на Windows, кросс-сборка MinGW-w64 на macOS/Linux | `build/windows/bin/MinimalExample.exe` |
| `./build.sh all` | всё, что доступно на этой машине | — |

Дополнительные опции: `--debug`, `--clean`, `--jobs N`, `--help`. Работает и обычный CMake:

```bash
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
./build/host/bin/MinimalExample
```

Ассеты движок находит сам: он поднимается от каталога с бинарником (`build/bin`) и находит `<корень>/assets`.

## Headless-проверка

Без окна — для CI:

```bash
./build/host/bin/MinimalExample --headless --frames 120 --screenshot alphabet.png
```

PNG сохраняется в `<user root>/screenshots/` (путь печатается в лог).

## Как это работает

`src/main.cpp` — около 200 строк и весь API, нужный для старта:

* `EngineConfig` + `crossrender::RunExample` — конфигурация окна/подсистем и главный цикл;
* `AlphabetScene` — своя сцена (`Render2D`/`Update`), зарегистрированная в `SceneManager`;
* `ResourceCache::Font_` — загрузка TTF из `assets/fonts/` с фолбэком на встроенный шрифт движка;
* `Renderer2D::DrawText` — по одному вызову на букву; яркость складывается из медленной фоновой волны и гауссова свечения вокруг точки, следующей за `Input::MousePos()`;
* размер букв подгоняется под ширину окна, раскладка пересчитывается при ресайзе.
