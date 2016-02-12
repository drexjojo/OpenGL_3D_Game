#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <stdlib.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>
#include <FTGL/ftgl.h>
#include <GLFW/glfw3.h>
#include <SOIL/SOIL.h>

using namespace std;

GLfloat fov = 70;

struct VAO {
	GLuint VertexArrayID;
	GLuint VertexBuffer;
	GLuint ColorBuffer;
	GLuint TextureBuffer;
	GLuint TextureID;

	GLenum PrimitiveMode; // GL_POINTS, GL_LINE_STRIP, GL_LINE_LOOP, GL_LINES, GL_LINE_STRIP_ADJACENCY, GL_LINES_ADJACENCY, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_TRIANGLES, GL_TRIANGLE_STRIP_ADJACENCY and GL_TRIANGLES_ADJACENCY
	GLenum FillMode; // GL_FILL, GL_LINE
	int NumVertices;
};
typedef struct VAO VAO;

struct CUBE {
	GLfloat posx ,posy,posz;
	GLint direction;
	bool moving,missing;
	VAO *vao,*edges;
};
typedef struct CUBE CUBE;

struct SEA {
	GLfloat posx ,posy,posz;
	VAO *vao;
};
typedef struct SEA SEA;

struct PLAYER {
	GLfloat posx ,posy,posz;
	GLfloat radius,velx,vely,velz;
	VAO *vao;
};
typedef struct CUBE CUBE;

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID; // For use with normal shader
	GLuint TexMatrixID; // For use with texture shader
} Matrices;

struct FTGLFont {
	FTFont* font;
	GLuint fontMatrixID;
	GLuint fontColorID;
} GL3Font;

GLuint programID, fontProgramID, textureProgramID;

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	cout << "Compiling shader : " <<  vertex_file_path << endl;
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage( max(InfoLogLength, int(1)) );
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	cout << VertexShaderErrorMessage.data() << endl;

	// Compile Fragment Shader
	cout << "Compiling shader : " << fragment_file_path << endl;
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage( max(InfoLogLength, int(1)) );
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	cout << FragmentShaderErrorMessage.data() << endl;

	// Link the program
	cout << "Linking program" << endl;
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	cout << ProgramErrorMessage.data() << endl;

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description)
{
	cout << "Error: " << description << endl;
}

void quit(GLFWwindow *window)
{
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}

glm::vec3 getRGBfromHue (int hue)
{
	float intp;
	float fracp = modff(hue/60.0, &intp);
	float x = 1.0 - abs((float)((int)intp%2)+fracp-1.0);

	if (hue < 60)
		return glm::vec3(1,x,0);
	else if (hue < 120)
		return glm::vec3(x,1,0);
	else if (hue < 180)
		return glm::vec3(0,1,x);
	else if (hue < 240)
		return glm::vec3(0,x,1);
	else if (hue < 300)
		return glm::vec3(x,0,1);
	else
		return glm::vec3(1,0,x);
}

/* Generate VAO, VBOs and return VAO handle */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, GLenum fill_mode=GL_FILL)
{
	struct VAO* vao = new struct VAO;
	vao->PrimitiveMode = primitive_mode;
	vao->NumVertices = numVertices;
	vao->FillMode = fill_mode;

	// Create Vertex Array Object
	// Should be done after CreateWindow and before any other GL calls

	glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
	glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
	glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

	glBindVertexArray (vao->VertexArrayID); // Bind the VAO
	glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
	glVertexAttribPointer(
						  0,                  // attribute 0. Vertices
						  3,                  // size (x,y,z)
						  GL_FLOAT,           // type
						  GL_FALSE,           // normalized?
						  0,                  // stride
						  (void*)0            // array buffer offset
						  );

	glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
	glVertexAttribPointer(
						  1,                  // attribute 1. Color
						  3,                  // size (r,g,b)
						  GL_FLOAT,           // type
						  GL_FALSE,           // normalized?
						  0,                  // stride
						  (void*)0            // array buffer offset
						  );

	return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue, GLenum fill_mode=GL_FILL)
{
	GLfloat* color_buffer_data = new GLfloat [3*numVertices];
	for (int i=0; i<numVertices; i++) {
		color_buffer_data [3*i] = red;
		color_buffer_data [3*i + 1] = green;
		color_buffer_data [3*i + 2] = blue;
	}

	return create3DObject(primitive_mode, numVertices, vertex_buffer_data, color_buffer_data, fill_mode);
}

