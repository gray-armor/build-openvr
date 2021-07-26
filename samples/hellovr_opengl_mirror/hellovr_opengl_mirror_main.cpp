#include <SDL.h>
#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL_opengl.h>

#include <stdio.h>
#include <string>
#include <cstdlib>

#include <openvr.h>

#include "shared/lodepng.h"
#include "shared/Matrices.h"
#include "shared/pathtools.h"

#include <iostream>

class MainApp
{
};
struct FramebufferDesc
{
  GLuint DepthBufferId;
  GLuint RenderTextureId;
  GLuint RenderFramebufferId;
  GLuint ResolveTextureId;
  GLuint ResolveFramebufferId;
};
vr::IVRSystem *HMD;
FramebufferDesc leftEyeDesc;
FramebufferDesc rightEyeDesc;
uint32_t HMDWidth;
uint32_t HMDHeight;

vr::TrackedDevicePose_t TrackedDevicePose[vr::k_unMaxTrackedDeviceCount];

Matrix4 mat4DevicePose[vr::k_unMaxTrackedDeviceCount];
Matrix4 mat4HMDPose;
Matrix4 mat4ProjectionLeft;
Matrix4 mat4ProjectionRight;
Matrix4 mat4EyePoseLeft;
Matrix4 mat4EyePoseRight;

GLuint SceneProgramID;
GLuint SceneMatrixLocation;
GLuint SceneVAO;
GLuint iTexture;
GLuint SceneVertBuffer;

unsigned int iVertcount;

struct VertexDataScene
{
  Vector3 position; // (x, y, z)
  Vector2 texCoord; // テクスチャの座標？
};

bool CreateFrameBuffer(int Width, int Height, FramebufferDesc &framebufferDesc)
{
  glGenFramebuffers(1, &framebufferDesc.RenderFramebufferId);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.RenderFramebufferId);

  glGenRenderbuffers(1, &framebufferDesc.DepthBufferId);
  glBindRenderbuffer(GL_RENDERBUFFER, framebufferDesc.DepthBufferId);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, Width, Height);
  glFramebufferRenderbuffer(GL_RENDERBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebufferDesc.DepthBufferId);

  glGenTextures(1, &framebufferDesc.RenderTextureId);
  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc.RenderTextureId);
  glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, Width, Height, true);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc.RenderTextureId, 0);

  glGenFramebuffers(1, &framebufferDesc.ResolveFramebufferId);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.ResolveFramebufferId);

  glGenTextures(1, &framebufferDesc.ResolveTextureId);
  glBindTexture(GL_TEXTURE_2D, framebufferDesc.ResolveTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc.ResolveTextureId, 0);

  // check FBO status
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE)
  {
    return false;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return true;
};

bool SetupStereoRenderTargets()
{
  if (!HMD)
    return false;

  HMD->GetRecommendedRenderTargetSize(&HMDWidth, &HMDHeight);
  printf("HMD's Width%d\n", HMDWidth);
  printf("HMD's Height%d\n", HMDHeight);

  CreateFrameBuffer(HMDWidth, HMDHeight, leftEyeDesc);
  CreateFrameBuffer(HMDWidth, HMDHeight, rightEyeDesc);

  return true;
}

bool SetupTexturemaps()
{
  std::string ExecutableDirectory = Path_StripFilename(Path_GetExecutablePath());
  std::string FullPath = Path_MakeAbsolute("../cube_texture.png", ExecutableDirectory);

  std::vector<unsigned char> imageRGBA;
  unsigned ImageWidth, ImageHeight;
  unsigned nError = lodepng::decode(imageRGBA, ImageWidth, ImageHeight, FullPath.c_str());

  if (nError != 0)
    return false;

  glGenTextures(1, &iTexture);
  glBindTexture(GL_TEXTURE_2D, iTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ImageWidth, ImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &imageRGBA[0]);

  glGenerateMipmap(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  GLfloat Largest;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &Largest);
  glTexParameterf(GL_TEXTURE_2D, GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, Largest);

  glBindTexture(GL_TEXTURE_2D, 0);

  return (iTexture != 0);
}

