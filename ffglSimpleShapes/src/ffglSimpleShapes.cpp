#include <FFGL.h>
#include <FFGLLib.h>
#include <cstdlib>
#include <cmath>
#include <stdio.h>
#include "ffglSimpleShapes.h"
#include "utilities.h"

#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
// glm::translate, glm::rotate, glm::scale
#include <glm/gtc/matrix_transform.hpp>
// glm::value_ptr
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/constants.hpp>

#ifdef _WIN32
#include "glut.h"
#include <windows.h>
#else	//all other platforms
#include <OpenGL/glu.h>
#include <sys/time.h>
#endif
#ifdef __APPLE__	//OS X
#include <Carbon.h>
#endif

#define	FFPARAM_Mode	(0)
#define FFPARAM_Rotation (1)
#define FFPARAM_Radius (2)

bool hastime = false;	//workaround for hosts without Time support

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Plugin information
////////////////////////////////////////////////////////////////////////////////////////////////////

static CFFGLPluginInfo PluginInfo ( 
	FFGLStaticSource::CreateInstance,	// Create method
	"SHAP",								// Plugin unique ID											
	"Basic Shape",					// Plugin name											
	1,						   			// API major version number 													
	500,								  // API minor version number	
	1,										// Plugin major version number
	100,									// Plugin minor version number
	FF_SOURCE,						// Plugin type
	"Shapes",	// Plugin description
	"by Tim Cantwell" // About
);

char *vertexShaderCode =
"void main()"
"{"
"  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
"  gl_FrontColor = gl_Color;"
"}";

char *fragmentShaderCode =
"float rand(vec2 co){"
"        return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);"
"}"
"uniform float time;"
"uniform bool twotone;"
"uniform bool grayscale;"
"void main(void)"
"{"
"    vec4 c_out = vec4(0.0, 1.0, 0.0, 1.0);"
"    vec2 texCoord = gl_FragCoord.xy;"
"    texCoord.s += 8.64*fract(texCoord.t + time);"
"    texCoord.t += 4.57*fract(texCoord.s + time);"
"    if (grayscale && !twotone)"    //grayscale
"        c_out.rgb = vec3(mod(time+rand(vec2(texCoord.s+2.34*time, texCoord.t+3.14*time)), 1.0));"
"    if (!grayscale && !twotone)"    //full color
"        c_out.rgb = vec3(mod(time+rand(texCoord+vec2(time)), 1.0), mod(time+rand(texCoord+vec2(2.0*time)), 1.0), mod(time+rand(texCoord+vec2(3.0*time)), 1.0));"
"    if (twotone)"    //twotone
"        if (mod(time+rand(texCoord + vec2(time)), 1.0) > 0.5)"
"            c_out.rgb = vec3(1.0);"
"        else"
"            c_out.rgb = vec3(0.0);"
"    gl_FragColor = c_out;"
"}";

static glm::mat4 MVP;
static int m_width;
static int m_height;
////////////////////////////////////////////////////////////////////////////////////////////////////
//  Constructor and destructor
////////////////////////////////////////////////////////////////////////////////////////////////////

FFGLStaticSource::FFGLStaticSource()
:CFreeFrameGLPlugin(),
 m_initResources(1)
{
	// Input properties
	SetMinInputs(0);
	SetMaxInputs(0);
	SetTimeSupported(true);

	init_time(&t0);
#ifdef _WIN32
	srand(GetTickCount());
#else
	srand(time(NULL));
#endif

	// parameters:
	SetParamInfo(FFPARAM_Mode, "Sides", FF_TYPE_STANDARD, 0.5f);
	SetParamInfo(FFPARAM_Rotation, "Rotation", FF_TYPE_STANDARD, 0.0f);
	SetParamInfo(FFPARAM_Radius, "Radius", FF_TYPE_STANDARD, 0.5f);
	m_mode = 0.5f;
	m_rotation = 0.0f;
	m_radius = 0.5f;
}

