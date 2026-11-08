#!/usr/bin/env bash
#
# One-time setup of the Deploii micro:bit development environment.
#
#   ./scripts/setup.sh            install everything, then do a first build
#   ./scripts/setup.sh --no-build install everything, skip the first build
#
# Installs:
#   - Arm GCC, CMake, Ninja, Python, Git      (to compile CODAL C++ locally)
#   - Node.js 24 via nvm, if needed            (pxt-microbit 9.x requires Node >= 24)
#   - pxt CLI + pxt-microbit (local, in node_modules)
#   - MakeCode dependencies (core, bluetooth) into pxt_modules
#   - serial port access (dialout group) and a udev rule for WebUSB flashing
#
set -euo pipefail

cd "$(dirname "$0")/.."
PROJECT_DIR="$(pwd)"
NODE_MAJOR=24
NVM_VERSION=v0.40.3
DO_BUILD=1
[[ "${1:-}" == "--no-build" ]] && DO_BUILD=0

info() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33mWARNING: %s\033[0m\n' "$*"; }
die()  { printf '\033[1;31mERROR: %s\033[0m\n' "$*"; exit 1; }

SUDO=""
if [[ $EUID -ne 0 ]]; then SUDO="sudo"; fi

# ---------------------------------------------------------------------------
info "0/6 Creating Makefile and VS Code config"
# ---------------------------------------------------------------------------
if [[ ! -f Makefile ]]; then
    printf '# Developer commands live in scripts/dev.mk. Run `make help`.\ninclude scripts/dev.mk\n' > Makefile
    echo "Created Makefile"
fi
mkdir -p .vscode
for f in scripts/vscode/*.json; do
    if [[ ! -f ".vscode/$(basename "$f")" ]]; then
        cp "$f" .vscode/
        echo "Created .vscode/$(basename "$f")"
    fi
done

# ---------------------------------------------------------------------------
info "1/6 Installing system packages (compiler toolchain)"
# ---------------------------------------------------------------------------
if command -v apt-get >/dev/null; then
    $SUDO apt-get update
    $SUDO apt-get install -y \
        git curl cmake ninja-build build-essential \
        python3 python-is-python3 \
        gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
        picocom
elif command -v dnf >/dev/null; then
    $SUDO dnf install -y \
        git curl cmake ninja-build gcc gcc-c++ make \
        python3 python-unversioned-command \
        arm-none-eabi-gcc-cs arm-none-eabi-gcc-cs-c++ arm-none-eabi-binutils-cs arm-none-eabi-newlib \
        picocom
elif command -v pacman >/dev/null; then
    $SUDO pacman -S --needed --noconfirm \
        git curl cmake ninja base-devel python \
        arm-none-eabi-gcc arm-none-eabi-binutils arm-none-eabi-newlib \
        picocom
elif command -v brew >/dev/null; then
    brew install git cmake ninja python picocom
    brew install --cask gcc-arm-embedded
else
    die "No supported package manager found (apt, dnf, pacman, brew). Install git, cmake, ninja, python3 and the Arm GNU toolchain manually, then re-run."
fi

# The pxt CLI runs `python build.py`, so a `python` command must exist
if ! command -v python >/dev/null; then
    mkdir -p "$HOME/.local/bin"
    ln -sf "$(command -v python3)" "$HOME/.local/bin/python"
    export PATH="$HOME/.local/bin:$PATH"
    warn "Created ~/.local/bin/python -> python3. Make sure ~/.local/bin is on your PATH."
fi

# ---------------------------------------------------------------------------
info "2/6 Checking Node.js (need >= ${NODE_MAJOR})"
# ---------------------------------------------------------------------------
node_major() { node -p 'process.versions.node.split(".")[0]' 2>/dev/null || echo 0; }

export NVM_DIR="${NVM_DIR:-$HOME/.nvm}"
if [[ "$(node_major)" -lt ${NODE_MAJOR} ]]; then
    if [[ ! -s "$NVM_DIR/nvm.sh" ]]; then
        echo "Installing nvm ${NVM_VERSION} into $NVM_DIR"
        curl -fsSL "https://raw.githubusercontent.com/nvm-sh/nvm/${NVM_VERSION}/install.sh" | bash
    fi
    # shellcheck disable=SC1091
    set +u; . "$NVM_DIR/nvm.sh"; set -u
    nvm install ${NODE_MAJOR}
    nvm alias default ${NODE_MAJOR}
    nvm use ${NODE_MAJOR}
fi
echo "node $(node -v), npm $(npm -v)"

# ---------------------------------------------------------------------------
info "3/6 Installing pxt CLI and pxt-microbit (into ./node_modules)"
# ---------------------------------------------------------------------------
if ! npm install; then
    warn "npm install failed with the pinned pxt-microbit version, trying the latest release"
    npm install --save-dev pxt-microbit@latest
fi
test -f node_modules/pxtcli.json || die "node_modules/pxtcli.json missing (postinstall did not run)"

# ---------------------------------------------------------------------------
info "4/6 Installing MakeCode dependencies (core, bluetooth)"
# ---------------------------------------------------------------------------
npx pxt install

# ---------------------------------------------------------------------------
info "5/6 USB access for the micro:bit"
# ---------------------------------------------------------------------------
if [[ "$(uname)" == "Linux" ]]; then
    # Serial console (make monitor)
    if getent group dialout >/dev/null && ! id -nG "$USER" | grep -qw dialout; then
        $SUDO usermod -aG dialout "$USER"
        warn "Added $USER to the 'dialout' group. Log out and back in before using 'make monitor'."
    fi
    if getent group uucp >/dev/null && ! getent group dialout >/dev/null && ! id -nG "$USER" | grep -qw uucp; then
        $SUDO usermod -aG uucp "$USER"
        warn "Added $USER to the 'uucp' group. Log out and back in before using 'make monitor'."
    fi
    # WebUSB flashing from the MakeCode web editor in Chrome
    RULE=/etc/udev/rules.d/50-microbit.rules
    if [[ ! -f $RULE ]]; then
        echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="0d28", ATTR{idProduct}=="0204", MODE="0666"' | $SUDO tee $RULE >/dev/null
        $SUDO udevadm control --reload-rules || true
        $SUDO udevadm trigger || true
        echo "Installed $RULE"
    fi
fi

# ---------------------------------------------------------------------------
info "6/6 Toolchain check"
# ---------------------------------------------------------------------------
missing=0
for tool in git cmake python arm-none-eabi-gcc arm-none-eabi-g++ node npx; do
    if command -v $tool >/dev/null; then
        printf '  %-20s %s\n' "$tool" "$($tool --version 2>&1 | head -n1)"
    else
        printf '  %-20s MISSING\n' "$tool"; missing=1
    fi
done
if command -v ninja >/dev/null; then printf '  %-20s %s\n' ninja "$(ninja --version)"; else echo "  ninja                (not found, make will be used)"; fi
[[ $missing -eq 0 ]] || die "Some tools are missing, see above."

if [[ $DO_BUILD -eq 1 ]]; then
    info "First build (downloads and compiles CODAL, takes a few minutes the first time)"
    make -C "$PROJECT_DIR" build
fi

info "Done"
cat <<EOF

  make build     compile to built/binary.hex (local C++ toolchain)
  make deploy    build and copy the hex to the micro:bit
  make monitor   open the micro:bit's USB serial console
  make help      list all commands

If Node was installed by this script, open a new terminal (or run 'nvm use') first.
EOF