void AddCubeVertex(float fl0, float fl1, float fl2, float fl3, float fl4, std::vector<float> &vertdata)
{
  // fl0~fl4までの4つのデータを挿入
  vertdata.push_back(fl0); // x
  vertdata.push_back(fl1); // y
  vertdata.push_back(fl2); // z
  vertdata.push_back(fl3);
  vertdata.push_back(fl4);
}

void AddCubeToScene(Matrix4 mat, std::vector<float> &vertdata)
{
  // matはGlobal座標
  Vector4 A = mat * Vector4(0, 0, 0, 1);
  Vector4 B = mat * Vector4(1, 0, 0, 1);
  Vector4 C = mat * Vector4(1, 1, 0, 1);
  Vector4 D = mat * Vector4(0, 1, 0, 1);
  Vector4 E = mat * Vector4(0, 0, 1, 1);
  Vector4 F = mat * Vector4(1, 0, 1, 1);
  Vector4 G = mat * Vector4(1, 1, 1, 1);
  Vector4 H = mat * Vector4(0, 1, 1, 1);

  AddCubeVertex(E.x, E.y, E.z, 0, 1, vertdata); //Front
  AddCubeVertex(F.x, F.y, F.z, 1, 1, vertdata);
  AddCubeVertex(G.x, G.y, G.z, 1, 0, vertdata);
  AddCubeVertex(G.x, G.y, G.z, 1, 0, vertdata);
  AddCubeVertex(H.x, H.y, H.z, 0, 0, vertdata);
  AddCubeVertex(E.x, E.y, E.z, 0, 1, vertdata);

  AddCubeVertex(B.x, B.y, B.z, 0, 1, vertdata); //Back
  AddCubeVertex(A.x, A.y, A.z, 1, 1, vertdata);
  AddCubeVertex(D.x, D.y, D.z, 1, 0, vertdata);
  AddCubeVertex(D.x, D.y, D.z, 1, 0, vertdata);
  AddCubeVertex(C.x, C.y, C.z, 0, 0, vertdata);
  AddCubeVertex(B.x, B.y, B.z, 0, 1, vertdata);

  AddCubeVertex(H.x, H.y, H.z, 0, 1, vertdata); //Top
  AddCubeVertex(G.x, G.y, G.z, 1, 1, vertdata);
  AddCubeVertex(C.x, C.y, C.z, 1, 0, vertdata);
  AddCubeVertex(C.x, C.y, C.z, 1, 0, vertdata);
  AddCubeVertex(D.x, D.y, D.z, 0, 0, vertdata);
  AddCubeVertex(H.x, H.y, H.z, 0, 1, vertdata);

  AddCubeVertex(A.x, A.y, A.z, 0, 1, vertdata); //Bottom
  AddCubeVertex(B.x, B.y, B.z, 1, 1, vertdata);
  AddCubeVertex(F.x, F.y, F.z, 1, 0, vertdata);
  AddCubeVertex(F.x, F.y, F.z, 1, 0, vertdata);
  AddCubeVertex(E.x, E.y, E.z, 0, 0, vertdata);
  AddCubeVertex(A.x, A.y, A.z, 0, 1, vertdata);

  AddCubeVertex(A.x, A.y, A.z, 0, 1, vertdata); //Left
  AddCubeVertex(E.x, E.y, E.z, 1, 1, vertdata);
  AddCubeVertex(H.x, H.y, H.z, 1, 0, vertdata);
  AddCubeVertex(H.x, H.y, H.z, 1, 0, vertdata);
  AddCubeVertex(D.x, D.y, D.z, 0, 0, vertdata);
  AddCubeVertex(A.x, A.y, A.z, 0, 1, vertdata);

  AddCubeVertex(F.x, F.y, F.z, 0, 1, vertdata); //Right
  AddCubeVertex(B.x, B.y, B.z, 1, 1, vertdata);
  AddCubeVertex(C.x, C.y, C.z, 1, 0, vertdata);
  AddCubeVertex(C.x, C.y, C.z, 1, 0, vertdata);
  AddCubeVertex(G.x, G.y, G.z, 0, 0, vertdata);
  AddCubeVertex(F.x, F.y, F.z, 0, 1, vertdata);
}

