#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;

uniform mat4 um4p;	
uniform mat4 um4v;
uniform mat4 um4m;

out vec2 texCoord;
out vec3 vertex_color;
out vec3 vertex_normal;
out vec3 _aPos;

uniform vec2 offset;
uniform int isEye;
uniform mat4 view;
uniform int LightIdx;
uniform float shininess;




// [TODO] passing uniform variable for texture coordinate offset

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

void main() 
{
	// [TODO]
	vec4 vertexInView = um4v * um4m * vec4(aPos.x, aPos.y, aPos.z, 1.0);
	vec4 normalInView = transpose(inverse(um4v * um4m)) * vec4(aNormal, 0.0);
	vertex_normal = normalInView.xyz;

	vec3 N = normalize(vertex_normal);
	vec3 V = -vertexInView.xyz;

	if(LightIdx==0)
		vertex_color = directionalLight(N, V);

	if(isEye == 1) {
        texCoord = aTexCoord;
    }else{
        texCoord = aTexCoord + offset;
    }

	_aPos = aPos;
	gl_Position = um4p * um4v * um4m * vec4(aPos, 1.0);
}
