#!/bin/bash
set -euo pipefail

LOG_FILE="build.log"
exec > >(tee "$LOG_FILE") 2>&1

echo "=== build.sh started: $(date) ==="
echo "=== pwd: $(pwd) ==="
echo "=== docker: $(docker --version 2>&1) ==="
echo ""

run_step() {
    local label="$1"; shift
    echo "--- STEP: $label ---"
    if "$@"; then
        echo "--- OK: $label ---"
    else
        local rc=$?
        echo "--- FAILED: $label (exit $rc) ---"
        exit $rc
    fi
    echo ""
}

run_step "apply vendored libdragon patches (fgeom/rspq_profile backports for tiny3d)" \
    cp -r patches/libdragon/. libdragon/

run_step "generate fortunes table from assets/fortunes.md" \
    python3 tools/gen_fortunes.py assets/fortunes.md src/meta/fortunes_data.h

# The gameplay soundtrack ships as VADPCM wav64 streamed from ROM; the
# committed source is an mp3, decoded on the host (ffmpeg isn't in the
# libdragon container) to 22.05kHz stereo WAV for audioconv64.
run_step "decode soundtrack mp3 -> wav (host ffmpeg, cached)" \
    bash -c 'if [ ! -f assets/music/gameplay.wav ] || \
                [ assets/music/soundtrack.mp3 -nt assets/music/gameplay.wav ]; then
                 ffmpeg -y -v error -i assets/music/soundtrack.mp3 \
                        -ar 22050 -ac 2 assets/music/gameplay.wav
             else echo "gameplay.wav up to date"; fi'

# Boot splash sources are full-size PNGs and an mp3 jingle; downscale the
# logos to blit size and decode the jingle on the host (no ffmpeg in the
# libdragon container). mksprite/audioconv64 pick them up from gen/.
run_step "prepare splash assets (host ffmpeg, cached)" \
    bash -c '
        mkdir -p assets/splash/gen
        gen() {
            if [ ! -f "$2" ] || [ "$1" -nt "$2" ]; then
                ffmpeg -y -v error -i "$1" "${@:3}" "$2"
            else
                echo "$(basename "$2") up to date"
            fi
        }
        gen assets/splash/ld-logo.png assets/splash/gen/ld_logo.png \
            -vf "scale=320:180:flags=lanczos,format=rgba"
        gen assets/splash/Logo_Cydonis.png assets/splash/gen/cydonis.png \
            -vf "scale=320:173:flags=lanczos,format=rgba"
        gen assets/splash/cydonis-splash.mp3 assets/splash/gen/jingle.wav \
            -ar 22050 -ac 2
    '

run_step "docker image pull check" \
    docker image inspect ghcr.io/dragonminded/libdragon:latest --format '{{.Id}}'

run_step "libdragon install + tiny3d build + make" \
    docker run --rm \
        -v "$(pwd)":/project \
        -w /project \
        ghcr.io/dragonminded/libdragon:latest \
        sh -c '
            set -e
            echo "=== [container] PATH: $PATH ==="
            echo "=== [container] N64_INST: ${N64_INST:-unset} ==="
            echo ""

            echo "--- [container] STEP: libdragon install ---"
            make -C libdragon install tools-install
            echo "--- [container] OK: libdragon install ---"
            echo ""

            echo "--- [container] STEP: tiny3d build ---"
            make -C tiny3d CFLAGS="-Wno-error" CXXFLAGS="-Wno-error"
            echo "--- [container] OK: tiny3d build ---"
            echo ""

            echo "--- [container] STEP: project make ---"
            make "$@"
            echo "--- [container] OK: project make ---"
        ' -- "$@"

echo ""
echo "=== build.sh finished OK: $(date) ==="
echo "=== ROM: $(ls -lh voidstrider64.z64 2>/dev/null || echo 'not found') ==="
