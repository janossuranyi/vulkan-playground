#ifndef LINEAR_DEPTH_INC
#define LINEAR_DEPTH_INC

//reference: https://stackoverflow.com/questions/51108596/linearize-depth
float linearize_depth(float d,float zNear,float zFar)
{
    return zNear * zFar / (zFar + d * (zNear - zFar));
}

#endif // #ifndef LINEAR_DEPTH_INC