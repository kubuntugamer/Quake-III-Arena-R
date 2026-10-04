#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "defines.glsl"

//#include "rt_vattributes.glsl"

// Push Constants
layout(push_constant) uniform PushConstant {
    layout(offset = 160) mat4 mvp;
};

// Bindings
//layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
//layout(binding = 0, set = 1) uniform sampler2D tex[];

// Specific
hitAttributeEXT vec2 hitAttribute;
layout(location = 0) rayPayloadInEXT RayPayload rp;

const mat4 clip = mat4(1.0f,  0.0f, 0.0f, 0.0f,
                         0.0f, -1.0f, 0.0f, 0.0f,
                         0.0f,  0.0f, 0.5f, 0.0f,
                         0.0f,  0.0f, 0.5f, 1.0f);

void main()
{
  // if(rp.depth > 6) return;
  // rp.depth++;

  // Triangle triangle = getTriangle();
  // HitPoint hitPoint = getHitPoint();

  // // COLOR AND DISTANCE
  // vec4 color = /*(c/255) */ texture(tex[uint(uint(iData.data[gl_InstanceID].texIdx))], hitPoint.uv); //calcLOD(index));
	// rp.color += color;//vec4(color.w, color.w, color.w, color.w);//barycentricCoords;
  // rp.blendFunc = iData.data[gl_InstanceID].blendfunc;
  // rp.distance = gl_RayTmaxEXT;
  // //rp.transparent = uint(instanceData.data[gl_InstanceID].texIdx2);

  // if(iData.data[gl_InstanceID].isMirror == true){
  //   vec3 direction2 = reflect(gl_WorldRayDirectionEXT, triangle.normal);
  //   rp.cullMask = MIRROR_VISIBLE;
  //   uint rayFlags = gl_RayFlagsCullBackFacingTrianglesEXT;// = /*gl_RayFlagsOpaqueEXT | */gl_RayFlagsCullFrontFacingTrianglesEXT ;
  //   float tmin = 0.01;
  //   float tmax = 10000.0;
  //   traceRayEXT(topLevelAS, rayFlags, rp.cullMask, 0, 0, 0, gl_WorldRayOriginEXT + gl_RayTmaxEXT * gl_WorldRayDirectionEXT, tmin, direction2, tmax, 0);
  // } else if(iData.data[gl_InstanceID].shaderSort > SS_OPAQUE || 
  //   iData.data[gl_InstanceID].blendfunc == (GLS_SRCBLEND_ONE|GLS_DSTBLEND_ONE)){
  //   uint rayFlags = 0;//gl_RayFlagsCullBackFacingTrianglesEXT;// = gl_RayFlagsCullFrontFacingTrianglesEXT ;
  //   float tmin = 0.01;
  //   float tmax = 10000.0;
  //   traceRayEXT(topLevelAS, rayFlags, rp.cullMask, 0, 0, 0, gl_WorldRayOriginEXT + ((gl_RayTmaxEXT+ 0.1) * gl_WorldRayDirectionEXT), tmin, gl_WorldRayDirectionEXT, tmax, 0);
  // }

  rp.barycentric = hitAttribute;
  rp.instanceID = gl_InstanceID;
	rp.primitiveID = gl_PrimitiveID;
	rp.hit_distance = gl_RayTmaxEXT;
  //rp.modelmat = gl_ObjectToWorldEXT;
}  

//gl_WorldRayOriginEXT;
//gl_WorldRayDirectionEXT;
// https://media.contentapi.ea.com/content/dam/ea/seed/presentations/2019-ray-tracing-gems-chapter-20-akenine-moller-et-al.pdf
// float calcLOD(ivec3 index){
//   // screen space
//   vec2 p0 =  (clip * mvp * vertices.v[index.x].pos).xy;
//   vec2 p1 =  (clip * mvp * vertices.v[index.y].pos).xy;
//   vec2 p2 =  (clip * mvp * vertices.v[index.z].pos).xy;
//   // world space
//   vec3 P0 =  vertices.v[index.x].pos.xyz;
//   vec3 P1 =  vertices.v[index.y].pos.xyz;
//   vec3 P2 =  vertices.v[index.z].pos.xyz;


//   vec2 uv0 = vertices.v[index.x].uv.xy;
//   vec2 uv1 = vertices.v[index.y].uv.xy;
//   vec2 uv2 = vertices.v[index.z].uv.xy;

//   ivec2 texSize = textureSize(tex[uint(uint(instanceData.data[gl_InstanceID].texIdx))], 0);

//   float t_a = texSize.x * texSize.y * abs((uv1.x - uv0.x)*(uv2.y - uv0.y)-
//                                           (uv2.x - uv0.x)*(uv1.y - uv0.y));

//   float txn = texSize.x * texSize.y * abs((uv0.x - uv1.y)+(uv1.x - uv2.y)+(uv2.x - uv0.y)-
//                                           (uv0.y - uv1.x)-(uv1.y - uv2.x)-(uv2.y - uv0.x));
//   float xps = (texSize.x * texSize.y)/(gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y);
//   float txs = txn * xps;
//   // screen space
//   //float p_a = abs((p1.x - p0.x)*(p2.y - p0.y)-(p2.x - p0.x)*(p1.y - p0.y));
//   // world space
//   float p_a = length(cross(P1-P0,P2-P0));

//   vec3 AB = vertices.v[index.y].pos.xyz - vertices.v[index.x].pos.xyz;
//   vec3 AC = vertices.v[index.z].pos.xyz - vertices.v[index.x].pos.xyz;
//   vec3 n = normalize(cross(AB, AC));

//   float lod = (0.5f * log2(t_a/p_a)); 
//   //lod += log2(abs(gl_HitTEXT));
//   //lod += 0.5f * log2(gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y); 
//   //lod -= log2(abs(dot(normalize(n),normalize(gl_WorldRayOriginEXT + gl_RayTmaxEXT * gl_WorldRayDirectionEXT))));
//   return 1 / lod;//sqrt(txs/p_a);
// }