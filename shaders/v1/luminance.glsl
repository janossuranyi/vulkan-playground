#ifndef LUMINANCE_INC
#define LUMINANCE_INC

//reference: https://en.wikipedia.org/wiki/Relative_luminance
float computeLuminance(vec3 color){
    return dot(color, vec3(0.21, 0.72, 0.07));
}

#endif // #ifndef LUMINANCE_INC