struct VAO* create3DTexturedObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* texture_buffer_data, GLuint textureID, GLenum fill_mode=GL_FILL)
{
	struct VAO* vao = new struct VAO;
	vao->PrimitiveMode = primitive_mode;
	vao->NumVertices = numVertices;
	vao->FillMode = fill_mode;
	vao->TextureID = textureID;

	// Create Vertex Array Object
	// Should be done after CreateWindow and before any other GL calls
	glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
	glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
	glGenBuffers (1, &(vao->TextureBuffer));  // VBO - textures

	glBindVertexArray (vao->VertexArrayID); // Bind the VAO
	glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
	glVertexAttribPointer(
						  0,                  // attribute 0. Vertices
						  3,                  // size (x,y,z)
						  GL_FLOAT,           // type
						  GL_FALSE,           // normalized?
						  0,                  // stride
						  (void*)0            // array buffer offset
						  );

	glBindBuffer (GL_ARRAY_BUFFER, vao->TextureBuffer); // Bind the VBO textures
	glBufferData (GL_ARRAY_BUFFER, 2*numVertices*sizeof(GLfloat), texture_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
	glVertexAttribPointer(
						  2,                  // attribute 2. Textures
						  2,                  // size (s,t)
						  GL_FLOAT,           // type
						  GL_FALSE,           // normalized?
						  0,                  // stride
						  (void*)0            // array buffer offset
						  );

	return vao;
}

/* Render the VBOs handled by VAO */
void draw3DObject (struct VAO* vao)
{
	// Change the Fill Mode for this object
	glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);


	// Bind the VAO to use
	glBindVertexArray (vao->VertexArrayID);

	// Enable Vertex Attribute 0 - 3d Vertices
	glEnableVertexAttribArray(0);
	// Bind the VBO to use

	glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

	// Enable Vertex Attribute 1 - Color
	glEnableVertexAttribArray(1);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

	// Draw the geometry !
	glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

void draw3DTexturedObject (struct VAO* vao)
{
	// Change the Fill Mode for this object
	glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

	// Bind the VAO to use
	glBindVertexArray (vao->VertexArrayID);

	// Enable Vertex Attribute 0 - 3d Vertices
	glEnableVertexAttribArray(0);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

	// Bind Textures using texture units
	glBindTexture(GL_TEXTURE_2D, vao->TextureID);

	// Enable Vertex Attribute 2 - Texture
	glEnableVertexAttribArray(2);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->TextureBuffer);

	// Draw the geometry !
	glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle

	// Unbind Textures to be safe
	glBindTexture(GL_TEXTURE_2D, 0);
}

/* Create an OpenGL Texture from an image */
GLuint createTexture (const char* filename)
{
	GLuint TextureID;
	// Generate Texture Buffer
	glGenTextures(1, &TextureID);
	// All upcoming GL_TEXTURE_2D operations now have effect on our texture buffer
	glBindTexture(GL_TEXTURE_2D, TextureID);
	// Set our texture parameters
	// Set texture wrapping to GL_REPEAT
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Set texture filtering (interpolation)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Load image and create OpenGL texture
	int twidth, theight;
	unsigned char* image = SOIL_load_image(filename, &twidth, &theight, 0, SOIL_LOAD_RGB);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, twidth, theight, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D); // Generate MipMaps to use
	SOIL_free_image_data(image); // Free the data read from file after creating opengl texture
	glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture when done, so we won't accidentily mess it up

	return TextureID;
}


/**************************
 * Customizable functions *
 **************************/

float triangle_rot_dir = 1;
float rectangle_rot_dir = -1;
bool triangle_rot_status = true;
bool rectangle_rot_status = true;
GLfloat eyex ,eyey ,eyez ,tarx,tary,tarz;
bool topview = false;
bool playerview = true;
bool towerview =false;
VAO *axises;
CUBE cubes[100];
PLAYER player;
SEA sea[1000];

/* Executed when a regular key is pressed/released/held-down */
/* Prefered for Keyboard events */

