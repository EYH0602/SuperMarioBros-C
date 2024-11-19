#include <cstdio>
#include <iostream>

#include <SDL2/SDL.h>

#include "Emulation/Controller.hpp"
#include "SMB/SMBEngine.hpp"
#include "Util/Video.hpp"

#include "Configuration.hpp"
#include "Constants.hpp"

uint8_t *romImage;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Texture *scanlineTexture;
static SMBEngine *smbEngine = nullptr;
static uint32_t renderBuffer[RENDER_WIDTH * RENDER_HEIGHT];

static int level = 0;

/**
 * Load the Super Mario Bros. ROM image.
 */
static bool loadRomImage() {
  FILE *file = fopen(Configuration::getRomFileName().c_str(), "r");
  if (file == NULL) {
    std::cout << "Failed to open the file \"" << Configuration::getRomFileName()
              << "\". Exiting.\n";
    return false;
  }

  // Find the size of the file
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  fseek(file, 0L, SEEK_SET);

  // Read the entire file into a buffer
  romImage = new uint8_t[fileSize];
  fread(romImage, sizeof(uint8_t), fileSize, file);
  fclose(file);

  return true;
}

/**
 * SDL Audio callback function.
 */
static void audioCallback(void *userdata, uint8_t *buffer, int len) {
  if (smbEngine != nullptr) {
    smbEngine->audioCallback(buffer, len);
  }
}

/**
 * Initialize libraries for use.
 */
static bool initialize() {
  // Load the configuration
  //
  Configuration::initialize(CONFIG_FILE_NAME);

  // Load the SMB ROM image
  if (!loadRomImage()) {
    return false;
  }

  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    std::cout << "SDL_Init() failed during initialize(): " << SDL_GetError()
              << std::endl;
    return false;
  }

  // Create the window
  window = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            RENDER_WIDTH * Configuration::getRenderScale(),
                            RENDER_HEIGHT * Configuration::getRenderScale(), 0);
  if (window == nullptr) {
    std::cout << "SDL_CreateWindow() failed during initialize(): "
              << SDL_GetError() << std::endl;
    return false;
  }

  // Setup the renderer and texture buffer
  renderer = SDL_CreateRenderer(
      window, -1,
      (Configuration::getVsyncEnabled() ? SDL_RENDERER_PRESENTVSYNC : 0) |
          SDL_RENDERER_ACCELERATED);
  if (renderer == nullptr) {
    std::cout << "SDL_CreateRenderer() failed during initialize(): "
              << SDL_GetError() << std::endl;
    return false;
  }

  if (SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT) < 0) {
    std::cout << "SDL_RenderSetLogicalSize() failed during initialize(): "
              << SDL_GetError() << std::endl;
    return false;
  }

  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH,
                              RENDER_HEIGHT);
  if (texture == nullptr) {
    std::cout << "SDL_CreateTexture() failed during initialize(): "
              << SDL_GetError() << std::endl;
    return false;
  }

  if (Configuration::getScanlinesEnabled()) {
    scanlineTexture = generateScanlineTexture(renderer);
  }

  // Set up custom palette, if configured
  //
  if (!Configuration::getPaletteFileName().empty()) {
    const uint32_t *palette = loadPalette(Configuration::getPaletteFileName());
    if (palette) {
      paletteRGB = palette;
    }
  }

  if (Configuration::getAudioEnabled()) {
    // Initialize audio
    SDL_AudioSpec desiredSpec;
    desiredSpec.freq = Configuration::getAudioFrequency();
    desiredSpec.format = AUDIO_S8;
    desiredSpec.channels = 1;
    desiredSpec.samples = 2048;
    desiredSpec.callback = audioCallback;
    desiredSpec.userdata = NULL;

    SDL_AudioSpec obtainedSpec;
    SDL_OpenAudio(&desiredSpec, &obtainedSpec);

    // Start playing audio
    SDL_PauseAudio(0);
  }

  return true;
}

/**
 * Shutdown libraries for exit.
 */
static void shutdown() {
  SDL_CloseAudio();

  SDL_DestroyTexture(scanlineTexture);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  SDL_Quit();
}

