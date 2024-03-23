#include "pch.h"
#include "sample1.h"
#include "vkjs/vkjs.h"
#include "jsrlib/jsr_logger.h"

//#include "jsrlib/jsr_jobsystem2.h"

template<class R, class ...Args>
struct Function
{
    virtual R operator()(Args... args) = 0;
};

struct MyFunc : public Function<float, int, int, int>
{
    float operator()(int a, int b, int c) override
    {
        return static_cast<float>(a + b * c);
    }
};

float foo(int a, int b, int c, Function<float, int, int, int>& fn)
{
    return fn(a, b, c) / 0.5f;
}

void demo()
{
    jsrlib::gLogWriter.SetFileName("vulkan_engine.log");


    Sample1App* app = new Sample1App(true);
    app->settings.hdr = true;
    app->settings.fullscreen = false;
    app->settings.exclusive = false;
    app->settings.vsync = true;
    app->settings.vfreq = static_cast<float>(60);
    app->settings.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    app->width = 1920;
    app->height = 1080;
    
    if (app->init())
    {
        MyFunc fn;

        std::cout << "fn()=" << foo(2,3,5,fn) << "\n";
        app->prepare();
        app->run();
    }

    delete app;    
}

int main(int argc, char* argv[])
{
    demo();

    return 0;
}

