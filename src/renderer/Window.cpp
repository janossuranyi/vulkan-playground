#include "renderer/window.h"
#include <stdexcept>

namespace jsr {
	
	void Window::init(const WindowCreateInfo& windowCreateInfo)
	{
        // We initialize SDL and create a window with it. 
        SDL_Init(SDL_INIT_VIDEO);

        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (windowCreateInfo.fullscreen)
        {
            if (windowCreateInfo.width == 0 && windowCreateInfo.height == 0)
            {
                // borderless desktop resolution
                window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            else
            {
                window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_FULLSCREEN);
            }
        }

        m_window = SDL_CreateWindow(
            windowCreateInfo.title,
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            windowCreateInfo.width,
            windowCreateInfo.height,
            window_flags
        );

        if (m_window == nullptr) {
            throw std::runtime_error("SDL cannot create window !");
        }
	}
    SDL_Window* Window::getWindow()
    {
        return m_window;
    }
    void Window::getWindowSize(int xy[2])
    {
        SDL_GetWindowSize(m_window, &xy[0], &xy[1]);
    }
    Window::~Window()
    {
        if (m_window) {
            SDL_DestroyWindow(m_window);
        }
        SDL_Quit();
    }
}