void SetupScene()
{
  if (!HMD)
    return;

  std::vector<float> vertdataarray;

  float Scale = 0.3;
  float ScaleSpacing = 4.0;
  int SceneVolumeWidth = 10;
  int SceneVolumeHeight = 10;
  int SceneVolumeDepth = 10;

  Matrix4 matScale;
  matScale.scale(Scale, Scale, Scale); // 拡大縮小行列。

  Matrix4 matTransform;
  matTransform.translate(
      -((float)SceneVolumeWidth * ScaleSpacing) / 2.f,
      -((float)SceneVolumeHeight * ScaleSpacing) / 2.f,
      -((float)SceneVolumeDepth * ScaleSpacing) / 2.f); // 平行移動

  Matrix4 mat = matScale * matTransform;
  // matはglobal座標への変換ぽい

  for (int z = 0; z < SceneVolumeDepth; z++)
  {
    for (int y = 0; y < SceneVolumeHeight; y++)
    {
      for (int x = 0; x < SceneVolumeWidth; x++)
      {
        AddCubeToScene(mat, vertdataarray);
        mat = mat * Matrix4().translate(ScaleSpacing, 0, 0); // x軸方向に移動
      }
      mat = mat * Matrix4().translate(-((float)SceneVolumeWidth) * ScaleSpacing, ScaleSpacing, 0);
    }
    mat = mat * Matrix4().translate(0, -((float)SceneVolumeHeight) * ScaleSpacing, ScaleSpacing);
  }
  iVertcount = vertdataarray.size() / 5; // 5データ1頂点分

  glGenVertexArrays(1, &SceneVAO);
  glBindVertexArray(SceneVAO);
  glGenBuffers(1, &SceneVertBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, SceneVertBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertdataarray.size(), &vertdataarray[0], GL_STATIC_DRAW);

  GLsizei stride = sizeof(VertexDataScene);
  uintptr_t offset = 0;

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offset); // positionの設定（?）

  offset += sizeof(Vector3);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)offset); // texCoordの設定（?）

  glBindVertexArray(0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

Matrix4 GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye)
{
  // Gets a Matrix Projection Eye with respect to nEye.
  if (!HMD)
    return Matrix4();

  // クリッピングの範囲.投影行列を
  float NearClip = 0.1;
  float FarClip = 30.0;

  vr::HmdMatrix44_t mat = HMD->GetProjectionMatrix(nEye, NearClip, FarClip);

  return Matrix4(
      mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
      mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
      mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
      mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]);
}

Matrix4 GetHMDMatrixPoseEye(vr::Hmd_Eye nEye)
{
  // Gets an HMDMatrixPoseEye with respect to nEye.
  if (!HMD)
    return Matrix4();

  // Returns the transform from eye space to the head space
  vr::HmdMatrix34_t matEye = HMD->GetEyeToHeadTransform(nEye);
  Matrix4 matrixObj(
      matEye.m[0][0], matEye.m[1][0], matEye.m[2][0], 0.0,
      matEye.m[0][1], matEye.m[1][1], matEye.m[2][1], 0.0,
      matEye.m[0][2], matEye.m[1][2], matEye.m[2][2], 0.0,
      matEye.m[0][3], matEye.m[1][3], matEye.m[2][3], 1.0f);

  // transform from head space to eye space by inverting
  return matrixObj.invert();
}

void SetupCameras()
{
  mat4ProjectionLeft = GetHMDMatrixProjectionEye(vr::Eye_Left);
  mat4ProjectionRight = GetHMDMatrixProjectionEye(vr::Eye_Right);
  mat4EyePoseLeft = GetHMDMatrixPoseEye(vr::Eye_Left);
  mat4EyePoseRight = GetHMDMatrixPoseEye(vr::Eye_Right);
}

