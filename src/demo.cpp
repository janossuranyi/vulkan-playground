#include "pch.h"
#include "sample1.h"
#include "sample2.h"
#include "vkjs/vkjs.h"
#include "jsrlib/jsr_logger.h"
#include <memory>
#include <iostream>

//#include "jsrlib/jsr_jobsystem2.h"

void demo()
{
    jsrlib::gLogWriter.SetFileName("vulkan_engine.log");
    auto app = std::make_unique<Sample1App>(true);
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

}

int main(int argc, char* argv[])
{
    demo();

    return 0;
}

