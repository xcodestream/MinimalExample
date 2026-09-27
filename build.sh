#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# MinimalExample — сборочный скрипт для всех поддерживаемых платформ.
#
# Использование:
#   ./build.sh                # сборка под текущую ОС (Release) -> build/host
#   ./build.sh run            # собрать и запустить на текущей ОС
#   ./build.sh macos          # то же, явно (только на macOS)
#   ./build.sh linux          # Linux: X11 + GLX
#   ./build.sh windows        # Windows: MSVC на Windows, MinGW-w64 кросс на macOS/Linux
#   ./build.sh wasm           # Emscripten / WebGL 2 -> build/wasm/bin/MinimalExample.html
#   ./build.sh ios            # Xcode-проект iOS (arm64) -> build/ios
#   ./build.sh android        # NDK arm64-v8a (нужен ANDROID_NDK_HOME) -> build/android
#   ./build.sh all            # всё, что доступно на этой машине
#   ./build.sh deps           # проверить/подтянуть submodule crossrender
#   ./build.sh --clean        # пересборка с нуля
#   ./build.sh --debug        # Debug вместо Release
#   ./build.sh --jobs 8
#   ./build.sh --help
# ---------------------------------------------------------------------------
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="host"
SUBMODULE_DIR="crossrender"
SUBMODULE_URL="https://github.com/xcodestream/CrossRender"
BUILD_TYPE="Release"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
CLEAN=0
RUN_AFTER=0

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

host_platform() {
    case "$(uname -s)" in
        Darwin)               echo macos ;;
        Linux)                echo linux ;;
        MINGW*|MSYS*|CYGWIN*) echo windows ;;
        *)                    echo unknown ;;
    esac
}

# Проверяет наличие submodule с движком. Если каталог пуст или отсутствует —
# добавляет submodule (когда он ещё не зарегистрирован в .gitmodules) и
# рекурсивно подтягивает его содержимое.
ensure_submodule() {
    [[ -f "$ROOT/$SUBMODULE_DIR/CMakeLists.txt" ]] && return 0
    command -v git >/dev/null 2>&1 || \
        die "каталог $SUBMODULE_DIR пуст, а git не найден: установите git или клонируйте репозиторий с --recurse-submodules"

    if git -C "$ROOT" config --file .gitmodules --get "submodule.$SUBMODULE_DIR.path" >/dev/null 2>&1; then
        log "Submodule $SUBMODULE_DIR зарегистрирован, но не скачан — инициализирую"
    else
        log "Submodule $SUBMODULE_DIR отсутствует — добавляю ($SUBMODULE_URL)"
        git -C "$ROOT" submodule add "$SUBMODULE_URL" "$SUBMODULE_DIR" || \
            die "не удалось добавить submodule $SUBMODULE_DIR"
    fi
    log "Подтягиваю submodule рекурсивно"
    git -C "$ROOT" submodule update --init --recursive || \
        die "не удалось подтянуть submodule $SUBMODULE_DIR"
    [[ -f "$ROOT/$SUBMODULE_DIR/CMakeLists.txt" ]] || \
        die "submodule $SUBMODULE_DIR скачан, но $SUBMODULE_DIR/CMakeLists.txt не найден"
    log "Submodule $SUBMODULE_DIR готов"
}

usage() {
    cat <<EOF
MinimalExample — минимальный пример CrossRender (переливающийся алфавит)

Использование: ./build.sh [цель] [опции]

Цели:
  deps      проверить submodule crossrender; если его нет — добавить и рекурсивно подтянуть
  host      сборка под текущую ОС (по умолчанию)
  macos     macOS (Cocoa + OpenGL 3.3) — только на Mac
  linux     Linux (X11 + GLX)
  windows   Windows (MSVC на Windows / MinGW-w64 кросс-сборка)
  wasm      WebAssembly + WebGL 2 (нужен emsdk)
  ios       Xcode-проект для iOS (только на macOS)
  android   Android arm64-v8a (нужен ANDROID_NDK_HOME)
  all       все платформы, доступные на этой машине
  run       собрать host-версию и запустить

Опции:
  --debug     конфигурация Debug (по умолчанию Release)
  --release   конфигурация Release
  --clean     удалить каталог сборки цели перед сборкой
  --jobs N    число потоков сборки (по умолчанию — все ядра)
  -h, --help  эта справка
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        host|macos|linux|windows|wasm|ios|android|all|deps) TARGET="$1" ;;
        run)        RUN_AFTER=1 ;;
        --debug)    BUILD_TYPE=Debug ;;
        --release)  BUILD_TYPE=Release ;;
        --jobs)     shift; [[ -n "${1:-}" ]] || die "--jobs требует число"; JOBS="$1" ;;
        --clean)    CLEAN=1 ;;
        -h|--help)  usage; exit 0 ;;
        *)          die "неизвестная цель/опция: $1 (см. --help)" ;;
    esac
    shift
done