void changeview()
{
	if(towerview == true)
	{
		towerview = false;
		playerview = false;
		topview = true;
	}
	else if (topview == true)
	{
		topview = false;
		playerview = true;
		towerview = false;
	}
	else if (playerview == true)
	{
		topview = false;
		playerview = false;
		towerview = true;
	}
}
void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Function is called first on GLFW_PRESS.

	if (action == GLFW_PRESS) {
		switch (key) {
			case GLFW_KEY_ESCAPE:
				quit(window);
				break;
			case GLFW_KEY_T:
				changeview();
				break;
			case GLFW_KEY_UP:
				player.posz -= 0.2;
				if(player.posz < cubes[99].posz)
					player.posz = cubes[99].posz;
				break;
			case GLFW_KEY_DOWN:
				player.posz += 0.2;
				if(player.posz > cubes[1].posz)
					player.posz = cubes[1].posz;
				break;
			case GLFW_KEY_LEFT:
				player.posx -= 0.2;
				if(player.posx < cubes[0].posx)
					player.posx = cubes[0].posx;
				break;
			case GLFW_KEY_RIGHT:
				player.posx += 0.2;
				if(player.posx > cubes[9].posx)
					player.posx = cubes[9].posx;
				break;

			default:
				break;
		}
	}
	else if (action == GLFW_REPEAT) {
        switch (key) {
            case GLFW_KEY_UP:
				player.posz -= 0.2;
				if(player.posz < cubes[99].posz)
					player.posz = cubes[99].posz;
				break;
			case GLFW_KEY_DOWN:
				player.posz += 0.2;
				if(player.posz > cubes[1].posz)
					player.posz = cubes[1].posz;
				break;
			case GLFW_KEY_LEFT:
				player.posx -= 0.2;
				if(player.posx < cubes[0].posx)
					player.posx = cubes[0].posx;
				break;
			case GLFW_KEY_RIGHT:
				player.posx += 0.2;
				if(player.posx > cubes[9].posx)
					player.posx = cubes[9].posx;
				break;

			default:
				break;
        }
    }
}

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
	switch (key) {
		case 'Q':
		case 'q':
			quit(window);
			break;
		default:
			break;
	}
}

/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
	switch (button) {
		case GLFW_MOUSE_BUTTON_LEFT:
			if (action == GLFW_RELEASE)
				triangle_rot_dir *= -1;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			if (action == GLFW_RELEASE) {
				rectangle_rot_dir *= -1;
			}
			break;
		default:
			break;
	}
}


/* Executed when window is resized to 'width' and 'height' */
/* Modify the bounds of the screen here in glm::ortho or Field of View in glm::Perspective */
void reshapeWindow (GLFWwindow* window, int width, int height)
{
	int fbwidth=width, fbheight=height;
	/* With Retina display on Mac OS X, GLFW's FramebufferSize
	 is different from WindowSize */
	glfwGetFramebufferSize(window, &fbwidth, &fbheight);

	// sets the viewport of openGL renderer
	glViewport (0, 0, (GLsizei) fbwidth, (GLsizei) fbheight);

	// set the projection matrix as perspective
	/* glMatrixMode (GL_PROJECTION);
	 glLoadIdentity ();
	 gluPerspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1, 500.0); */
	// Store the projection matrix in a variable for future use
	// Perspective projection for 3D views
	Matrices.projection = glm::perspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 1.0f, 300.0f);

	// Ortho projection for 2D views
	//Matrices.projection = glm::ortho(-4.0f, 4.0f, -4.0f, 4.0f, 0.1f, 500.0f);
}


// Creates the triangle object used in this sample code
void createaxis ()
{
	/* ONLY vertices between the bounds specified in glm::ortho will be visible on screen */

	/* Define vertex array as used in glBegin (GL_TRIANGLES) */
	static const GLfloat vertex_buffer_data [] = {
		-10,0,0, // vertex 0
		10,0,0, // vertex 1
		0,-10,0, // vertex 2
		0,10,0,
		0,0,-10,
		0,0,10
	};

	static const GLfloat color_buffer_data [] = {
		1,0,0, // color 0
		1,0,0, // color 1
		0,0,1, // color 2
		0,0,1,
		0,1,0,
		0,1,0
	};

	// create3DObject creates and returns a handle to a VAO that can be used later
	axises = create3DObject(GL_LINES, 6, vertex_buffer_data, color_buffer_data, GL_LINE);
}


// Creates the rectangle object used in this sample code

