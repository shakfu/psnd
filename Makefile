.DEFAULT_GOAL := all
.PHONY: all build clean reset test show-config rebuild remake docs library psnd \
		configure-tsf configure-tsf-csound configure-fluid configure-fluid-csound \
		configure-tsf-web configure-fluid-web configure-fluid-csound-web \
		configure-minihost configure-minihost-csound \
		configure-mhs-small configure-mhs-src configure-mhs-src-small configure-no-mhs \
		psnd-tsf default psnd-tsf-csound csound psnd-fluid psnd-fluid-csound \
		psnd-tsf-web web psnd-fluid-web psnd-fluid-csound-web full \
		psnd-minihost minihost psnd-minihost-csound \
		mhs-small mhs-src mhs-src-small no-mhs \
		test-tsf test-csound test-fluid test-fluid-csound \
		test-web test-fluid-web test-full test-minihost test-minihost-csound

BUILD_DIR ?= build
CMAKE ?= cmake

all: build

# ============================================================================
# Configure targets
# ============================================================================

# CMake caches options, so a configure that simply omits -DBUILD_X=ON inherits
# whatever the previous variant set. Every variant therefore states the full
# option set explicitly, and switching variants in place is safe.
PSND_ALL_OPTS = -DBUILD_CSOUND_BACKEND=OFF -DBUILD_FLUID_BACKEND=OFF \
                -DBUILD_WEB_HOST=OFF -DBUILD_MINIHOST_BACKEND=OFF
PSND_CONFIGURE = $(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_TESTING=ON $(PSND_ALL_OPTS)

configure-tsf:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE)

configure-tsf-csound:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DBUILD_CSOUND_BACKEND=ON

configure-fluid:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DBUILD_FLUID_BACKEND=ON

configure-fluid-csound:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DBUILD_FLUID_BACKEND=ON -DBUILD_CSOUND_BACKEND=ON

configure-tsf-web:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DBUILD_WEB_HOST=ON

configure-fluid-web:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DBUILD_FLUID_BACKEND=ON -DBUILD_WEB_HOST=ON

configure-fluid-csound-web:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DBUILD_FLUID_BACKEND=ON -DBUILD_CSOUND_BACKEND=ON -DBUILD_WEB_HOST=ON

# Minihost (VST/AU plugin) variants
# Note: JUCE is fetched automatically during configure (first build takes longer)
configure-minihost:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DBUILD_MINIHOST_BACKEND=ON

configure-minihost-csound:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DBUILD_MINIHOST_BACKEND=ON -DBUILD_CSOUND_BACKEND=ON

# MHS variants
configure-mhs-small:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DMHS_ENABLE_COMPILATION=OFF

configure-mhs-src:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DMHS_EMBED_MODE=SRC_ZSTD

configure-mhs-src-small:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DMHS_EMBED_MODE=SRC_ZSTD -DMHS_ENABLE_COMPILATION=OFF

configure-no-mhs:
	@mkdir -p $(BUILD_DIR) && $(PSND_CONFIGURE) -DENABLE_MHS_INTEGRATION=OFF

# ============================================================================
# Build presets
# ============================================================================

# TinySoundFont only
psnd-tsf: configure-tsf
	@$(CMAKE) --build $(BUILD_DIR) --config Release

default: psnd-tsf  # alias
build: psnd-tsf    # alias

# TinySoundFont + Csound
psnd-tsf-csound: configure-tsf-csound
	@$(CMAKE) --build $(BUILD_DIR) --config Release

csound: psnd-tsf-csound  # alias

# FluidSynth only
psnd-fluid: configure-fluid
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# FluidSynth + Csound
psnd-fluid-csound: configure-fluid-csound
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# TinySoundFont + Web
psnd-tsf-web: configure-tsf-web
	@$(CMAKE) --build $(BUILD_DIR) --config Release

web: psnd-tsf-web  # alias

# FluidSynth + Web
psnd-fluid-web: configure-fluid-web
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# FluidSynth + Csound + Web (everything)
psnd-fluid-csound-web: configure-fluid-csound-web
	@$(CMAKE) --build $(BUILD_DIR) --config Release

full: psnd-fluid-csound-web  # alias

# ============================================================================
# Minihost (VST/AU plugin) build variants
# ============================================================================

# TinySoundFont + Minihost
psnd-minihost: configure-minihost
	@$(CMAKE) --build $(BUILD_DIR) --config Release

minihost: psnd-minihost  # alias

# TinySoundFont + Minihost + Csound
psnd-minihost-csound: configure-minihost-csound
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# ============================================================================
# MHS build variants
# ============================================================================

# MHS without compilation support (~4.5MB binary, no -o executable output)
mhs-small: configure-mhs-small
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# MHS with source embedding (~17s startup, ~4MB binary with compilation)
mhs-src: configure-mhs-src
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# MHS source mode without compilation (smallest MHS binary, ~17s startup)
mhs-src-small: configure-mhs-src-small
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# Disable MHS entirely
no-mhs: configure-no-mhs
	@$(CMAKE) --build $(BUILD_DIR) --config Release

# ============================================================================

rebuild: clean psnd-tsf-csound test

library: configure-tsf
	@$(CMAKE) --build $(BUILD_DIR) --target libloki --config Release

# Primary target: unified psnd binary
psnd: configure-tsf
	@$(CMAKE) --build $(BUILD_DIR) --target psnd_bin --config Release

show-config: configure-tsf
	@$(CMAKE) --build $(BUILD_DIR) --target show-config --config Release

# Per-test ceiling shared with scripts/build_release.py. Tests carrying their
# own TIMEOUT property keep it. Without this a test that blocks on stdin hangs
# the run until the caller's own timeout, with no indication of which test.
PSND_CTEST = $(CMAKE) -E chdir $(BUILD_DIR) ctest -C Release --output-on-failure --timeout 120

# Generic test - builds current configuration and runs tests
test:
	@$(CMAKE) --build $(BUILD_DIR) --config Release
	@$(PSND_CTEST)

# Configuration-specific test targets
test-tsf: psnd-tsf
	@$(PSND_CTEST)

test-csound: psnd-tsf-csound
	@$(PSND_CTEST)

test-fluid: psnd-fluid
	@$(PSND_CTEST)

test-fluid-csound: psnd-fluid-csound
	@$(PSND_CTEST)

test-web: psnd-tsf-web
	@$(PSND_CTEST)

test-fluid-web: psnd-fluid-web
	@$(PSND_CTEST)

test-full: psnd-fluid-csound-web
	@$(PSND_CTEST)

test-minihost: psnd-minihost
	@$(PSND_CTEST)

test-minihost-csound: psnd-minihost-csound
	@$(PSND_CTEST)

clean:
	@$(CMAKE) --build $(BUILD_DIR) --target clean 2>/dev/null || true

reset:
	@$(CMAKE) -E rm -rf $(BUILD_DIR)

remake: reset build

# Generate architecture diagrams from D2 sources
docs:
	@command -v d2 >/dev/null 2>&1 || { echo "d2 not found. Install from https://d2lang.com"; exit 1; }
	@echo "Generating architecture diagrams..."
	d2 docs/arch-highlevel.d2 docs/arch-highlevel.svg
