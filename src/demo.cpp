#include "pch.h"
#include "sample1.h"
#include "vkjs/vkjs.h"
#include "jsrlib/jsr_logger.h"

//#include "jsrlib/jsr_jobsystem2.h"

template<class T, class R = T>
class cFunc3 {
public:
    virtual R operator()(T a, T b, T c) = 0;
};

template<class T, class R = T>
class muladd : public cFunc3<T,R> {
    R operator()(T a, T b, T c) { 
        T p1 = static_cast<T>(a);
        T p2 = static_cast<T>(b);
        T p3 = static_cast<T>(c);
        return static_cast<R>(p1 * p2 + p3);
    }
};

float func1(float x, cFunc3<float>& fn) {
    return x * fn(x, 2, 3);
}

void demo()
{
    jsrlib::gLogWriter.SetFileName("vulkan_engine.log");

    auto* fn1 = new muladd<float>();
    float x = func1(2.4f, *fn1);

    std::cout << "x=" << x << "\n";

    Sample1App* app = new Sample1App(true);
    app->settings.hdr = false;
    app->settings.fullscreen = false;
    app->settings.exclusive = false;
    app->settings.vsync = true;
    app->settings.vfreq = static_cast<float>(60);
    app->settings.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    app->width = 1920;
    app->height = 1080;
    
    if (app->init())
    {
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

