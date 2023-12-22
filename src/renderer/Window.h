#pragma once

#include <SDL.h>

namespace jsr {

    struct WindowCreateInfo {
        bool fullscreen;
        int width;
        int height;
        const char* title;
    };

    class Window {
    public:
        void init(const WindowCreateInfo& windowCreateInfo);
        SDL_Window* getWindow();
        void getWindowSize(int xy[2]);
        ~Window();
    private:
        SDL_Window* m_window = {};
    };
}