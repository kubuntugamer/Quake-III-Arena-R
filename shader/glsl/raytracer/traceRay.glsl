layout(location = PAYLOAD_BRDF) rayPayloadEXT RayPayload rp;
layout(location = PAYLOAD_SHADOW) rayPayloadEXT ShadowRayPayload rp_shadow;
#ifndef REFLECT_PAYLOAD_EXTERNAL
layout(location = PAYLOAD_REFLECT) rayPayloadEXT RayPayloadReflect rp_reflect;
#endif

layout(binding = BINDING_OFFSET_AS, set = 0) uniform accelerationStructureEXT topLevelAS;



void trace_ray(Ray ray, uint cullMask)
{
	const uint rayFlags = gl_RayFlagsNoneEXT;//gl_RayFlagsCullFrontFacingTrianglesEXT;//gl_RayFlagsOpaqueEXT;// | gl_RayFlagsCullBackFacingTrianglesEXT;
	//const uint rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsCullBackFacingTrianglesEXT;
	//const uint cullMask = RAY_FIRST_PERSON_VISIBLE;
	rp.transparent = vec4(0);
	rp.max_transparent_distance = 0;
	rp.addCount = 0;
	rp.transLight = false;
	rp.trans = false;

	traceRayEXT( topLevelAS, rayFlags, cullMask,
			SBT_RCHIT_OPAQUE /*sbtRecordOffset*/, 0 /*sbtRecordStride*/, SBT_RMISS_PATH_TRACER /*missIndex*/,
			ray.origin, ray.t_min, ray.direction, ray.t_max, PAYLOAD_BRDF);
}

void traceRayOpaque(Ray ray, uint cullMask)
{
	const uint rayFlags = gl_RayFlagsCullFrontFacingTrianglesEXT;
	rp.transparent = vec4(0);
	rp.max_transparent_distance = 0;
	rp.addCount = 0;
	rp.transLight = false;
	rp.trans = false;

	traceRayEXT( topLevelAS, rayFlags, cullMask,
			SBT_RCHIT_OPAQUE /*sbtRecordOffset*/, 0 /*sbtRecordStride*/, SBT_RMISS_PATH_TRACER /*missIndex*/,
			ray.origin, ray.t_min, ray.direction, ray.t_max, PAYLOAD_BRDF);
}
void traceRay(Ray ray, uint cullMask)
{
	const uint rayFlags = gl_RayFlagsNoOpaqueEXT  ;
	rp.transparent = vec4(0);
	rp.max_transparent_distance = 0;
	rp.addCount = 0;
	rp.transLight = false;
	rp.trans = false;

	traceRayEXT( topLevelAS, rayFlags, cullMask,
			SBT_RCHIT_OPAQUE /*sbtRecordOffset*/, 0 /*sbtRecordStride*/, SBT_RMISS_PATH_TRACER /*missIndex*/,
			ray.origin, ray.t_min, ray.direction, ray.t_max, PAYLOAD_BRDF);
}

float trace_shadow_ray(vec3 pos, vec3 dir, float t_min, float t_max, bool is_player)
{
	const uint rayFlags = gl_RayFlagsNoOpaqueEXT ;//gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
	uint cullMask = RAY_MIRROR_OPAQUE_VISIBLE;
	if(is_player) {
		cullMask = RAY_FIRST_PERSON_OPAQUE_VISIBLE;
	}
	
	Ray ray;
	ray.origin = pos;
	ray.direction = dir;
	ray.t_min = t_min;
	ray.t_max = t_max - t_min;

	rp_shadow.visFactor = 0.0f;

	traceRayEXT( topLevelAS, rayFlags, cullMask,
			SBT_RCHIT_SHADOW_RAY /*sbtRecordOffset*/, 0 /*sbtRecordStride*/, SBT_RMISS_SHADOW_RAY /*missIndex*/,
			ray.origin, ray.t_min, ray.direction, ray.t_max, PAYLOAD_SHADOW);

	return rp_shadow.visFactor;
}

