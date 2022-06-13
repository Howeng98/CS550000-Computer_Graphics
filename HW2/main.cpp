#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

using namespace std;

#define PI 3.14159

// Default window size
const int WINDOW_WIDTH = 600;
const int WINDOW_HEIGHT = 600;

bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;


enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	LightEdit = 3,
	ShininessEdit = 4,
};

struct Uniform
{
	GLint iLocMVP;
};
Uniform uniform;

vector<string> filenames; // .obj filename list

struct PhongMaterial
{
	Vector3 Ka;
	Vector3 Kd;
	Vector3 Ks;
	GLfloat shininess;
};

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	PhongMaterial material;
	int indexCount;
	GLuint m_texture;
} Shape;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form

	vector<Shape> shapes;
};
vector<model> models;

struct camera
{
	Vector3 Position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};
ProjMode cur_proj_mode = Orthogonal;
TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix;
Matrix4 project_matrix;

int cur_idx = 0; // represent which model should be rendered now

// Requirement Lighting Attribute
int pixel_vertex = 0;
int LightIdx = 0;


GLint iLocLightIdx;
GLint iLocKa;
GLint iLocKd;
GLint iLocKs;
GLint iLocView;
GLint iLocShininess;

struct LightSource_Object {

	// Lightning Attribute
	GLint Position;
	GLint Direction;

	// Intensity
	GLint Ambient;
	GLint Diffuse;
	GLint Specular;

	// 
	GLint spotCutoff;
	GLint spotExponent;

	// Attenuation
	GLint constantAttenuation;
	GLint linearAttenuation;
	GLint quadraticAttenuation;

} LightSource_Object[3];


struct LightInfo {

	//
	Vector3 Position;
	Vector3 spotDirection;
	Vector3 Ambient;
	Vector3 Diffuse;
	Vector3 Specular;

	//
	float spotExponent;
	float spotCutoff;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
	
} LightInfo[3];


static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l;

	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{

	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}


// [TODO] given a translation vector then output a Matrix4 (Translation Matrix) // DONE
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, vec.x,
		0, 1, 0, vec.y,
		0, 0, 1, vec.z,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix) // DONE
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1
	);

	return mat;
}


// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X) // DONE
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, 0,
		0, cos(val), -sin(val), 0,
		0, sin(val), cos(val), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y) // DONE
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), 0, sin(val), 0,
		0, 1, 0, 0,
		-sin(val), 0, cos(val), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z) // DONE
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), -sin(val), 0, 0,
		sin(val), cos(val), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera // DONE
void setViewingMatrix()
{
	Vector3 P1P2 = main_camera.center - main_camera.Position;
	Vector3 P1P3 = main_camera.up_vector - main_camera.Position;
	GLfloat p1p2[3] = { P1P2.x, P1P2.y, P1P2.z };
	GLfloat p1p3[3] = { P1P3.x, P1P3.y, P1P3.z };
	GLfloat Rx[3], Ry[3], Rz[3];

	Rz[0] = -p1p2[0];
	Rz[1] = -p1p2[1];
	Rz[2] = -p1p2[2];

	Cross(p1p2, p1p3, Rx);
	Cross(Rz, Rx, Ry);

	Normalize(Rx);
	Normalize(Ry);
	Normalize(Rz);

	Matrix4 R = Matrix4(
		Rx[0], Rx[1], Rx[2], 0,
		Ry[0], Ry[1], Ry[2], 0,
		Rz[0], Rz[1], Rz[2], 0,
		0, 0, 0, 1
	);
	Matrix4 T = Matrix4(
		1, 0, 0, -main_camera.Position.x,
		0, 1, 0, -main_camera.Position.y,
		0, 0, 1, -main_camera.Position.z,
		0, 0, 0, 1
	);
	view_matrix = R * T;
}

// Compute orthogonal projection model (Not used in this HW)
void setOrthogonal()
{
	cur_proj_mode = Orthogonal;

	project_matrix = Matrix4(
		2.0 / (proj.right - proj.left), 0, 0, -((proj.right + proj.left) / (proj.right - proj.left)),
		0, 2.0 / (proj.top - proj.bottom), 0, -((proj.top + proj.bottom) / (proj.top - proj.bottom)),
		0, 0, -2.0 / (proj.farClip - proj.nearClip), -((proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip)),
		0, 0, 0, 1.0
	);
}