GLuint CompileGLShader(const char *ShaderName, const char *VertexShader, const char *FragmentShader)
{
  GLuint ProgramID = glCreateProgram();
  printf("Success glCreateProgram\n");
  GLuint SceneVertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(SceneVertexShader, 1, &VertexShader, NULL);
  glCompileShader(SceneVertexShader);
  GLint VertexShaderCompiled = GL_FALSE;
  glGetShaderiv(SceneVertexShader, GL_COMPILE_STATUS, &VertexShaderCompiled);
  if (VertexShaderCompiled != GL_TRUE)
  {
    printf("Unable to compile vertex shader\n");
  }
  glAttachShader(ProgramID, SceneVertexShader);
  glDeleteShader(SceneVertexShader);

  GLuint SceneFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(SceneFragmentShader, 1, &FragmentShader, NULL);
  glCompileShader(SceneFragmentShader);
  GLint FragmentShaderCompiled = GL_FALSE;
  glGetShaderiv(SceneFragmentShader, GL_COMPILE_STATUS, &FragmentShaderCompiled);
  if (FragmentShaderCompiled != GL_TRUE)
  {
    printf("Unable to compile fragment shader\n");
  }
  glAttachShader(ProgramID, SceneFragmentShader);
  glDeleteShader(SceneFragmentShader);

  glLinkProgram(ProgramID);

  GLint programSuccess = GL_TRUE;
  glGetProgramiv(ProgramID, GL_LINK_STATUS, &programSuccess);
  if (programSuccess != GL_TRUE)
  {
    printf("Error linking program\n");
    return 0;
  }

  glUseProgram(ProgramID);
  glUseProgram(0);

  return ProgramID;
}

bool CreateAllShaders()
{
  SceneProgramID = CompileGLShader(
      "Scene",

      "#version 410\n"
      "uniform mat4 matrix;\n"
      "layout(location = 0) in vec4 position;\n"
      "layout(location = 1) in vec2 v2UVcoordsIn;\n"
      "layout(location = 2) in vec3 v3NormalIn;\n"
      "out vec2 v2UVcoords;\n"
      "void main()\n"
      "{\n"
      "	v2UVcoords = v2UVcoordsIn;\n"
      "	gl_Position = matrix * position;\n"
      "}\n",

      "#version 410 core\n"
      "uniform sampler2D mytexture;\n"
      "in vec2 v2UVcoords;\n"
      "out vec4 outputColor;\n"
      "void main()\n"
      "{\n"
      "outputColor = texture(mytexture, v2UVcoords);\n"
      "}\n");
  SceneMatrixLocation = glGetUniformLocation(SceneProgramID, "matrix");
  if (SceneMatrixLocation == -1)
  {
    printf("Unable to find matrix uniform in scene shader\n");
  }

  return SceneProgramID != 0;
}

bool InitGL()
{
  if (!CreateAllShaders())
    return false;

  SetupTexturemaps(); // キューブ表面に貼り付ける画像
  SetupScene();       // キューブの頂点データ生成
  SetupCameras();     // 投影面の座標（?）とHMDの視点座標の取得（?）←一度切りで良いっぽい..
  SetupStereoRenderTargets();

  return true;
}

bool Init()
{
  printf("Start Init\n");
  vr::EVRInitError eError = vr::VRInitError_None;
  HMD = vr::VR_Init(&eError, vr::VRApplication_Scene);
  if (eError != vr::VRInitError_None)
  {
    printf("Unable to init OpenVR"); // TODO
  }

  SDL_Window *CompanionWindow = SDL_CreateWindow("hellovr", 0, 0, 100, 100, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  SDL_GLContext Context = SDL_GL_CreateContext(CompanionWindow);

  glewExperimental = GL_TRUE;
  GLenum nGlewError = glewInit();
  if (nGlewError != GLEW_OK)
  {
    printf("%s - Error initializing GLEW! %s\n", __FUNCTION__, glewGetErrorString(nGlewError));
    return false;
  }
  glGetError();

  if (!InitGL())
  {
    printf("Unable to init OpenGL"); // TODO
  }

  if (!vr::VRCompositor())
  {
    printf("Unable to init VR Compositor"); // TODO
  }

  return true;
}

bool HandleInput()
{
  return false;
}

Matrix4 GetCurrentViewProjectionMatrix(vr::Hmd_Eye nEye)
{
  Matrix4 matMVP;
  if (nEye == vr::Eye_Left)
  {
    matMVP = mat4ProjectionLeft * mat4EyePoseLeft * mat4HMDPose; // HMDPoseだけが更新される。
  }
  else if (nEye == vr::Eye_Right)
  {
    matMVP = mat4ProjectionRight * mat4EyePoseRight * mat4HMDPose;
  }

  return matMVP;
}

void RenderScene(vr::Hmd_Eye nEye)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  // キューブ(シーン)の描画
  glUseProgram(SceneProgramID);
  glUniformMatrix4fv(SceneMatrixLocation, 1, GL_FALSE, GetCurrentViewProjectionMatrix(nEye).get());
  glBindVertexArray(SceneVAO);
  glBindTexture(GL_TEXTURE_2D, iTexture); //
  glDrawArrays(GL_TRIANGLES, 0, iVertcount);
  glBindVertexArray(0);

  glUseProgram(0);
}