PLAYER makeplayer(PLAYER player,GLuint textureID)
{
	player.posx = cubes[0].posx;
	player.posy = cubes[0].posy+3.5+5;
	player.posz = cubes[0].posz;
	player.velx =0;
	player.vely =0;
	player.velz =0;
	player.radius = sqrt(0.64+0.64+0.64)/2;
	int length =2,width=2,height=2;
	static const GLfloat vertex_buffer_data [] = {
			 //left face
	    -length/2,height/2,width/2, // vertex 1
	    -length/2,-height/2,width/2, // vertex 2
	    -length/2,-height/2,-width/2, // vertex 3

	    -length/2,height/2,width/2, // vertex 1
	    -length/2,-height/2,-width/2, // vertex 3
	    -length/2,height/2,-width/2, //vertex 4

	    //front face
	    -length/2,height/2,width/2, // vertex 1
	    length/2,height/2,width/2, // vertex 5
	    -length/2,-height/2,width/2, // vertex 2

	    length/2,height/2,width/2, // vertex 5
	    -length/2,-height/2,width/2, // vertex 2
	    length/2,-height/2,width/2, //vertex 6
	    
	    //top face
	    -length/2,height/2,width/2, // vertex 1
	    length/2,height/2,width/2, // vertex 5
	    -length/2,height/2,-width/2, //vertex 4
	    
	    length/2,height/2,width/2, // vertex 5
	    -length/2,height/2,-width/2, //vertex 4
	    length/2,height/2,-width/2, // vertex 7

	    //right face
	    length/2,height/2,width/2, // vertex 5
	    length/2,-height/2,width/2, //vertex 6
	    length/2,height/2,-width/2, // vertex 7

	    length/2,-height/2,width/2, //vertex 6
	    length/2,height/2,-width/2, // vertex 7
	    length/2,-height/2,-width/2, //vertex 8

	    //Back face
	    -length/2,-height/2,-width/2, // vertex 3
	    -length/2,height/2,-width/2, //vertex 4
	    length/2,height/2,-width/2, // vertex 7

	    -length/2,-height/2,-width/2, // vertex 3
	    length/2,-height/2,-width/2, //vertex 8
	    length/2,height/2,-width/2, // vertex 7

	    //Bottom face
	    -length/2,-height/2,-width/2, // vertex 3
	    length/2,-height/2,-width/2, //vertex 8
	    length/2,-height/2,width/2, //vertex 6

	    -length/2,-height/2,-width/2, // vertex 3
	    -length/2,-height/2,width/2, // vertex 2
	    length/2,-height/2,width/2 //vertex 6
	};
	
	static const GLfloat texture_buffer_data [] = {
		0,1, 
		1,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 
	};
	player.vao = create3DTexturedObject(GL_TRIANGLES, 36, vertex_buffer_data, texture_buffer_data, textureID, GL_FILL);
	return player;
}