// [TODO] compute persepective projection matrix // DONE
void setPerspective()
{
	cur_proj_mode = Perspective;
	GLfloat f = -cos(proj.fovy / 2) / sin(proj.fovy / 2);
	project_matrix = Matrix4(
		f / proj.aspect, 0, 0, 0,
		0, f, 0, 0,
		0, 0, (proj.farClip + proj.nearClip) / (proj.nearClip - proj.farClip), (2 * proj.farClip * proj.nearClip) / (proj.nearClip - proj.farClip),
		0, 0, -1, 0
	);
}

void setGLMatrix(GLfloat* glm, Matrix4& m) {
	glm[0] = m[0];  glm[4] = m[1];   glm[8] = m[2];    glm[12] = m[3];
	glm[1] = m[4];  glm[5] = m[5];   glm[9] = m[6];    glm[13] = m[7];
	glm[2] = m[8];  glm[6] = m[9];   glm[10] = m[10];   glm[14] = m[11];
	glm[3] = m[12];  glm[7] = m[13];  glm[11] = m[14];   glm[15] = m[15];
}

// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	glViewport(width, 0, width, height);
	// [TODO] change your aspect ratio  // DONE
	proj.aspect = (float)width / (float)height;
	float aspect_ratio = (float)width / (float)height;

	// Update your project matrix with current aspect_ratio instead of proj.aspect
	GLfloat f = -cos(proj.fovy / 2) / sin(proj.fovy / 2);
	project_matrix = Matrix4(
		f / aspect_ratio, 0, 0, 0,
		0, f, 0, 0,
		0, 0, (proj.farClip + proj.nearClip) / (proj.nearClip - proj.farClip), (2.0 * proj.farClip * proj.nearClip) / (proj.nearClip - proj.farClip),
		0, 0, -1, 0
	);
}

void updateLight() {

	glUniform1i(iLocLightIdx, LightIdx);

	// Directional light
	glUniform3f(LightSource_Object[0].Position, LightInfo[0].Position.x, LightInfo[0].Position.y, LightInfo[0].Position.z);
	glUniform3f(LightSource_Object[0].Ambient,  LightInfo[0].Ambient.x,  LightInfo[0].Ambient.y,  LightInfo[0].Ambient.z);
	glUniform3f(LightSource_Object[0].Diffuse,  LightInfo[0].Diffuse.x,  LightInfo[0].Diffuse.y,  LightInfo[0].Diffuse.z);
	glUniform3f(LightSource_Object[0].Specular, LightInfo[0].Specular.x, LightInfo[0].Specular.y, LightInfo[0].Specular.z);

	// Point light
	glUniform3f(LightSource_Object[1].Position,  LightInfo[1].Position.x, LightInfo[1].Position.y, LightInfo[1].Position.z);
	glUniform3f(LightSource_Object[1].Ambient,   LightInfo[1].Ambient.x,  LightInfo[1].Ambient.y,  LightInfo[1].Ambient.z);
	glUniform3f(LightSource_Object[1].Diffuse,   LightInfo[1].Diffuse.x,  LightInfo[1].Diffuse.y,  LightInfo[1].Diffuse.z);
	glUniform3f(LightSource_Object[1].Specular,  LightInfo[1].Specular.x, LightInfo[1].Specular.y, LightInfo[1].Specular.z);
	glUniform1f(LightSource_Object[1].constantAttenuation,  LightInfo[1].constantAttenuation);
	glUniform1f(LightSource_Object[1].linearAttenuation,    LightInfo[1].linearAttenuation);
	glUniform1f(LightSource_Object[1].quadraticAttenuation, LightInfo[1].quadraticAttenuation);

	// Spot light
	glUniform3f(LightSource_Object[2].Position,  LightInfo[2].Position.x,      LightInfo[2].Position.y,      LightInfo[2].Position.z);
	glUniform3f(LightSource_Object[2].Diffuse,   LightInfo[2].Diffuse.x,       LightInfo[2].Diffuse.y,       LightInfo[2].Diffuse.z);
	glUniform3f(LightSource_Object[2].Ambient,   LightInfo[2].Ambient.x,       LightInfo[2].Ambient.y,       LightInfo[2].Ambient.z);
	glUniform3f(LightSource_Object[2].Specular,  LightInfo[2].Specular.x,      LightInfo[2].Specular.y,      LightInfo[2].Specular.z);
	glUniform3f(LightSource_Object[2].Direction, LightInfo[2].spotDirection.x, LightInfo[2].spotDirection.y, LightInfo[2].spotDirection.z);
	glUniform1f(LightSource_Object[2].spotExponent,  LightInfo[2].spotExponent);
	glUniform1f(LightSource_Object[2].spotCutoff,    LightInfo[2].spotCutoff);
	glUniform1f(LightSource_Object[2].constantAttenuation,  LightInfo[2].constantAttenuation);
	glUniform1f(LightSource_Object[2].linearAttenuation,    LightInfo[2].linearAttenuation);
	glUniform1f(LightSource_Object[2].quadraticAttenuation, LightInfo[2].quadraticAttenuation);
}

