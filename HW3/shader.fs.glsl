#version 330

in vec2 texCoord;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec3 _aPos;

out vec4 fragColor;

uniform mat4 um4p;	// projection matrix
uniform mat4 um4v;	// camera viewing transformation matrix
uniform mat4 um4m;	// rotation matrix

uniform mat4 view;

uniform int is_pixel_vertex;
uniform int LightIdx;
uniform float shininess;

// [TODO] passing texture from main.cpp
// Hint: sampler2D
uniform sampler2D tex;

struct LightInfo{
	vec3 Position;
	vec3 spotDirection;
	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	float spotExponent;
	float spotCutoff;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
};
uniform LightInfo light[3];

struct MaterialInfo
{
	vec3 Ka;
	vec3 Kd;
	vec3 Ks;
};
uniform MaterialInfo material;

float dot(vec3 u, vec3 v) {
	return u.x * v.x + u.y * v.y + u.z * v.z;
}

vec3 directionalLight(vec3 N, vec3 V){

	vec4 lightInView = view * vec4(light[0].Position, 1.0f);
	vec3 S = normalize(lightInView.xyz + V);		 
	vec3 H = normalize(S + V);

	float dot_v = dot(N, S);
	float pow_v = pow(max(dot(N, H), 0), shininess);
	
	return light[0].Ambient * material.Ka + dot_v * light[0].Diffuse * material.Kd + pow_v * light[0].Specular * material.Ks;
}


void main() {

	// [TODO] sampleing from texture
	// Hint: texture
	vec4 vertexInView = um4v * um4m * vec4(_aPos.x, _aPos.y, _aPos.z, 1.0);

	vec3 N = normalize(vertex_normal);
	vec3 V = -vertexInView.xyz;
	vec3 color = vec3(0, 0, 0);

	if(LightIdx==0)
		color = directionalLight(N, V);

	if(is_pixel_vertex==1)
		fragColor = vec4(vertex_color, 1.0f);
	else
		fragColor = vec4(color, 1.0f);

	fragColor *= texture(tex, texCoord);
}
