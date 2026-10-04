// reflect-trace helpers (payload PAYLOAD_REFLECT); included only by stages
// that declare an OUT ray payload at PAYLOAD_REFLECT (see traceRay.glsl guard).
void traceStraight(vec3 P, vec3 D, vec3 N){
	// if(dot(D, N) >= 0)
	// 	P += D * 0.001;
	// else
	// 	P -= N * 0.001;

	traceRayEXT( topLevelAS, gl_RayFlagsNoneEXT, RAY_FIRST_PERSON_OPAQUE_VISIBLE | RAY_FIRST_PERSON_PARTICLE_VISIBLE,
			SBT_RCHIT_OPAQUE, 0, SBT_RMISS_PATH_TRACER,
			P, 0.01, D, 10000.0, PAYLOAD_REFLECT);
}
void traceReflect(vec3 P, vec3 D, vec3 N, bool player){
	D = reflect(D, N);
	if(dot(D, N) >= 0)
		P -= D * 0.001;
	else
		P -= N * 0.001;

	uint mask = RAY_MIRROR_OPAQUE_VISIBLE | RAY_MIRROR_PARTICLE_VISIBLE;
	if(player) mask = RAY_FIRST_PERSON_OPAQUE_VISIBLE | RAY_FIRST_PERSON_PARTICLE_VISIBLE;
	traceRayEXT( topLevelAS, gl_RayFlagsNoneEXT, mask,
			SBT_RCHIT_OPAQUE, 0, SBT_RMISS_PATH_TRACER,
			P, 0.01, D, 10000.0, PAYLOAD_REFLECT);
}
void traceRefract(vec3 P, vec3 D, vec3 N, float n1, float n2, bool player){
	D = refract( D, N, n2/n1 );
	D = refract( D, N, n1/n2 );
	if(dot(D, N) >= 0)
		P -= D * 0.001;
	else
		P -= N * 0.001;
		
	uint mask = RAY_MIRROR_OPAQUE_VISIBLE | RAY_MIRROR_PARTICLE_VISIBLE;
	if(player) mask = RAY_FIRST_PERSON_OPAQUE_VISIBLE | RAY_FIRST_PERSON_PARTICLE_VISIBLE;
	traceRayEXT( topLevelAS, gl_RayFlagsNoneEXT, mask,
			SBT_RCHIT_OPAQUE, 0, SBT_RMISS_PATH_TRACER,
			P, 0.01, D, 10000.0, PAYLOAD_REFLECT);
}