// Render function for display rendering
void RenderScene(void) {	
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S;
	// [TODO] update translation, rotation and scaling // DONE
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);

	Matrix4 MVP;
	GLfloat mvp[16];

	// [TODO] multiply all the matrix // DONE
	MVP = project_matrix * view_matrix * T * R * S;

	// row-major ---> column-major
	setGLMatrix(mvp, MVP);

	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(uniform.iLocMVP, 1, GL_FALSE, mvp);

	updateLight();

	// per-Pixel lighting
	glUniform1i(pixel_vertex, 0);
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	for (int i = 0; i < models[cur_idx].shapes.size(); i++)
	{
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
		glUniformMatrix4fv(iLocView, 1, GL_FALSE, view_matrix.getTranspose());
	}

	// per-Vertex lighting
	glUniform1i(pixel_vertex, 1);
	glViewport(WINDOW_WIDTH, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	for (int i = 0; i < models[cur_idx].shapes.size(); i++)
	{
		PhongMaterial mat = models[cur_idx].shapes[i].material;
		glUniform3f(iLocKa, mat.Ka.x, mat.Ka.y, mat.Ka.z);
		glUniform3f(iLocKd, mat.Kd.x, mat.Kd.y, mat.Kd.z);
		glUniform3f(iLocKs, mat.Ks.x, mat.Ks.y, mat.Ks.z);
		// shininess
		glUniform1f(iLocShininess, mat.shininess);


		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}
}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if (action == GLFW_PRESS) {
		// Actions depends on pressed key
		switch (key) {
			case GLFW_KEY_Z:
				cur_idx = (cur_idx == 0) ? 4 : cur_idx - 1;
				cout << "Switch to previous model" << endl;
				break;
			case GLFW_KEY_X:
				cur_idx = (cur_idx == 4) ? 0 : cur_idx + 1;
				cout << "Switch to next model" << endl;
				break;
			case GLFW_KEY_T:
				cur_trans_mode = GeoTranslation;
				cout << "Translation mode" << endl;
				break;
			case GLFW_KEY_S:
				cur_trans_mode = GeoScaling;
				cout << "Scaling mode" << endl;
				break;
			case GLFW_KEY_R:
				cur_trans_mode = GeoRotation;
				cout << "Rotation mdoe" << endl;
				break;
			case GLFW_KEY_L:
				LightIdx = (LightIdx == 2) ? 0 : LightIdx + 1;
				if (LightIdx == 0)
					cout << "Directional Light" << endl;
				else if (LightIdx == 1)
					cout << "Point Light" << endl;
				else if (LightIdx == 2)
					cout << "Spot Light" << endl;
				break;
			case GLFW_KEY_K:
				cur_trans_mode = LightEdit;
				break;
			case GLFW_KEY_J:
				cur_trans_mode = ShininessEdit;
				break;
		}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
	double offset = yoffset - xoffset;
	switch (cur_trans_mode) {
		case GeoTranslation:
			models[cur_idx].position.z += 0.1 * offset;
			break;
		case GeoRotation:
			models[cur_idx].rotation.z += 0.1 * offset;
			break;
		case GeoScaling:
			models[cur_idx].scale.z += 0.1 * offset;
			break;
		case LightEdit:
			if (LightIdx == 0){
				LightInfo[LightIdx].Diffuse -= Vector3(0.05, 0.05, 0.105) * yoffset;
				if (LightInfo[LightIdx].Diffuse.x < 0.0) 
					LightInfo[LightIdx].Diffuse = Vector3(0.0, 0.0, 0.0);
				if (LightInfo[LightIdx].Diffuse.x > 1.0) 
					LightInfo[LightIdx].Diffuse = Vector3(1.0, 1.0, 1.0);
			}
			else if (LightIdx == 1) {
				LightInfo[LightIdx].Diffuse -= Vector3(0.05, 0.05, 0.105) * yoffset;
				if (LightInfo[LightIdx].Diffuse.x < 0.0)
					LightInfo[LightIdx].Diffuse = Vector3(0.0, 0.0, 0.0);
				if (LightInfo[LightIdx].Diffuse.x > 1.0)
					LightInfo[LightIdx].Diffuse = Vector3(1.0, 1.0, 1.0);
			}
			else if (LightIdx == 2) {
				LightInfo[LightIdx].spotCutoff -= 1.0 * yoffset;
				if (LightInfo[LightIdx].spotCutoff < 0.0) 
					LightInfo[LightIdx].spotCutoff = 0.0;
				if (LightInfo[LightIdx].spotCutoff > 90.0) 
					LightInfo[LightIdx].spotCutoff = 90.0;
			}
			break;
		case ShininessEdit:
			for (int i = 0; i < 5; i++)
				for (int j = 0; j < models[i].shapes.size(); j++)
					models[i].shapes[j].material.shininess += 5 * offset;
			break;
		default:
			break;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
	if (action == GLFW_PRESS) {
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			mouse_pressed = true;
			cout << "Left button pressed" << endl;
		}
		else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			mouse_pressed = true;
			cout << "Right button pressed" << endl;
		}
	}
	else if (action == GLFW_RELEASE) {
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			mouse_pressed = false;
			cout << "Left button release" << endl;
		}
		else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			mouse_pressed = false;
			cout << "Right button release" << endl;
		}
	}	
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] cursor Position callback function
	if (mouse_pressed) {
		double x_diff = xpos - starting_press_x;
		double y_diff = ypos - starting_press_y;

		switch (cur_trans_mode) {
			case GeoTranslation:
				models[cur_idx].position.x += 0.005 * x_diff;
				models[cur_idx].position.y -= 0.005 * y_diff;
				break;
			case GeoRotation:
				models[cur_idx].rotation.x += PI / 180.0 * y_diff * 0.125;
				models[cur_idx].rotation.y += PI / 180.0 * x_diff * 0.125;
				break;
			case GeoScaling:
				models[cur_idx].scale.x += 0.005 * x_diff;
				models[cur_idx].scale.y += 0.005 * y_diff;
				break;
			case LightEdit:
				// Change light Position
				LightInfo[LightIdx].Position.x += 0.003 * x_diff;
				LightInfo[LightIdx].Position.y -= 0.003 * y_diff;
				break;
		}
	}

	// Update current x,y pos to starting_press x,y
	starting_press_x = (int)xpos;
	starting_press_y = (int)ypos;
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p,f);
	glAttachShader(p,v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);

	uniform.iLocMVP = glGetUniformLocation(p, "mvp");

	//
	pixel_vertex = glGetUniformLocation(p, "pixel_vertex");
	iLocView = glGetUniformLocation(p, "view");
	iLocLightIdx = glGetUniformLocation(p, "LightIdx");

	// Materials
	iLocKa = glGetUniformLocation(p, "material.Ka");
	iLocKd = glGetUniformLocation(p, "material.Kd");
	iLocKs = glGetUniformLocation(p, "material.Ks");
	iLocShininess = glGetUniformLocation(p, "material.shininess");

	// Directional light
	LightSource_Object[0].Position = glGetUniformLocation(p, "light[0].position");
	LightSource_Object[0].Ambient = glGetUniformLocation(p, "light[0].ambient");
	LightSource_Object[0].Diffuse = glGetUniformLocation(p, "light[0].diffuse");
	LightSource_Object[0].Specular = glGetUniformLocation(p, "light[0].specular");
	LightSource_Object[0].Direction = glGetUniformLocation(p, "light[0].spotDirection");
	LightSource_Object[0].spotCutoff = glGetUniformLocation(p, "light[0].spotCutoff");
	LightSource_Object[0].spotExponent = glGetUniformLocation(p, "light[0].spotExponent");
	LightSource_Object[0].constantAttenuation = glGetUniformLocation(p, "light[0].constantAttenuation");
	LightSource_Object[0].linearAttenuation = glGetUniformLocation(p, "light[0].linearAttenuation");
	LightSource_Object[0].quadraticAttenuation = glGetUniformLocation(p, "light[0].quadraticAttenuation");

	// Point light
	LightSource_Object[1].Position = glGetUniformLocation(p, "light[1].position");
	LightSource_Object[1].Ambient = glGetUniformLocation(p, "light[1].ambient");
	LightSource_Object[1].Diffuse = glGetUniformLocation(p, "light[1].diffuse");
	LightSource_Object[1].Specular = glGetUniformLocation(p, "light[1].specular");
	LightSource_Object[1].Direction = glGetUniformLocation(p, "light[1].spotDirection");
	LightSource_Object[1].spotCutoff = glGetUniformLocation(p, "light[1].spotCutoff");
	LightSource_Object[1].spotExponent = glGetUniformLocation(p, "light[1].spotExponent");
	LightSource_Object[1].constantAttenuation = glGetUniformLocation(p, "light[1].constantAttenuation");
	LightSource_Object[1].linearAttenuation = glGetUniformLocation(p, "light[1].linearAttenuation");
	LightSource_Object[1].quadraticAttenuation = glGetUniformLocation(p, "light[1].quadraticAttenuation");

	// Spot light
	LightSource_Object[2].Position = glGetUniformLocation(p, "light[2].position");
	LightSource_Object[2].Ambient = glGetUniformLocation(p, "light[2].ambient");
	LightSource_Object[2].Diffuse = glGetUniformLocation(p, "light[2].diffuse");
	LightSource_Object[2].Specular = glGetUniformLocation(p, "light[2].specular");
	LightSource_Object[2].Direction = glGetUniformLocation(p, "light[2].spotDirection");
	LightSource_Object[2].spotCutoff = glGetUniformLocation(p, "light[2].spotCutoff");
	LightSource_Object[2].spotExponent = glGetUniformLocation(p, "light[2].spotExponent");
	LightSource_Object[2].constantAttenuation = glGetUniformLocation(p, "light[2].constantAttenuation");
	LightSource_Object[2].linearAttenuation = glGetUniformLocation(p, "light[2].linearAttenuation");
	LightSource_Object[2].quadraticAttenuation = glGetUniformLocation(p, "light[2].quadraticAttenuation");

	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, vector<GLfloat>& normals, tinyobj::shape_t* shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++) {
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++) {
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
			// Optional: vertex normals
			if (idx.normal_index >= 0) {
				normals.push_back(attrib->normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 2]);
			}
		}
		index_offset += fv;
	}
}

