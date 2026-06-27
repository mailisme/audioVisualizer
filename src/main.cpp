#include "imgui.h"
#include "../libs/imgui/backends/imgui_impl_sdl3.h"
#include "../libs/imgui/backends/imgui_impl_sdlrenderer3.h"
#include <stdio.h>
#include <algorithm>
#include <iostream>
#include <SDL3/SDL.h>
#include <thread>
#include <atomic>
#include <vector>
#define MINIAUDIO_IMPLEMENTATION
#include "../libs/miniaudio/miniaudio.h"
#include "fft.h"
#include "draw_line.h"


#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif


// init
int periodSizeInFrames = 960;
std::atomic<float> gRMS = 0.0f;
std::vector<float> audioBuffer;
std::vector<float> audioVolume;
std::atomic<int> buffer_write_index = 0;
std::atomic<int> volume_write_index = 0;
const int BUFFER_SIZE = 192000;
const int FFT_SIZE = 2048;
std::atomic<float> gFreq = 0;
std::thread analysis;
int sampleRate = 48000;




// fft
std::thread fftThread;
void FFTThread(int sampleRate)
{
    const int N = 2048;
    std::vector<float> frame(N);

    while (true)
    {
        int current = buffer_write_index.load();

        int size = audioBuffer.size() / 2;
        if (size < N)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        for (int i = 0; i < N; i++)
        {
            int base = current - N + i;

            if (base < 0)
                base += size;

            base %= size;

            int idx = base * 2;

            float l = audioBuffer[idx];
            float r = audioBuffer[idx + 1];

            frame[i] = (l + r) * 0.5f;
        }

        float freq = getFundamental(frame, sampleRate);

        static float smooth = 0;
        smooth = 0.9f * smooth + 0.1f * freq;

        gFreq.store(smooth);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

            
//audio loopback code

void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    const float* samples = (const float*)input;

    float sum = 0.0f, peak = 0.0f;
    //get peak
    for (ma_uint32 i = 0; i < frameCount * 2; i++)
    {
        float s = samples[i];
        sum += s * s;

        int idx = buffer_write_index.fetch_add(1) % BUFFER_SIZE;
        audioBuffer[idx] = samples[i];

        if (s > peak){
            peak = s;
        }
    }
    float rms = sqrtf(sum / (frameCount * 2));
    gRMS.store(rms);

    int idx2 = volume_write_index.fetch_add(1) % (BUFFER_SIZE / periodSizeInFrames);
    audioVolume[idx2] = rms;

    (void)device;
    (void)output;
}
void silent_playback_callback(ma_device* device, void* output, const void*, ma_uint32 frameCount){
    memset(output, 0, frameCount * ma_get_bytes_per_frame(device->playback.format, device->playback.channels));
}


int main(int, char**)
{
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }


    // fft threads
    fftThread = std::thread(FFTThread, sampleRate);
    fftThread.detach();


    //Audio Loopback Capture Init
    audioBuffer.resize(BUFFER_SIZE);
    audioVolume.resize(BUFFER_SIZE / periodSizeInFrames);
    ma_device_config config =
        ma_device_config_init(
            ma_device_type_loopback
        );

    config.capture.format = ma_format_f32;
    config.capture.channels = 2;
    config.sampleRate = 48000;
    config.periodSizeInFrames = periodSizeInFrames;

    config.dataCallback = data_callback;

    ma_device device;


    if (ma_device_init(
        nullptr,
        &config,
        &device
    ) != MA_SUCCESS)
    {
        printf("failed when trying to get loopback");
    }

    ma_device_start(&device);

    ma_device silentDevice;
    ma_device_config silentConfig = ma_device_config_init(ma_device_type_playback);
    silentConfig.playback.format   = ma_format_f32;
    silentConfig.playback.channels = 2;
    silentConfig.sampleRate        = 48000;
    silentConfig.dataCallback      = silent_playback_callback;

    if (ma_device_init(nullptr, &silentConfig, &silentDevice) != MA_SUCCESS)
    {
        printf("failed to init silent keep-alive device");
    }
    else
    {
        ma_device_start(&silentDevice);
    }




    SDL_Rect rect;
    SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &rect);
    int w = rect.w;
    int h = rect.h;
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_UTILITY | SDL_WINDOW_HIGH_PIXEL_DENSITY  | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP;
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+SDL_Renderer example", (int)(w), (int)(h / 7), window_flags);

    SDL_SetWindowPosition(window, 0, h - (h / 7));
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, h-(h/7));
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;        
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale * 0.8);        
    style.FontScaleDpi = main_scale * 0.8;       

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // state
    bool setting = false;
    bool volume = true;
    bool volume_history = true;
    bool oscilloscope = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    float silenceHoldTimer = 0.0f;
    const float SILENCE_THRESHOLD = -60.0f;
    const float SILENCE_HOLD_TIME    = 0.05f; 

    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }


        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();  
        ImGui::NewFrame();

        // Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static int counter = 0;
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(w / 10, h), ImGuiCond_Always);
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);

            ImGui::Begin("AudioVisualizer", nullptr, ImGuiWindowFlags_NoMove);

            if (ImGui::Button("X"))                       // Buttons return true when clicked (most widgets return true when edited/activated)
                break;
            
            ImGui::SameLine();
            if (ImGui::Button("Setting"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            {
                setting = !setting;
            }
            ImGui::Text("%.1f FPS",io.Framerate);
            ImGui::Text("%f Hz", gFreq.load());
            ImGui::Text("%f dB", 20.0f * log10f(gRMS.load()));
            ImGui::End();
        }

        //setting
        if (setting)
        {

            ImGui::Begin("setting", &setting);
            ImGui::Checkbox("volume bar", &volume);
            ImGui::Checkbox("volume History", &volume_history);
            ImGui::Checkbox("oscilloscope", &oscilloscope);
            // ImGui::ListBox()
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        SDL_RenderClear(renderer);

        //sdl render
        if (volume){
            float RMS = gRMS.load();

            static float displayPeak = 0.0f;

            float db = 20.0f * log10f(RMS);

            if (db > displayPeak)
            {
                displayPeak += (db - displayPeak) * 0.5f;
            }
            else
            {
                displayPeak += (db - displayPeak) * 0.05f;
            }
            float visual = (db + 60.0f) / 60.0f;

            int width = w / 100;
            int height = h / 7;

            int barHeight = (int)(visual * height);

            SDL_FRect rect = {
                (float)w - width,
                (float)height-barHeight,
                (float)width,
                (float)barHeight
            };
            SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255);
            SDL_RenderFillRect(renderer, &rect);
        }
        

        if (volume_history){        
            int writeIndex = volume_write_index.load();
            int size = audioVolume.size();

            for (int i = 0; i < size; i++)
            {
                int idx = (writeIndex - i + size) % size;

                float sample = (20.0f * log10f(audioVolume[idx]) + 60.0f) / 60.0f;


                int width = (w / 100) + ((i) * w * 0.001);
                int height = h / 7;
                int barHeight = (int)(sample * height);

                SDL_FRect rect = {
                    (float)w - width,
                    (float)height - barHeight,
                    (float)(w * 0.001),
                    (float)barHeight
                };

                SDL_SetRenderDrawColor(renderer, 0, 150, 100, 255); 
                SDL_RenderFillRect(renderer, &rect);
            }
        }


        
        if(oscilloscope){
            float otherWidth = w / 100 + w * audioVolume.size() * 0.001;
            const int SEARCH = 2048;
            float freq = gFreq.load();
            int N = 2048;
            const int DRAW = (freq > 1.0f) ? std::clamp((int)(sampleRate / freq), 8, N) : N;
            int current = buffer_write_index.load();
            int size = audioBuffer.size() / 2;
            float ybase = h / 14;
            std::vector<float> frame(N);

            // std::cout << "freq: " << gFreq.load() << ", N: " << N << std::endl;
            drawThickLine(renderer, w - otherWidth, 0, w - otherWidth, h / 7, 2, {255,255,255,255});
            drawThickLine(renderer, w - 2*otherWidth,   0, w - 2*otherWidth, h / 7, 2, {255,255,255,255});
            for (int i = 0; i < N; i++){
                int base = current - N + i;

                if (base < 0)
                    base += size;

                base %= size;

                int idx = base * 2;

                float l = audioBuffer[idx];
                float r = audioBuffer[idx + 1];
                frame[i] = (l + r) * 0.5f;
                
                            // std::cout << otherWidth * (i + 1) / N + (w - 2 * otherWidth) << "\n" << ybase + ybase * (l + r) * 0.5f << "\n";
            }

            int trigger = N - DRAW;

            for (int i = current - SEARCH; i < N - 1; i++)
            {
                float a = frame[(i + frame.size()) % frame.size()];
                float b = frame[(i + 1 + frame.size()) % frame.size()];

                if (a < 0 && b >= 0 && fabs(b-a) > 0.02f)
                {
                    trigger = i;
                }
            }

            float db = 20.0f * log10f(std::max(gRMS.load(), 1e-6f));
            if (db > SILENCE_THRESHOLD)
                silenceHoldTimer = SILENCE_HOLD_TIME;
            else
                silenceHoldTimer -= io.DeltaTime;

            bool hasLiveAudio = silenceHoldTimer > 0.0f;

            for (int i = 0; i < DRAW; i++){
                if (hasLiveAudio) {
                    drawThickLine(renderer, w - 2*otherWidth + i*otherWidth / DRAW, ybase + ybase * frame[(trigger + i) % frame.size()], w - 2*otherWidth + (i+1)*otherWidth / DRAW, ybase + ybase * frame[(trigger + i + 1) % frame.size()], 1, {255,255,255,255});
                } 
                else {
                    drawThickLine(renderer, w - 2*otherWidth + i*otherWidth / DRAW, ybase, w - 2*otherWidth + (i+1)*otherWidth / DRAW, ybase, 1, {255,255,255,255});
                }
            }
        }    


        //render everything out
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }




#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup  
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    ma_device_uninit(&device);
    ma_device_uninit(&silentDevice);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}