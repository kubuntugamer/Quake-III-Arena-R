#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "defines.glsl"

layout(location = 0) rayPayloadInEXT RayPayload rp;

void main()
{
	rp.instanceID = ~0u;
}