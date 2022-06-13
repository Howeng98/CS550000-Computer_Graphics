#version 330 core

out vec4 FragColor;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec3 V_color;

uniform mat4 mvp;
uniform int pixel_vertex;
uniform mat4 view;
uniform int LightIdx;

struct LightInfo{
	vec3 position;
	vec3 spotDirection;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
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
	float shininess;
};
uniform MaterialInfo material;

float dot(vec3 u, vec3 v) {
	return u.x * v.x + u.y * v.y + u.z * v.z;
}

vec3 directionalLight(vec3 N, vec3 V){

	vec4 lightInView = view * vec4(light[0].position, 1.0f);
	vec3 S = normalize(lightInView.xyz + V);		 
	vec3 H = normalize(S + V);

	float dot_v = dot(N, S);
	float pow_v = pow(max(dot(N, H), 0), material.shininess);
	
	return light[0].ambient * material.Ka + dot_v * light[0].diffuse * material.Kd + pow_v * light[0].specular * material.Ks;
}

vec3 pointLight(vec3 N, vec3 V){
	vec4 lightInView = view * vec4(light[1].position, 1.0f);
	vec3 S = normalize(lightInView.xyz + V);		 
	vec3 H = normalize(S + V);

	float dot_v = dot(N, S);
	float pow_v = pow(max(dot(N, H), 0), material.shininess);

	vec4 vertexInView = mvp * vec4(V_color.x, V_color.y, V_color.z, 1.0);

	float distance = length(vertexInView.xyz - lightInView.xyz);
	float f = min(1.0 / (light[1].constantAttenuation + light[1].linearAttenuation * distance + light[1].quadraticAttenuation * distance * distance), 1.0);
	
	return light[1].ambient * material.Ka + f * (dot_v * light[1].diffuse * material.Kd + pow_v * light[1].specular * material.Ks);
}

vec3 spotLight(vec3 N, vec3 V){
	vec4 lightInView = view * vec4(light[2].position, 1.0f);
	vec3 S = normalize(lightInView.xyz + V);		 
	vec3 H = normalize(S + V);

	float dot_v = dot(N, S);
	float pow_v = pow(max(dot(N, H), 0), material.shininess);

	vec4 vertexInView = mvp * vec4(V_color.x, V_color.y, V_color.z, 1.0);

	float distance = length(vertexInView.xyz - lightInView.xyz);
	float f = min(1.0 / (light[2].constantAttenuation + light[2].linearAttenuation * distance + light[2].quadraticAttenuation * distance * distance), 1.0);
	
	float spot = dot(normalize(vertexInView.xyz - light[2].position), normalize(light[2].spotDirection));
    float spotLightEffect = (spot > cos(light[2].spotCutoff * 0.0174532925)) ? pow(max(spot, 0), light[2].spotExponent) : 0.0;

	return light[2].ambient * material.Ka + spotLightEffect * f * (dot_v * light[2].diffuse * material.Kd + pow_v * light[2].specular * material.Ks);
}

void main() {
	vec3 color = vec3(0, 0, 0);

	vec4 vertexInView = mvp * vec4(V_color.x, V_color.y, V_color.z, 1.0);

	vec3 N = normalize(vertex_normal);
	vec3 V = -vertexInView.xyz;

	if(LightIdx == 0)
			color = directionalLight(N, V);
	else if (LightIdx == 1)
			color = pointLight(N, V);
	else if (LightIdx == 2)
			color = spotLight(N, V);

	if (pixel_vertex == 0)
		FragColor = vec4(vertex_color, 1.0f);
	else
		FragColor = vec4(color, 1.0f);

}