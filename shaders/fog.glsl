#ifndef _FOG_INCLUDE
#define _FOG_INCLUDE

struct FogParameters
{
	vec3 color;
	float linearStart;
	float linearEnd;
	float density;
	
	int equation;
	bool isEnabled;
};

float getFogFactor(FogParameters params, float fogCoordinate)
{
	float result = 0.0;
	if(params.equation == 0)
	{
		float fogLength = params.linearEnd - params.linearStart;
		result = (params.linearEnd - fogCoordinate) / fogLength;
	}
	else if(params.equation == 1) {
		result = exp(-params.density * fogCoordinate);
	}
	else if(params.equation == 2) {
        float k = params.density * fogCoordinate;
        k *= k;
		result = exp(-k);
	}
	
	return clamp(result, 0.0, 1.0);
}

#endif