static void mainLoop() {
  SMBEngine engine(romImage);
  smbEngine = &engine;
  engine.reset();

  bool running = true;
  int progStartTime = SDL_GetTicks();
  int frame = 0;

  // level config from IJON
  int sleep = 100;
  int enter = 0;

  uint64_t last_world_pos = 0xffffffff;
  uint64_t idle = 0;
  bool hammertime = false;

  while (running) {

    if (sleep == 0) {
      enter = 1;
      engine.writeData(0x0760, level); // overwrite level
    } else {
      sleep--;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_WINDOWEVENT:
        switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
          running = false;
          break;
        }
        break;

      default:
        break;
      }
    }

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    Controller &controller1 = engine.getController1();
    controller1.setButtonState(BUTTON_A, keys[SDL_SCANCODE_X]);
    controller1.setButtonState(BUTTON_B, keys[SDL_SCANCODE_Z]);
    controller1.setButtonState(BUTTON_SELECT, keys[SDL_SCANCODE_BACKSPACE]);
    controller1.setButtonState(BUTTON_START, keys[SDL_SCANCODE_RETURN]);
    controller1.setButtonState(BUTTON_UP, keys[SDL_SCANCODE_UP]);
    controller1.setButtonState(BUTTON_DOWN, keys[SDL_SCANCODE_DOWN]);
    controller1.setButtonState(BUTTON_LEFT, keys[SDL_SCANCODE_LEFT]);
    controller1.setButtonState(BUTTON_RIGHT, keys[SDL_SCANCODE_RIGHT]);

    if (keys[SDL_SCANCODE_R]) {
      // Reset
      engine.reset();
    }
    if (keys[SDL_SCANCODE_ESCAPE]) {
      // quit
      running = false;
      break;
    }
    if (keys[SDL_SCANCODE_F]) {
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    engine.update();
    engine.render(renderBuffer);

    SDL_UpdateTexture(texture, NULL, renderBuffer,
                      sizeof(uint32_t) * RENDER_WIDTH);

    SDL_RenderClear(renderer);

    // Render the screen
    SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT);
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    // Render scanlines
    //
    if (Configuration::getScanlinesEnabled()) {
      SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH * 3, RENDER_HEIGHT * 3);
      SDL_RenderCopy(renderer, scanlineTexture, NULL, NULL);
    }

    SDL_RenderPresent(renderer);

    /**
     * Ensure that the framerate stays as close to the desired FPS as possible.
     * If the frame was rendered faster, then delay. If the frame was slower,
     * reset time so that the game doesn't try to "catch up", going super-speed.
     */
    int now = SDL_GetTicks();
    int delay = progStartTime +
                int(double(frame) * double(MS_PER_SEC) /
                    double(Configuration::getFrameRate())) -
                now;
    if (delay > 0) {
      SDL_Delay(delay);
    } else {
      frame = 0;
      progStartTime = now;
    }
    frame++;
  }
}

int main(int argc, char **argv) {
  if (argc > 2) {
    printf("usage: smbc level\n");
    exit(0);
  }

  printf("got argc %d\n", argc);
  assert(argc > 1);

  level = atoi(argv[1]);
  printf("run level %d\n", level);

  if (level >= 36 || level < 0) {
    printf("ERROR: invalid level...\n");
    printf("===== Levels: =====\n");
    printf("0:      Level 1-1\n");
    printf("1:  Pre Level 1-2\n");
    printf("2:      Level 1-2\n");
    printf("3:      Level 1-3\n");
    printf("4:      Level 1-4\n");
    printf("5:      Level 2-1\n");
    printf("6:  Pre Level 2-2\n");
    printf("7:      Level 2-2\n");
    printf("8:      Level 2-3\n");
    printf("9:      Level 2-4\n");
    printf("10:     Level 3-1\n");
    printf("11:     Level 3-2\n");
    printf("12:     Level 3-3\n");
    printf("13:     Level 3-4\n");
    printf("14:     Level 4-1\n");
    printf("15: Pre Level 4-2\n");
    printf("16:     Level 4-2\n");
    printf("17:     Level 4-3\n");
    printf("18:     Level 4-4\n");
    printf("19:     Level 5-1\n");
    printf("20:     Level 5-2\n");
    printf("21:     Level 5-3\n");
    printf("22:     Level 5-4\n");
    printf("23:     Level 6-1\n");
    printf("24:     Level 6-2\n");
    printf("25:     Level 6-3\n");
    printf("26:     Level 6-4\n");
    printf("27:     Level 7-1\n");
    printf("28: Pre Level 7-2\n");
    printf("29:     Level 7-2\n");
    printf("30:     Level 7-3\n");
    printf("31:     Level 7-4\n");
    printf("32:     Level 8-1\n");
    printf("33:     Level 8-2\n");
    printf("34:     Level 8-3\n");
    printf("35:     Level 8-4\n");
    exit(0);
  }
  if (!initialize()) {
    std::cout << "Failed to initialize. Please check previous error messages "
                 "for more information. The program will now exit.\n";
    return -1;
  }

  mainLoop();

  shutdown();

  return 0;
}