command -v cmake >/dev/null 2>&1 || die "cmake не найден. Установите: brew install cmake / apt install cmake"

# configure_and_build <каталог> [доп. аргументы cmake...]
configure_and_build() {
    local dir="$1"; shift
    [[ "$CLEAN" == "1" ]] && rm -rf "$dir"
    mkdir -p "$dir"
    if [[ $# -gt 0 ]]; then
        log "Конфигурация ($BUILD_TYPE): $*"
    else
        log "Конфигурация ($BUILD_TYPE)"
    fi
    cmake -S "$ROOT" -B "$dir" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" "$@" || die "конфигурация не удалась"
    log "Сборка (--parallel $JOBS)"
    cmake --build "$dir" --config "$BUILD_TYPE" --parallel "$JOBS" || die "сборка не удалась"
}

build_host() {
    local dir="$ROOT/build/host"
    case "$(host_platform)" in
        macos)
            configure_and_build "$dir"
            log "Готово: $dir/bin/MinimalExample"
            ;;
        linux)
            configure_and_build "$dir"
            log "Готово: $dir/bin/MinimalExample"
            ;;
        windows)
            configure_and_build "$dir"
            log "Готово: $dir/bin/MinimalExample.exe"
            ;;
        *) die "неизвестная хост-платформа" ;;
    esac
    if [[ "$RUN_AFTER" == "1" ]]; then
        local exe="$dir/bin/MinimalExample"
        [[ "$(host_platform)" == "windows" ]] && exe="$exe.exe"
        [[ -x "$exe" ]] || die "бинарник не найден: $exe"
        log "Запуск"
        "$exe"
    fi
}

build_wasm() {
    if ! command -v emcmake >/dev/null 2>&1; then
        if [[ -f "$HOME/emsdk/emsdk_env.sh" ]]; then
            # shellcheck disable=SC1091
            source "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1 || true
        fi
    fi
    command -v emcmake >/dev/null 2>&1 || die "emcmake не найден. Установите emsdk и выполните source emsdk_env.sh"

    local dir="$ROOT/build/wasm"
    configure_and_build "$dir"
    log "Готово: $dir/bin/MinimalExample.html"
    log "Запуск: (cd $dir/bin && python3 -m http.server 8080), затем откройте http://localhost:8080/MinimalExample.html"
}

build_ios() {
    [[ "$(uname -s)" == "Darwin" ]] || die "сборка iOS возможна только на macOS"
    local dir="$ROOT/build/ios"
    configure_and_build "$dir" \
        -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
    log "Готово: $dir/MinimalExample.xcodeproj"
    log "Откройте проект в Xcode, выберите команду разработки и запустите на устройстве/симуляторе"
}

build_android() {
    : "${ANDROID_NDK_HOME:=${ANDROID_NDK_ROOT:-}}"
    if [[ -z "${ANDROID_NDK_HOME}" ]]; then
        for candidate in "$HOME/Library/Android/sdk/ndk"/* "$HOME/Android/Sdk/ndk"/*; do
            [[ -d "$candidate" ]] && ANDROID_NDK_HOME="$candidate"
        done
    fi
    [[ -n "${ANDROID_NDK_HOME}" && -d "${ANDROID_NDK_HOME}" ]] || \
        die "Android NDK не найден. Укажите ANDROID_NDK_HOME."

    local dir="$ROOT/build/android"
    configure_and_build "$dir" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-24
    log "Готово: $dir/bin/MinimalExample (нужна обёртка в APK или запуск через adb из /data/local/tmp)"
}

build_windows_cross() {
    command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1 || \
        die "MinGW-w64 не найден (brew install mingw-w64). На Windows-хосте используйте ./build.sh windows"
    local dir="$ROOT/build/windows"
    configure_and_build "$dir" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
    log "Готово: $dir/bin/MinimalExample.exe"
}

case "$TARGET" in
    host)
        case "$(host_platform)" in
            macos|linux|windows) build_host ;;
            *) die "неподдерживаемая хост-платформа: $(uname -s)" ;;
        esac
        ;;
    macos)
        [[ "$(host_platform)" == "macos" ]] || die "цель macos доступна только на macOS"
        build_host
        ;;
    linux)   build_host ;;  # на Linux-хосте host и есть linux
    windows) build_host ;;  # на Windows-хосте host и есть windows (MSVC)
    wasm)    build_wasm ;;
    ios)     build_ios ;;
    android) build_android ;;
    deps)    ensure_submodule ;;  # только по явному запросу пользователя
    all)
        case "$(host_platform)" in
            macos)
                build_host
                build_wasm   || warn "WASM пропущен (нет emsdk)"
                build_ios    || warn "iOS пропущен"
                ;;
            linux)
                build_host
                build_wasm          || warn "WASM пропущен (нет emsdk)"
                build_windows_cross || warn "Windows пропущен (нет mingw-w64)"
                ;;
            windows)
                build_host
                ;;
        esac
        ;;
esac

log "Готово."