string GetBaseDir(const string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;
	vector<GLfloat> normals;

	string err;
	string warn;

	string base_dir = GetBaseDir(model_path); // handle .mtl with relative path

#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str(), base_dir.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Material size %d\n", int(shapes.size()), int(materials.size()));
	model tmp_model;

	vector<PhongMaterial> allMaterial;
	for (int i = 0; i < materials.size(); i++)
	{
		PhongMaterial material;
		material.Ka = Vector3(materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
		material.Kd = Vector3(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
		material.Ks = Vector3(materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
		allMaterial.push_back(material);
	}

	for (int i = 0; i < shapes.size(); i++)
	{

		vertices.clear();
		colors.clear();
		normals.clear();
		normalization(&attrib, vertices, colors, normals, &shapes[i]);
		// printf("Vertices size: %d", vertices.size() / 3);

		Shape tmp_shape;
		glGenVertexArrays(1, &tmp_shape.vao);
		glBindVertexArray(tmp_shape.vao);

		glGenBuffers(1, &tmp_shape.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		tmp_shape.vertex_count = vertices.size() / 3;

		glGenBuffers(1, &tmp_shape.p_color);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &tmp_shape.p_normal);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_normal);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GL_FLOAT), &normals.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		// not support per face material, use material of first face
		if (allMaterial.size() > 0)
			tmp_shape.material = allMaterial[shapes[i].mesh.material_ids[0]];
		tmp_model.shapes.push_back(tmp_shape);
	}
	shapes.clear();
	materials.clear();
	models.push_back(tmp_model);
}

void initParameter()
{
	// [TODO] Setup some parameters if you need
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.Position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	// Direational light
	LightInfo[0].Position = Vector3(1.0f, 1.0f, 1.0f);
	LightInfo[0].Ambient = Vector3(0.15f, 0.15f, 0.15f);
	LightInfo[0].Diffuse = Vector3(1.0f, 1.0f, 1.0f);
	LightInfo[0].Specular = Vector3(1.0f, 1.0f, 1.0f);

	// Point light
	LightInfo[1].Position = Vector3(0.0f, 2.0f, 1.0f);
	LightInfo[1].Ambient = Vector3(0.15f, 0.15f, 0.15f);
	LightInfo[1].Diffuse = Vector3(1.0f, 1.0f, 1.0f);
	LightInfo[1].Specular = Vector3(1.0f, 1.0f, 1.0f);
	LightInfo[1].constantAttenuation = 0.01f;
	LightInfo[1].linearAttenuation = 0.8f;
	LightInfo[1].quadraticAttenuation = 0.1f;

	// Spot light
	LightInfo[2].Position = Vector3(0.0f, 0.0f, 2.0f);
	LightInfo[2].Ambient = Vector3(0.15f, 0.15f, 0.15f);
	LightInfo[2].Diffuse = Vector3(1.0f, 1.0f, 1.0f);
	LightInfo[2].Specular = Vector3(1.0f, 1.0f, 1.0f);
	LightInfo[2].spotDirection = Vector3(0.0f, 0.0f, -1.0f);
	LightInfo[2].spotExponent = 50;
	LightInfo[2].spotCutoff = 30;
	LightInfo[2].constantAttenuation = 0.05f;
	LightInfo[2].linearAttenuation = 0.3f;
	LightInfo[2].quadraticAttenuation = 0.6f;

	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);

	vector<string> model_list{ 
		"../NormalModels/al7KN.obj", 
		"../NormalModels/cessna7KN.obj", 
		"../NormalModels/cow3KN.obj", 
		"../NormalModels/duck4KN.obj", 
		"../NormalModels/dolphinN.obj"
	};

	// [TODO] Load five model at here  // DONE
	for (int model_idx = 0; model_idx < model_list.size(); model_idx++) {
		LoadModels(model_list[model_idx]);
	}
	for (int i = 0; i < 5; i++)
		for (int j = 0; j < models[i].shapes.size(); j++)
			models[i].shapes[j].material.shininess = 64;
	
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char*)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char*)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char*)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char*)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}


int main(int argc, char **argv)
{
    // initial glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif

    
    // create window
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH * 2, WINDOW_HEIGHT, "110062401 HW2", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    
    // load OpenGL function pointer
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
	// register glfw callback functions
    glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
    while (!glfwWindowShouldClose(window))
    {
        // render
        RenderScene();
        
        // swap buffer from back to front
        glfwSwapBuffers(window);
        
        // Poll input event
        glfwPollEvents();
    }
	
	// just for compatibiliy purposes
	return 0;
}