DWORD FFGLStaticSource::SetTime(double time)
{
  m_time = time;
  hastime = true;
  return FF_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Methods
////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD FFGLStaticSource::InitGL(const FFGLViewportStruct *vp)
{
  //initialize gl extensions and
  //make sure required features are supported
  m_extensions.Initialize();
  if (m_extensions.ARB_shader_objects==0)
    return FF_FAIL;
    
  //initialize gl shader
  m_shader.SetExtensions(&m_extensions);
  m_shader.Compile(vertexShaderCode,fragmentShaderCode);
 
  //activate our shader
  m_shader.BindShader();
    
  //to assign values to parameters in the shader, we have to lookup
  //the "location" of each value.. then call one of the glUniform* methods
  //to assign a value
  m_timeLocation = m_shader.FindUniform("time");
  m_grayscaleLocation = m_shader.FindUniform("grayscale");
  m_twotoneLocation = m_shader.FindUniform("twotone");
  //m_matrixLocation = m_shader.FindUniform("mvpmatrix");
    
  m_shader.UnbindShader();
  
  float width = (float)(vp->width);
  float height = (float)(vp->height);

  m_width = width;
  m_height = height;

  float eyeX = width / 2;
  float eyeY = height / 2;
  float halfFov = 3.14159 * 60 / 360.0f;
  float theTan = tanf(halfFov);
  float dist = eyeY / theTan;
  //float aspect = (float)width / height;
  float nearDist = dist / 10.0f;
  float farDist = dist * 10.0f;

  //Pixel Coordinates
  glm::mat4 Projection = glm::perspective(halfFov*2, width / height, nearDist, farDist);

  glm::mat4 View = glm::lookAt(
	  glm::vec3(eyeX, eyeY, -dist), // Camera is at (4,3,3), in World Space
	  glm::vec3(eyeX, eyeY, 0), // and looks at the origin
	  glm::vec3(0, -1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
	  );

  glm::mat4 Model      = glm::mat4(1.0f);  // Changes for each model !
  // Our ModelViewProjection : multiplication of our 3 matrices
  MVP        = Projection * View * Model; 
  
  return FF_SUCCESS;
}

DWORD FFGLStaticSource::DeInitGL()
{
  m_shader.FreeGLResources();

  return FF_SUCCESS;
}

DWORD FFGLStaticSource::ProcessOpenGL(ProcessOpenGLStruct *pGL)
{
// If no calls to SetTime have been made, the host probably
// doesn't support Time, so we'll update m_time manually
	if (!hastime)
	{
		update_time(&m_time, t0);
	}

	//activate our shader
	//m_shader.BindShader();
	    
	//get the max s,t that correspond to the 
	//width,height of the used portion of the allocated texture space
	float foo = (m_time+float(rand()%100));
	//m_extensions.glUniform1fARB(m_timeLocation, foo);

	//m_extensions.glUniform1iARB(m_grayscaleLocation, m_mode>0.33f?true:false);
	//m_extensions.glUniform1iARB(m_twotoneLocation, m_mode>0.66f?true:false);
	//m_extensions.glUniformMatrix4fvARB(m_matrixLocation, 1, false, &MVP[0][0]);

	//draw the quad that will be painted by the shader/textures
	//note that we are sending texture coordinates to texture unit 0..
	//the vertex shader and fragment shader refer to this when querying for
	//texture coordinates of the inputTexture
	
	float fhheight = m_height / 2;

	glm::vec2 vec = glm::vec2(0.0, m_radius * fhheight);
	vec = glm::rotate(vec, 2 * 3.14159f * m_rotation);
	glPushMatrix();
	glLoadMatrixf(&MVP[0][0]);
	glBegin(GL_TRIANGLE_FAN);
	glColor3f(1.0f, 1.0f, 1.0f);
	int hwidth = m_width / 2; int hheight = m_height / 2;
	
	glVertex2f(hwidth, hheight);
	
	float temp;
	if (m_mode < 0.3){
		temp = 0.31;
	}
	else{
		temp = m_mode;
	}
	
	int sides = (int)(temp * 10);
	float angle = 2 * 3.14159f / sides;

	int i = 0;
	while (i < sides+1){
		glVertex2f(hwidth + vec[0], hheight - vec[1]);
		vec = glm::rotate(vec, angle);
		i++;
	}

	glEnd();
	glPopMatrix();
  //unbind the shader
  //m_shader.UnbindShader();  
  return FF_SUCCESS;
}
DWORD FFGLStaticSource::GetParameter(DWORD dwIndex)
{
	DWORD dwRet;
	switch (dwIndex) {
	case FFPARAM_Mode:
    *((float *)(unsigned)&dwRet) = m_mode;
		return dwRet;
	case FFPARAM_Rotation:
		*((float *)(unsigned)&dwRet) = m_rotation;
		return dwRet;
	case FFPARAM_Radius:
		*((float *)(unsigned)&dwRet) = m_radius;
		return dwRet;
	default:
		return FF_FAIL;
	}
}

char* FFGLStaticSource::GetParameterDisplay(DWORD dwIndex)
{
	DWORD dwType = m_pPlugin->GetParamType(dwIndex);
	DWORD dwValue = m_pPlugin->GetParameter(dwIndex);

	if ((dwValue != FF_FAIL) && (dwType != FF_FAIL))
	{
		if (dwType == FF_TYPE_TEXT)
		{
			return (char *)dwValue;
		}
		else
		{
			switch (dwIndex) {
			case FFPARAM_Mode:
				if (m_mode < 0.33)
					sprintf(m_Displayvalue, "%s", "Color");
				else if (m_mode < 0.66)
					sprintf(m_Displayvalue, "%s", "Grayscale");
				else
					sprintf(m_Displayvalue, "%s", "Two-tone");
				break;
			case FFPARAM_Rotation:
				if (m_rotation < 0.33)
					sprintf(m_Displayvalue, "%s", "Slight");
				else if (m_rotation < 0.66)
					sprintf(m_Displayvalue, "%s", "More");
				else
					sprintf(m_Displayvalue, "%s", "Most");
				break;
			case FFPARAM_Radius:
				sprintf(m_Displayvalue, "%s", "Radius");
				break;
			default:
				return NULL;
			}
			return m_Displayvalue;
		}
	}
	return NULL;
}

DWORD FFGLStaticSource::SetParameter(const SetParameterStruct* pParam)
{
	if (pParam != NULL) {
		switch (pParam->ParameterNumber) {
		case FFPARAM_Mode:
			m_mode = *((float *)&(pParam->NewParameterValue));
			break;
		case FFPARAM_Rotation:
			m_rotation = *((float *)&(pParam->NewParameterValue));
			break;
		case FFPARAM_Radius:
			m_radius = *((float *)&(pParam->NewParameterValue));
			break;
		default:
			return FF_FAIL;
		}
		return FF_SUCCESS;
	}
	return FF_FAIL;
}