SEA create_sea(SEA sea ,GLuint textureID,GLfloat x,GLfloat y,GLfloat z){
	int length =2,width=2,height=2;
	sea.posx =x;
	sea.posy =y;
	sea.posz =z;
	static const GLfloat vertex_buffer_data [] = {
			 //left face
	    -length/2,height/2,width/2, // vertex 1
	    -length/2,-height/2,width/2, // vertex 2
	    -length/2,-height/2,-width/2, // vertex 3

	    -length/2,height/2,width/2, // vertex 1
	    -length/2,-height/2,-width/2, // vertex 3
	    -length/2,height/2,-width/2, //vertex 4

	    //front face
	    -length/2,height/2,width/2, // vertex 1
	    length/2,height/2,width/2, // vertex 5
	    -length/2,-height/2,width/2, // vertex 2

	    length/2,height/2,width/2, // vertex 5
	    -length/2,-height/2,width/2, // vertex 2
	    length/2,-height/2,width/2, //vertex 6
	    
	    //top face
	    -length/2,height/2,width/2, // vertex 1
	    length/2,height/2,width/2, // vertex 5
	    -length/2,height/2,-width/2, //vertex 4
	    
	    length/2,height/2,width/2, // vertex 5
	    -length/2,height/2,-width/2, //vertex 4
	    length/2,height/2,-width/2, // vertex 7

	    //right face
	    length/2,height/2,width/2, // vertex 5
	    length/2,-height/2,width/2, //vertex 6
	    length/2,height/2,-width/2, // vertex 7

	    length/2,-height/2,width/2, //vertex 6
	    length/2,height/2,-width/2, // vertex 7
	    length/2,-height/2,-width/2, //vertex 8

	    //Back face
	    -length/2,-height/2,-width/2, // vertex 3
	    -length/2,height/2,-width/2, //vertex 4
	    length/2,height/2,-width/2, // vertex 7

	    -length/2,-height/2,-width/2, // vertex 3
	    length/2,-height/2,-width/2, //vertex 8
	    length/2,height/2,-width/2, // vertex 7

	    //Bottom face
	    -length/2,-height/2,-width/2, // vertex 3
	    length/2,-height/2,-width/2, //vertex 8
	    length/2,-height/2,width/2, //vertex 6

	    -length/2,-height/2,-width/2, // vertex 3
	    -length/2,-height/2,width/2, // vertex 2
	    length/2,-height/2,width/2 //vertex 6
	};
	
	static const GLfloat texture_buffer_data [] = {
		0,1, 
		1,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 

		1,1, 
		0,1, 
		1,0, 

		0,1,  
		1,0, 
		0,0, 
	};
	sea.vao = create3DTexturedObject(GL_TRIANGLES, 36, vertex_buffer_data, texture_buffer_data, textureID, GL_FILL);
	return sea;
}
CUBE createcube (CUBE cube,float positionx,float positiony,float positionz)
{
	cube.posx = positionx;
	cube.posy = positiony; 
	cube.posz = positionz;
	cube.direction = 1;
	cube.moving = false;
	cube.missing = false;
	static const GLfloat vertex_buffer_data [] = {
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f, 1.0f, -1.0f,
		1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, 1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, -1.0f,
		1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, 1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, -1.0f,
		-1.0f, 1.0f, -1.0f,
		1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, -1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};
	
	static const GLfloat color_buffer_data [] = {
		0.36 ,0.25,0.20, //medium brown
		0.36 ,0.25,0.20,
		0.36 ,0.25,0.20,

		0.36 ,0.25,0.20, //medium brown somewhere
		0.36 ,0.25,0.20,
		0.36 ,0.25,0.20,

		0.36 ,0.25,0.20, //medium brown somewhere
		0.36 ,0.25,0.20,
		0.36 ,0.25,0.20,
		
		0.36 ,0.25,0.20, //medium brown somewhere
		0.36 ,0.25,0.20,
		0.36 ,0.25,0.20,

		0.36 ,0.25,0.20, //medium brown left side
		0.36 ,0.25,0.20,
		0.36 ,0.25,0.20,

		0.36 ,0.25,0.20, //medium brown somewhere
		0.36 ,0.25,0.20,
		0.36 ,0.25,0.20,

		0.35,0.16,0.14, //dark brown
		0.35,0.16,0.14,
		0.35,0.16,0.14,

		0.36 ,0.25,0.20, //medium brown right side
		0.36 ,0.25,0.20,
		0.36 ,0.25,0.20,

		0.36 ,0.25,0.20, //medium brown right side
		0.36 ,0.25,0.20,
		0.36 ,0.25,0.20,

		0.184314,0.309804,0.184314,
		0.184314,0.309804,0.184314,
		0.184314,0.309804,0.184314,

		0.184314,0.309804,0.184314,
		0.184314,0.309804,0.184314,
		0.184314,0.309804,0.184314,

		0.35,0.16,0.14, //dark brown
		0.35,0.16,0.14,
		0.35,0.16,0.14,
	};
	// Texture coordinates start with (0,0) at top left of the image to (1,1) at bot right
	// static const GLfloat texture_buffer_data [] = {
	// 	0,1, // TexCoord 1 - bot left
	// 	1,1, // TexCoord 2 - bot right
	// 	1,0, // TexCoord 3 - top right

	// 	1,0, // TexCoord 3 - top right
	// 	0,0, // TexCoord 4 - top left
	// 	0,1  // TexCoord 1 - bot left
	// };

	// create3DTexturedObject creates and returns a handle to a VAO that can be used later
	cube.vao = create3DObject(GL_TRIANGLES, 36, vertex_buffer_data, color_buffer_data, GL_FILL);
	return cube;
	
}


CUBE movecube(CUBE cube)
{
	cube.posy+=(0.07*cube.direction);
	if(cube.posy >2)
		cube.direction*=-1;
	if(cube.posy < -2)
		cube.direction*=-1;
	return cube;

}
float camera_rotation_angle = 90;
float rectangle_rotation = 0;
float triangle_rotation = 0;
GLfloat timenow=0,timethen=0,gravitypower = -0.1;


void gravity()
{
	timenow = glfwGetTime();
	player.vely += gravitypower *(timenow-timethen);
	player.posy += player.vely;
	//cout<<timenow<<endl;
	timethen = timenow;
}


