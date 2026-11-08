# Deploii micro:bit extension: developer commands, included from the root Makefile. Run `make help`.

PXT      := npx pxt
HEX      := built/binary.hex

# Compile C++ on this machine with the native Arm toolchain (no Docker, no cloud)
LOCAL    := PXT_FORCE_LOCAL=1 PXT_NODOCKER=1

# Where the MICROBIT USB drive gets mounted (Ubuntu/Debian: /media, Fedora/Arch: /run/media)
MICROBIT_DRIVE ?= $(firstword $(wildcard /media/$(USER)/MICROBIT /run/media/$(USER)/MICROBIT /Volumes/MICROBIT))
SERIAL_PORT    ?= $(firstword $(wildcard /dev/serial/by-id/*micro*bit* /dev/serial/by-id/*BBC* /dev/ttyACM0 /dev/cu.usbmodem*))

.PHONY: help setup build rebuild cloud deploy flash monitor shims clean distclean check-node

help:
	@echo "make setup      install toolchain + dependencies (runs scripts/setup.sh)"
	@echo "make build      compile main.ts + test.ts + C++ to $(HEX)"
	@echo "make rebuild    force a full C++ rebuild (after editing CODAL or config)"
	@echo "make cloud      compile using the MakeCode cloud compiler instead of the local toolchain"
	@echo "make deploy     build, then copy the hex to the micro:bit drive"
	@echo "make flash      copy the existing hex to the micro:bit drive without building"
	@echo "make monitor    open the USB serial console (115200 baud, exit with Ctrl-A Ctrl-X)"
	@echo "make shims      regenerate shims.d.ts / enums.d.ts from the C++ //% comments"
	@echo "make clean      delete build output (keeps the downloaded CODAL sources)"
	@echo "make distclean  delete everything downloaded or built"

setup:
	./scripts/setup.sh

check-node:
	@node -e 'if (+process.versions.node.split(".")[0] < 24) { console.error("Node " + process.version + " is too old, need >= 24. Run: nvm use"); process.exit(1) }'
	@test -f node_modules/pxtcli.json || (echo "Dependencies missing, run: make setup" && exit 1)

build: check-node
	$(LOCAL) $(PXT) build --localbuild
	@echo "Built $(HEX)"

rebuild: check-node
	$(LOCAL) PXT_RUNTIME_DEV=1 $(PXT) build --localbuild --force
	@echo "Built $(HEX)"

cloud: check-node
	$(PXT) build --cloudbuild
	@echo "Built $(HEX)"

deploy: build flash

flash:
	@test -f $(HEX) || (echo "No $(HEX), run make build first" && exit 1)
	@test -n "$(MICROBIT_DRIVE)" || (echo "micro:bit drive not found. Plug it in, or run: make flash MICROBIT_DRIVE=/path/to/MICROBIT" && exit 1)
	cp $(HEX) "$(MICROBIT_DRIVE)/" && sync
	@echo "Flashed to $(MICROBIT_DRIVE). The micro:bit restarts by itself when the yellow LED stops flashing."

monitor:
	@test -n "$(SERIAL_PORT)" || (echo "No serial port found. Plug in the micro:bit or run: make monitor SERIAL_PORT=/dev/ttyACM0" && exit 1)
	picocom -b 115200 --imap lfcrlf "$(SERIAL_PORT)"

shims: check-node
	$(LOCAL) $(PXT) buildshims

clean:
	rm -rf built/binary.* built/codal/build

distclean:
	rm -rf built node_modules pxt_modules
