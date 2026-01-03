
layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 cameraPosition;
	vec4 ambientColor;
	vec4 sunlightPosition;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	mat4 sunlightViewProj;
	uvec4 shadowParams;
	vec4 spotlightPos;
	vec4 spotlightDir;
	vec4 spotColor;
	vec4 spotCutoffAndIntensity;
} sceneData;

#ifdef USE_BINDLESS
layout(set = 0, binding = 1) uniform sampler2D shadowMaps[];
layout(set = 0, binding = 2) uniform sampler2D allTextures[];
#else
layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;
#endif

layout(set = 1, binding = 0) uniform GLTFMaterialData{   

	vec4 colorFactors;
	vec4 metal_rough_factors;
	int colorTexID;
	int metalRoughTexID;
} materialData;