PLAYER YouCollide(PLAYER player, CUBE cube) // AABB - Circle collision
{
  // Get center point circle first 
  glm::vec3 center(player.posx,player.posy,player.posz);
  // Calculate AABB info (center, half-extents)
  glm::vec3 aabb_half_extents(1, 3, 1);
  glm::vec3 aabb_center(cube.posx, cube.posy, cube.posz);
  // Get difference vector between both centers
  glm::vec3 difference = center - aabb_center;
  glm::vec3 clamped = glm::clamp(difference, -aabb_half_extents, aabb_half_extents);
  // Add clamped value to AABB_center and we get the value of box closest to circle
  glm::vec3 closest = aabb_center + clamped;
  difference = center-closest;
  glm::vec3 yaxis( 0.0, 1.0 ,0.0);
  glm::vec3 xaxis( 1.0, 0.0 ,0.0);

  glm::vec3 zaxis( 0.0, 0.0 ,1.0);
  // Retrieve vector between center circle and closest point AABB and check if length <= radius
  float yot=glm::dot(difference, yaxis);
  float xot=glm::dot(difference, xaxis);
  float zot=glm::dot(difference, zaxis);


  if (glm::length(difference) < (player.radius-0.1))
    {
      if( xot > 0 )
	{
	  cout<<"Hurray!\n";
	  player.posx += 0.05;
	  
	}
      else if( xot < 0 )
	{
	  cout<<"Eureka!\n";
	  player.posx -= 0.05;
	  
	}
      else
	{
	  if( yot > 0 )
	    {
	      cout<<"Yurray!\n";
	      //if(player.posy <= 3.5)
	      // 	player.posy =3.5;
	      //else
	      player.posy += 0.05;
	  
	    }
	  else if( yot < 0 )
	    {
	      cout<<"Yureka!\n";
	      player.posy -= 0.05;
	    }
	  else
	    {

	      if( zot > 0 )
		{
		  cout<<"Zurray!\n";
		  player.posz += 0.05;
	
		}
	      else if( zot < 0 )
		{
		  cout<<"Zureka!\n";
		  player.posz -= 0.05;
	
		}
	    }

	}
    }
     return player;
}
void draw ()
{
	// clear the color and depth in the frame buffer
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram (programID);
	glm::vec3 up (0, 1, 0);

	if(towerview == true)
	{
		if(eyex < 12)
			eyex+=0.3;
		
		if(eyey < 15)
			eyey+=0.3;
		
		eyez=0;
		tarx =player.posx;
		tary =player.posy;
		tarz =player.posz;
		fov = 70;
	}

	else if(topview == true)
	{
		if(eyex > 0)
			eyex -=0.3;
		// eyex=0;
		if(eyey < 30)
			eyey+=0.3;
		// eyey=30;
		if(eyez < 1)
			eyez+=0.3;
		// eyez=1;
		tarx = 0;
		tary=0;
		tarz=0;
		fov = 70.2;
	}

	else if(playerview == true)
	{
		eyex = player.posx;
		eyey = player.posy +5 ;
		eyez = player.posz + 7 ;
		tarx =player.posx;
		tary =player.posy;
		tarz =player.posz;

	}
	glm::vec3 eye (eyex,eyey,eyez);
	glm::vec3 target (tarx, tary, tarz);
	Matrices.view = glm::lookAt(eye,target,up); // Fixed camera for 2D (ortho) in XY plane

	glm::mat4 VP = Matrices.projection * Matrices.view;
	glm::mat4 MVP;	

	if(player.posy <= cubes[0].posy+4)
	{
		player.vely =0;
		gravitypower=0;
	}
	else
	{
		player.gravitypower = -0.1;
		gravity();
	}
	//Rendering cubes
	
	for (int j = 0; j < 100; ++j)
	{
		if(cubes[j].missing == false)
		{
			if(cubes[j].moving == true)
				cubes[j] = movecube(cubes[j]);
			Matrices.model = glm::mat4(1.0f);
			glm::mat4 translatecube = glm::translate (glm::vec3(cubes[j].posx, cubes[j].posy, cubes[j].posz)); 
			glm::mat4 scalecube = glm::scale (glm::vec3(1,3,1));
			glm::mat4 cubeTransform = translatecube*scalecube;
			Matrices.model *= cubeTransform;
			MVP = VP * Matrices.model; // MVP = p * V * M
			glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
		 	draw3DObject(cubes[j].vao);
		 	player = YouCollide(player,cubes[j]);
		 	
	 	}
	 }	

	

	 //Rendering axises
	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateaxis = glm::translate (glm::vec3(0.0f, 0.0f, 0.0f)); // glTranslatef
	glm::mat4 axisTransform = translateaxis;
	Matrices.model *= axisTransform;
	MVP = VP * Matrices.model; // MVP = p * V * M
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
 	draw3DObject(axises);

 	 // Rendering Sea
 	glUseProgram(textureProgramID);
 	for(int k = 0;k<300;k++)
 	{
 		Matrices.model = glm::mat4(1.0f);
		glm::mat4 translatesea = glm::translate (glm::vec3(sea[k].posx,sea[k].posy,sea[k].posz)); // glTranslatef
		glm::mat4 scalesea = glm::scale (glm::vec3(80,2,80));
		glm::mat4 seaTransform = translatesea*scalesea;
		Matrices.model *= seaTransform;
		MVP = VP * Matrices.model; // MVP = p * V * M
		glUniformMatrix4fv(Matrices.TexMatrixID, 1, GL_FALSE, &MVP[0][0]);
		glUniform1i(glGetUniformLocation(textureProgramID, "texSampler"), 0);
	 	draw3DTexturedObject(sea[k].vao);
 	}
 	
	

	 //Rendering Player
 	glUseProgram(textureProgramID);
	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateplayer = glm::translate (glm::vec3(player.posx, player.posy,player.posz)); // glTranslatef
	glm::mat4 scaleplayer = glm::scale (glm::vec3(0.4,0.4,0.4));
	glm::mat4 playerTransform = translateplayer*scaleplayer;
	Matrices.model *= playerTransform;
	MVP = VP * Matrices.model; // MVP = p * V * M
	glUniformMatrix4fv(Matrices.TexMatrixID, 1, GL_FALSE, &MVP[0][0]);
	glUniform1i(glGetUniformLocation(textureProgramID, "texSampler"), 0);
 	draw3DTexturedObject(player.vao);

	/*// Render with texture shaders now
	glUseProgram(textureProgramID);

	// Pop matrix to undo transformations till last push matrix instead of recomputing model matrix
	// glPopMatrix ();
	Matrices.model = glm::mat4(1.0f);

	glm::mat4 translateRectangle = glm::translate (glm::vec3(2, 0, 0));        // glTranslatef
	//glm::mat4 rotateRectangle = glm::rotate((float)(rectangle_rotation*M_PI/180.0f), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
	Matrices.model *= (translateRectangle);
	MVP = VP * Matrices.model;

	// Copy MVP to texture shaders
	glUniformMatrix4fv(Matrices.TexMatrixID, 1, GL_FALSE, &MVP[0][0]);

	// Set the texture sampler to access Texture0 memory
	glUniform1i(glGetUniformLocation(textureProgramID, "texSampler"), 0);

	// draw3DObject draws the VAO given to it using current MVP matrix
	draw3DTexturedObject(rectangle);

	// Increment angles
	float increments = 1;
*/
	/*// Render font on screen
	static int fontScale = 0;
	float fontScaleValue = 0.75 + 0.25*sinf(fontScale*M_PI/180.0f);
	glm::vec3 fontColor = getRGBfromHue (fontScale);



	// Use font Shaders for next part of code
	glUseProgram(fontProgramID);
	Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane

	// Transform the text
	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateText = glm::translate(glm::vec3(-3,2,0));
	glm::mat4 scaleText = glm::scale(glm::vec3(fontScaleValue,fontScaleValue,fontScaleValue));
	Matrices.model *= (translateText * scaleText);
	MVP = Matrices.projection * Matrices.view * Matrices.model;
	// send font's MVP and font color to fond shaders
	glUniformMatrix4fv(GL3Font.fontMatrixID, 1, GL_FALSE, &MVP[0][0]);
	glUniform3fv(GL3Font.fontColorID, 1, &fontColor[0]);

	// Render font
	GL3Font.font->Render("Round n Round we go !!");
    */

	//camera_rotation_angle++; // Simulating camera rotation
	//triangle_rotation = triangle_rotation + increments*triangle_rot_dir*triangle_rot_status;
	//rectangle_rotation = rectangle_rotation + increments*rectangle_rot_dir*rectangle_rot_status;

	// font size and color changes
	// fontScale = (fontScale + 1) % 360;
}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height)
{
	GLFWwindow* window; // window desciptor/handle

	glfwSetErrorCallback(error_callback);
	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(width, height, "Sample OpenGL 3.3 Application", NULL, NULL);

	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	glfwSwapInterval( 1 );

	/* --- register callbacks with GLFW --- */

	/* Register function to handle window resizes */
	/* With Retina display on Mac OS X GLFW's FramebufferSize
	 is different from WindowSize */
	glfwSetFramebufferSizeCallback(window, reshapeWindow);
	glfwSetWindowSizeCallback(window, reshapeWindow);

	/* Register function to handle window close */
	glfwSetWindowCloseCallback(window, quit);

	/* Register function to handle keyboard input */
	glfwSetKeyCallback(window, keyboard);      // general keyboard input
	glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

	/* Register function to handle mouse click */
	glfwSetMouseButtonCallback(window, mouseButton);  // mouse button clicks

	return window;
}

