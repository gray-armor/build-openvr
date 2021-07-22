#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <GL/glu.h>
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
  GLuint m_nDepthBufferId;
  GLuint m_nRenderTextureId;
  GLuint m_nRenderFramebufferId;
  GLuint m_nResolveTextureId;
  GLuint m_nResolveFramebufferId;
};
vr::IVRSystem *HMD;
FramebufferDesc leftEyeDesc;
FramebufferDesc rightEyeDesc;
uint32_t HMDWidth;
uint32_t HMDHeight;
GLuint unSceneProgramID;
vr::TrackedDevicePose_t TrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
Matrix4 mat4DevicePose[vr::k_unMaxTrackedDeviceCount];
Matrix4 mat4HMDPose;

bool CreateFrameBuffer(int Width, int Height, FramebufferDesc &framebufferDesc)
{
  glGenFramebuffers(1, &framebufferDesc.m_nRenderFramebufferId);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.m_nRenderFramebufferId);

  glGenRenderbuffers(1, &framebufferDesc.m_nDepthBufferId);
  glBindRenderbuffer(GL_RENDERBUFFER, framebufferDesc.m_nDepthBufferId);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, Width, Height);
  glFramebufferRenderbuffer(GL_RENDERBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebufferDesc.m_nDepthBufferId);

  glGenTextures(1, &framebufferDesc.m_nRenderTextureId);
  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc.m_nRenderTextureId);
  glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, Width, Height, true);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc.m_nRenderTextureId, 0);

  glGenFramebuffers(1, &framebufferDesc.m_nResolveFramebufferId);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.m_nResolveFramebufferId);

  glGenTextures(1, &framebufferDesc.m_nResolveTextureId);
  glBindTexture(GL_TEXTURE_2D, framebufferDesc.m_nResolveTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc.m_nResolveTextureId, 0);

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

bool Init()
{
  HMDWidth = 100;
  HMDHeight = 100;
}

void Shutdown()
{
}

bool HandleInput()
{
  return false;
}

void RenderScene(vr::Hmd_Eye nEye);
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  glUseProgram(unSceneProgramID);
  glUniformMatrix4fv(SceneMatrixLocation, 1, GL_FALSE, GetCurrentViewProjectionMatrix(nEye).get());
}

void BindFrameBuffer(vr::Hmd_Eye nEye, FramebufferDesc &framebufferDesc)
{
  glEnable(GL_MULTISAMPLE);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.m_nRenderFramebufferId);
  glViewport(0, 0, HMDWidth, HMDHeight);
  RenderScene(nEye);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_MULTISAMPLE);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferDesc.m_nRenderFramebufferId);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferDesc.m_nResolveFramebufferId);
  glBlitFramebuffer(0, 0, HMDWidth, HMDHeight, 0, 0, HMDWidth, HMDHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void RenderStereoTargetsByGL()
{
  glClearColor(0.0, 0.0, 0.0, 1.0);

  // Left Eye
  BindFrameBuffer(vr::Eye_Left, leftEyeDesc);

  // なぜここらへんでRenderSceneした後の動作を行っているか調べる。

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
    mat4DevicePose->invert();
  }
}

void RenderFrame()
{
  if (HMD)
  {
    RenderStereoTargetsByGL();

    // texture のアップデート
    vr::Texture_t leftEyeTexture = {(void *)(uintptr_t)leftEyeDesc.m_nResolveTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
    vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);

    vr::Texture_t rightEyeTexture = {(void *)(uintptr_t)rightEyeDesc.m_nResolveTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
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

int main(int argc, char *argv[])
{
  printf("Hello, World!\n");
  if (!Init())
  {
    Shutdown();
  }

  RunMainLoop();

  Shutdown();

  return 0;
}