void BindFrameBuffer(vr::Hmd_Eye nEye, FramebufferDesc &framebufferDesc)
{
  glEnable(GL_MULTISAMPLE);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.RenderFramebufferId);
  glViewport(0, 0, HMDWidth, HMDHeight);
  RenderScene(nEye);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_MULTISAMPLE);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferDesc.RenderFramebufferId);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferDesc.ResolveFramebufferId);
  glBlitFramebuffer(0, 0, HMDWidth, HMDHeight, 0, 0, HMDWidth, HMDHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void RenderStereoTargetsByGL()
{
  glClearColor(0.0, 0.0, 0.0, 1.0);

  // Left Eye
  BindFrameBuffer(vr::Eye_Left, leftEyeDesc);

  // Right Eye
  BindFrameBuffer(vr::Eye_Right, rightEyeDesc);
}

void UpdateHMDMatrixPose()
{
  if (!HMD)
    return;

  vr::VRCompositor()->WaitGetPoses(TrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

  if (TrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
  {
    mat4HMDPose = mat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
    mat4HMDPose.invert();
  }
}

void RenderFrame()
{
  if (HMD)
  {
    RenderStereoTargetsByGL();

    // texture のアップデート
    vr::Texture_t leftEyeTexture = {(void *)(uintptr_t)leftEyeDesc.ResolveTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
    vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);

    vr::Texture_t rightEyeTexture = {(void *)(uintptr_t)rightEyeDesc.ResolveTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
    vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);
  }

  // 背景色の初期化
  {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  UpdateHMDMatrixPose();
}

void RunMainLoop()
{
  bool quit = false;
  while (!quit)
  {
    quit = HandleInput();

    RenderFrame();
  }
}

void Shutdown()
{
  if (HMD)
  {
    vr::VR_Shutdown();
    HMD = NULL;
  }

  glDeleteBuffers(1, &SceneVertBuffer);

  if (SceneProgramID)
  {
    glDeleteProgram(SceneProgramID);
  }

  glDeleteRenderbuffers(1, &leftEyeDesc.DepthBufferId);
  glDeleteTextures(1, &leftEyeDesc.RenderTextureId);
  glDeleteFramebuffers(1, &leftEyeDesc.RenderFramebufferId);
  glDeleteTextures(1, &leftEyeDesc.ResolveTextureId);
  glDeleteFramebuffers(1, &leftEyeDesc.ResolveFramebufferId);

  glDeleteRenderbuffers(1, &rightEyeDesc.DepthBufferId);
  glDeleteTextures(1, &rightEyeDesc.RenderTextureId);
  glDeleteFramebuffers(1, &rightEyeDesc.RenderFramebufferId);
  glDeleteTextures(1, &rightEyeDesc.ResolveTextureId);
  glDeleteFramebuffers(1, &rightEyeDesc.ResolveFramebufferId);

  if (SceneVAO != 0)
  {
    glDeleteVertexArrays(1, &SceneVAO);
  }
}

int main(int argc, char *argv[])
{
  if (!Init())
  {
    printf("Shutdown");
    Shutdown();
  }
  printf("Init Success!");

  RunMainLoop();

  Shutdown();

  return 0;
}
