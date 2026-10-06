// ============================================================================
// vkq3ng RT Showcase Map - Custom Shaders
// ============================================================================
//
// IMPORTANT: this engine does not implement per-material shader directives.
// There is no rt_refraction, rt_metallic, rt_roughness, rt_emissive, and so
// on. Worse, an unrecognised token in a shader body makes ParseShader() return
// qfalse, which marks the shader as defaultShader: it keeps its name but
// loses every stage, so it renders as the engine's default texture. A shader
// script full of unsupported directives therefore does not degrade to "no
// PBR" -- it degrades to "no texture at all".
//
// Materials are instead classified by the renderer from the shader NAME and
// the standard Q3 sort keyword. See RB_GetMaterial() in code/renderer/
// tr_material.c. The reachable set is:
//
//   MATERIAL_KIND_GLASS   name contains "glass"
//   MATERIAL_KIND_WATER   name contains "textures/liquids/calm_poollight"
//                         or "textures/liquids/clear_ripple1"
//   MATERIAL_FLAG_SEE_THROUGH_ADD   name contains "portal", or sort is blend0
//   MATERIAL_FLAG_MIRROR  sort is portal AND name contains "mirror"
//   MATERIAL_FLAG_LIGHT   a handful of hard-coded lamp texture names
//
// MATERIAL_KIND_LAVA, _CHROME, _SLIME, _FOG, _SKY, _TRANSPARENT, _SCREEN and
// _INVISIBLE are defined in shader/glsl/constants.h but never assigned by
// RB_GetMaterial(), so no shader name can reach them. A "lava" surface is
// ordinary diffuse geometry as far as the ray tracer is concerned.
//
// The shaders below are written to that system: standard Q3 syntax only, with
// names and sort keywords chosen so each room exercises a distinct ray-traced
// material behaviour.
// ============================================================================

// ----------------------------------------------------------------------------
// GLASS - refractive, reflective
// Name contains "glass" -> MATERIAL_KIND_GLASS, which the ray tracer treats
// with refraction and reflection (reflect_rays.rchit).
// ----------------------------------------------------------------------------
textures/rt_showcase/glass_clean {
    qer_editorimage textures/rt_showcase/glass_clean_col
    surfaceparm trans
    surfaceparm nonsolid
    {
        map textures/rt_showcase/glass_clean_col
        blendFunc blend
        rgbGen identity
    }
}

// ----------------------------------------------------------------------------
// WATER - refractive liquid surface
//
// The name is deliberately redundant. RB_GetMaterial() classifies a shader as
// MATERIAL_KIND_WATER only when the name contains the literal substring
// "textures/liquids/calm_poollight", so the name below embeds that path inside
// the rt_showcase namespace. It looks like a typo; it is load-bearing.
// ----------------------------------------------------------------------------
textures/rt_showcase/textures/liquids/calm_poollight {
    qer_editorimage textures/rt_showcase/lava_emissive_col
    surfaceparm trans
    surfaceparm nonsolid
    surfaceparm water
    {
        map textures/rt_showcase/lava_emissive_col
        blendFunc blend
        rgbGen identity
        tcMod scroll 0.15 0.05
    }
}

// ----------------------------------------------------------------------------
// MIRROR - perfect reflection
// sort portal puts the shader at SS_PORTAL, and the name contains "mirror", so
// RB_GetMaterial() sets MATERIAL_FLAG_MIRROR. This is the only way to get a
// ray-traced planar reflection in this engine.
// ----------------------------------------------------------------------------
textures/rt_showcase/metal_mirror {
    qer_editorimage textures/rt_showcase/metal_brushed_col
    sort portal
    surfaceparm noshadow
    {
        map textures/rt_showcase/metal_brushed_col
        rgbGen identity
    }
}

// ----------------------------------------------------------------------------
// METAL - ordinary PBR-ish surface
// No reachable material kind for metal, so this is MATERIAL_KIND_REGULAR.
// It exists to show the baseline the glass, water and mirror are compared
// against, and to carry the normal map.
// ----------------------------------------------------------------------------
textures/rt_showcase/metal_brushed {
    qer_editorimage textures/rt_showcase/metal_brushed_col
    {
        map textures/rt_showcase/metal_brushed_col
        rgbGen identity
    }
}

// ----------------------------------------------------------------------------
// CONCRETE - ordinary diffuse surface, the map's baseline material
// ----------------------------------------------------------------------------
textures/rt_showcase/concrete_damaged {
    qer_editorimage textures/rt_showcase/concrete_damaged_col
    {
        map textures/rt_showcase/concrete_damaged_col
        rgbGen identity
    }
}

// ----------------------------------------------------------------------------
// SKY / ENVIRONMENT
// MATERIAL_KIND_SKY is not reachable from RB_GetMaterial(), so this is an
// ordinary surface. It is textured with the environment map so that it reads
// as sky in the rasteriser and gives the ray tracer something to reflect.
// ----------------------------------------------------------------------------
textures/rt_showcase/sky_environment {
    qer_editorimage textures/rt_showcase/sky_environment_col
    surfaceparm sky
    surfaceparm noimpact
    {
        map textures/rt_showcase/sky_environment_col
        rgbGen identity
    }
}

// ----------------------------------------------------------------------------
// PORTAL - additive transparent
// Name contains "portal" -> MATERIAL_FLAG_SEE_THROUGH_ADD, which the ray
// tracer composites additively rather than with alpha blending.
// ----------------------------------------------------------------------------
textures/rt_showcase/portal {
    qer_editorimage textures/rt_showcase/portal_col
    surfaceparm trans
    surfaceparm nonsolid
    {
        map textures/rt_showcase/portal_col
        blendFunc add
        rgbGen wave sin 0.5 0.5 0 0.5
    }
}