/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height)
{
	// Load Textures
	// Enable Texture0 as current texture memory
	glActiveTexture(GL_TEXTURE0);
	// load an image file directly as a new OpenGL texture
	//GLuint texID = SOIL_load_OGL_texture ("beach.png", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_TEXTURE_REPEATS); // Buggy for OpenGL3
	GLuint seaID = createTexture("sea.jpg");
	// check for an error during the load process

	GLuint playerID = createTexture("deepsea.png");

	// Create and compile our GLSL program from the texture shaders
	textureProgramID = LoadShaders( "TextureRender.vert", "TextureRender.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.TexMatrixID = glGetUniformLocation(textureProgramID, "MVP");
	

	/* Objects should be created before any other gl function and shaders */
	// Create the models
	// Generate the VAO, VBOs, vertices data & copy into the array buffer
	createaxis();

	int mark = 0;
	float positionx =-10,positionz=6,positiony=0;
	for (int j = 0; j < 10; ++j)
	{
		positionx =-10;
		for(int i=0;i<10;i++)
		{	
			cubes[mark++] = createcube(cubes[mark],positionx,positiony,positionz);
			positionx+=2.1;
			
	 	}
	 	positionz-=2.1;
	 }	

	 int no_of_moving_cubes =0;
	while(no_of_moving_cubes < 10)
		no_of_moving_cubes = rand() % 30;
	for (int i = 0; i < no_of_moving_cubes; ++i)
	{
		int cube_no = rand() %100;
		if(cube_no >9 && cube_no<90)
			cubes[cube_no].moving = true;
	}
	cubes[3].missing = true;
	cubes[15].missing = true;
	
	mark =0;
	positionx = -1000;
	positiony = -2;
	positionz = 1000;
	for (int j = 0; j < 30; ++j)
	{
		positionx =-1000;
		for(int i=0;i<10;i++)
		{	
			sea[mark++] = create_sea(sea[mark],seaID,positionx,positiony,positionz);
			positionx+=160;
			
	 	}
	 	positionz-=160;
	 }	
	// sea[i] = create_sea(sea,seaID);
	player = makeplayer(player,playerID);
	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL3.vert", "Sample_GL3.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");


	reshapeWindow (window, width, height);

	// Background color of the scene
	glClearColor (0.3f, 0.3f, 0.3f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LEQUAL);
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Initialise FTGL stuff
	//const char* fontfile = "arial.ttf";
	// GL3Font.font = new FTExtrudeFont(fontfile); // 3D extrude style rendering

	// if(GL3Font.font->Error())
	// {
	// 	cout << "Error: Could not load font `" << fontfile << "'" << endl;
	// 	glfwTerminate();
	// 	exit(EXIT_FAILURE);
	// }

	// // Create and compile our GLSL program from the font shaders
	// fontProgramID = LoadShaders( "fontrender.vert", "fontrender.frag" );
	// GLint fontVertexCoordAttrib, fontVertexNormalAttrib, fontVertexOffsetUniform;
	// fontVertexCoordAttrib = glGetAttribLocation(fontProgramID, "vertexPosition");
	// fontVertexNormalAttrib = glGetAttribLocation(fontProgramID, "vertexNormal");
	// fontVertexOffsetUniform = glGetUniformLocation(fontProgramID, "pen");
	// GL3Font.fontMatrixID = glGetUniformLocation(fontProgramID, "MVP");
	// GL3Font.fontColorID = glGetUniformLocation(fontProgramID, "fontColor");

	// GL3Font.font->ShaderLocations(fontVertexCoordAttrib, fontVertexNormalAttrib, fontVertexOffsetUniform);
	// GL3Font.font->FaceSize(1);
	// GL3Font.font->Depth(0);
	// GL3Font.font->Outset(0, 0);
	// GL3Font.font->CharMap(ft_encoding_unicode);

	cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
	cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
	cout << "VERSION: " << glGetString(GL_VERSION) << endl;
	cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

}

int main (int argc, char** argv)
{
	int width = 1600;
	int height = 800;

	GLFWwindow* window = initGLFW(width, height);

	initGL (window, width, height);

	double last_update_time = glfwGetTime(), current_time;

	
	/* Draw in loop */
	while (!glfwWindowShouldClose(window)) {

		// OpenGL Draw commands
		draw();

		reshapeWindow (window, width, height);

		// Swap Frame Buffer in double buffering
		glfwSwapBuffers(window);

		// Poll for Keyboard and mouse events
		glfwPollEvents();

		// Control based on time (Time based transformation like 5 degrees rotation every 0.5s)
		current_time = glfwGetTime(); // Time in seconds
		if ((current_time - last_update_time) >= 0.5) { // atleast 0.5s elapsed since last frame
			// do something every 0.5 seconds ..
			last_update_time = current_time;
		}
	}

	glfwTerminate();
	exit(EXIT_SUCCESS);
}
