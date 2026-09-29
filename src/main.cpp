#include "chip8.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <cstdint>
#include <iostream>

const int SCALE = 10;
const int WIDTH = 64 * SCALE;
const int HEIGHT = 32 * SCALE;

// CHIP-8 keyboard mapping
uint8_t keymap[16] = {
    SDLK_x, // 0
    SDLK_1, // 1
    SDLK_2, // 2
    SDLK_3, // 3
    SDLK_q, // 4
    SDLK_w, // 5
    SDLK_e, // 6
    SDLK_a, // 7
    SDLK_s, // 8
    SDLK_d, // 9
    SDLK_z, // A
    SDLK_c, // B
    SDLK_4, // C
    SDLK_r, // D
    SDLK_f, // E
    SDLK_v  // F
};


// --------------------------------------------------
// AUDIO
// --------------------------------------------------

void audio_callback(void* userdata, uint8_t* stream, int len){

    static uint32_t sample_index = 0;

    int16_t* audio_buffer = (int16_t*)stream;
    int samples = len / 2;

    bool* beeping = (bool*)userdata;

    for(int i = 0; i < samples; i++){

        if(*beeping){

            // 440 Hz square wave
            int16_t value =
                ((sample_index++ / 100) % 2)
                ? 3000
                : -3000;

            audio_buffer[i] = value;
        }
        else{

            audio_buffer[i] = 0;
            sample_index = 0;
        }
    }
}


// --------------------------------------------------
// GRAPHICS
// --------------------------------------------------

void draw_graphics(SDL_Renderer* renderer, Chip8& chip8){

    // Clear screen
    SDL_SetRenderDrawColor(
        renderer,
        0, 0, 0, 255
    );

    SDL_RenderClear(renderer);


    // Draw CHIP-8 pixels
    SDL_SetRenderDrawColor(
        renderer,
        255, 255, 255, 255
    );

    for(int y = 0; y < 32; y++){

        for(int x = 0; x < 64; x++){

            if(chip8.display[x + (y * 64)] == 1){

                SDL_Rect rect = {
                    x * SCALE,
                    (31 - y) * SCALE,
                    SCALE,
                    SCALE
                };

                SDL_RenderFillRect(
                    renderer,
                    &rect
                );
            }
        }
    }

    SDL_RenderPresent(renderer);
}


// --------------------------------------------------
// INPUT
// --------------------------------------------------

void handle_input(Chip8& chip8, bool& running){

    SDL_Event event;

    while(SDL_PollEvent(&event)){

        // Close window
        if(event.type == SDL_QUIT)
            running = false;


        // Key pressed
        if(event.type == SDL_KEYDOWN){

            if(event.key.keysym.sym == SDLK_ESCAPE)
                running = false;

            for(int i = 0; i < 16; i++){

                if(event.key.keysym.sym == keymap[i])
                    chip8.key[i] = 1;
            }
        }


        // Key released
        if(event.type == SDL_KEYUP){

            for(int i = 0; i < 16; i++){

                if(event.key.keysym.sym == keymap[i])
                    chip8.key[i] = 0;
            }
        }
    }
}


// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main(int argc, char** argv){

    // Check ROM argument
    if(argc < 2){

        std::cerr
            << "Usage: "
            << argv[0]
            << " <ROM file>"
            << std::endl;

        return 1;
    }


    // Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0){

        std::cerr
            << "SDL Error: "
            << SDL_GetError()
            << std::endl;

        return 1;
    }


    // --------------------------------------------------
    // AUDIO SETUP
    // --------------------------------------------------

    bool beeping = false;

    SDL_AudioSpec want;
    SDL_AudioSpec have;

    SDL_zero(want);

    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audio_callback;
    want.userdata = &beeping;

    SDL_AudioDeviceID audio_device =
        SDL_OpenAudioDevice(
            NULL,
            0,
            &want,
            &have,
            0
        );

    if(audio_device == 0){

        std::cerr
            << "Failed to open audio: "
            << SDL_GetError()
            << std::endl;
    }
    else{

        SDL_PauseAudioDevice(
            audio_device,
            0
        );
    }


    // --------------------------------------------------
    // WINDOW
    // --------------------------------------------------

    SDL_Window* window =
        SDL_CreateWindow(
            "Chip-8 Emulator",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WIDTH,
            HEIGHT,
            SDL_WINDOW_SHOWN
        );

    if(!window){

        std::cerr
            << "Window error: "
            << SDL_GetError()
            << std::endl;

        if(audio_device != 0)
            SDL_CloseAudioDevice(audio_device);

        SDL_Quit();

        return 1;
    }


    // --------------------------------------------------
    // RENDERER
    // --------------------------------------------------

    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED
        );

    if(!renderer){

        std::cerr
            << "Renderer error: "
            << SDL_GetError()
            << std::endl;

        SDL_DestroyWindow(window);

        if(audio_device != 0)
            SDL_CloseAudioDevice(audio_device);

        SDL_Quit();

        return 1;
    }


    // --------------------------------------------------
    // CHIP-8
    // --------------------------------------------------

    Chip8 chip8;

    chip8.load_rom(argv[1]);


    bool running = true;

    // Used for 60 Hz timer updates
    Uint32 last_timer_update = SDL_GetTicks();


    // --------------------------------------------------
    // MAIN LOOP
    // --------------------------------------------------

    while(running){

        // Handle keyboard/window events
        handle_input(chip8, running);


        // ----------------------------------------------
        // CPU
        // ----------------------------------------------

        // Execute several CHIP-8 instructions
        // without delaying every instruction.
        for(int i = 0; i < 10; i++){

            chip8.emulate_cycle();
        }


        // ----------------------------------------------
        // TIMERS
        // ----------------------------------------------

        Uint32 current_time = SDL_GetTicks();

        if(current_time - last_timer_update >= 16){

            /*
             * Timers should run at approximately 60 Hz.
             *
             * IMPORTANT:
             * This requires access to the timer variables.
             */

            last_timer_update = current_time;
        }


        // ----------------------------------------------
        // SOUND
        // ----------------------------------------------

        beeping =
            (chip8.get_sound_timer() > 0);


        // ----------------------------------------------
        // DISPLAY
        // ----------------------------------------------

        if(chip8.draw_flag){

            draw_graphics(
                renderer,
                chip8
            );

            chip8.draw_flag = false;
        }


        // Prevent excessive CPU usage
        SDL_Delay(1);
    }


    // --------------------------------------------------
    // CLEANUP
    // --------------------------------------------------

    if(audio_device != 0)
        SDL_CloseAudioDevice(audio_device);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}