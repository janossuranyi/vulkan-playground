#ifndef CAMERA_INC_
#define CAMERA_INC_

#ifndef sqr
float sqr(float x) { return x*x; }
#endif

float computeEV100( float aperture, float shutterTime, float ISO )
{
    // EV number is defined as:
    // 2^ EV_s = N^2 / t and EV_s = EV_100 + log2 (S /100)
    // This gives
    // EV_s = log2 (N^2 / t)
    // EV_100 + log2 (S /100) = log2 (N^2 / t)
    // EV_100 = log2 (N^2 / t) - log2 (S /100)
    // EV_100 = log2 (N^2 / t . 100 / S)
    return log2( sqr( aperture ) / shutterTime * 100.0 / ISO );
}

float computeEV100FromAvgLuminance( float avgLuminance )
{
    // We later use the middle gray at 12.7% in order to have
    // a middle gray at 18% with a sqrt (2) room for specular highlights
    // But here we deal with the spot meter measuring the middle gray
    // which is fixed at 12.5 for matching standard camera
    // constructor settings (i.e. calibration constant K = 12.5)
    // Reference : http://en.wikipedia.org/wiki/Film_speed
    return log2( avgLuminance * 100.0 / 12.5 );
}

float convertEV100ToExposure( float EV100 )
{
    // Compute the maximum luminance possible with H_sbs sensitivity
    // maxLum = 78 / ( S * q ) * N^2 / t
    // = 78 / ( S * q ) * 2^ EV_100
    // = 78 / (100 * 0.65) * 2^ EV_100
    // = 1.2 * 2^ EV
    // Reference: http://en.wikipedia.org/wiki/Film_speed
    float maxLuminance = 1.2 * pow ( 2.0, EV100 );
    return 1.0 / maxLuminance ;
}

#endif