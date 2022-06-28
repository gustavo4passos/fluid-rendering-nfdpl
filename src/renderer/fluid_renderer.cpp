#include "fluid_renderer.h"
#include "../utils/glcall.h"
#include "../utils/logger.h"
#include "../Vec.hpp"
#include <SDL2/SDL.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace fluidity
{
    FluidRenderer::FluidRenderer(unsigned windowWidth, unsigned windowHeight, float pointRadius)
    :   Renderer(),
        m_fluidSurfaces(nullptr),
        m_textureRenderer(nullptr),
        m_particleRenderPass(nullptr),
        m_depthPass(nullptr),
        m_filterPass(nullptr),
        m_currentVAO(0),
        m_uniformBufferCameraData(0),
        m_uniformBufferLights(0),
        m_uniformBufferMaterial(0),
        m_currentNumberOfParticles(0),
        m_windowWidth(windowWidth),
        m_windowHeight(windowHeight),
        m_aspectRatio((float) windowWidth / windowHeight),
        m_pointRadius(pointRadius),
        m_filteringEnabled(true),
        m_cameraController(Camera({ 9.66f, 7.73f, 5}, 45.f))
    { /* */ }

    auto FluidRenderer::Clear() -> void
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    auto FluidRenderer::SetClearColor(float r, float g, float b, float a) -> void
    {
        glClearColor(r, g, b, a);
    }

    auto FluidRenderer::SetVAO(GLuint vao) -> void
    {
        // TODO: Have an array of RenderPass* so it is not needed to modify this method
        // every time a new pass is added
        m_currentVAO = vao;
        m_fluidSurfaces->SetVAO(vao);
        m_particleRenderPass->SetParticlesVAO(vao);
        m_depthPass->SetParticlesVAO(vao);
    }

    auto FluidRenderer::SetNumberOfParticles(unsigned n) -> void
    {
        // TODO: Same as SetVao()
        m_currentNumberOfParticles = n;
        m_fluidSurfaces->SetNumberOfParticles(n);
        m_particleRenderPass->SetNumberOfParticles(n);
        m_depthPass->SetNumberOfParticles(n);
    }

    auto FluidRenderer::SetFiltering(bool enabled) -> void
    {
        m_filteringEnabled = enabled;
    }

    auto FluidRenderer::Init() -> bool 
    {
        GLCall(glEnable(GL_PROGRAM_POINT_SIZE));

        m_fluidSurfaces = new FluidSurfaceRenderers(
            m_windowWidth, 
            m_windowHeight, 
            m_pointRadius,
            Camera::FAR_PLANE);

        m_textureRenderer = new TextureRenderer();

        m_surfaceSmoothingPass = new SurfaceSmoothingPass(
            m_windowWidth,
            m_windowHeight,
            4U,
            5U);

        m_particleRenderPass = new ParticleRenderPass(
            m_windowWidth,
            m_windowHeight,
            m_currentNumberOfParticles,
            m_pointRadius,
            m_currentVAO
        );

        m_depthPass = new DepthPass(
            m_windowWidth,
            m_windowHeight,
            m_currentNumberOfParticles,
            m_pointRadius,
            m_currentVAO
        );

        m_filterPass = new FilterPass(
            m_windowWidth,
            m_windowHeight,
            m_pointRadius
        );

        if  (!m_particleRenderPass->Init())
        {
            LOG_ERROR("Unable to initialize particle render pass.");
            return false;
        }

        if(!m_fluidSurfaces->Init())
        {
            LOG_ERROR("Unable to initialize fluid surfaces generation pass.");
            return false;
        } 
        
        if(!m_textureRenderer->Init()) 
        {
            LOG_ERROR("Unable to initialize texture renderer.");
            return false;  
        }
        if(!m_surfaceSmoothingPass->Init())
        {
            LOG_ERROR("Unable to initialize surface smoothing pass.");
            return false;
        }

        if(!m_depthPass->Init())
        {
            LOG_ERROR("Unable to initialize depth pass.");
            return false;
        }

        if(!m_filterPass->Init())
        {
            LOG_ERROR("Unable to initialize filter pass.");
            return false;
        }

        if (!InitUniformBuffers()) return false;

        SetUpLights();
        SetUpMaterial();

        return true;
    }

    auto FluidRenderer::ProcessInput(const SDL_Event& e) -> void 
    {
      m_cameraController.ProcessInput(e);

      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_f)
      {
        SetFiltering(!m_filteringEnabled);
      }
    }

    auto FluidRenderer::InitUniformBuffers() -> bool
    {
        GLCall(glGenBuffers(1, &m_uniformBufferCameraData));
        GLCall(glGenBuffers(1, &m_uniformBufferLights));
        GLCall(glGenBuffers(1, &m_uniformBufferMaterial));

        GLCall(glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBufferCameraData));
        GLCall(glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraData), nullptr, GL_DYNAMIC_DRAW));
        GLCall(glBindBufferRange(GL_UNIFORM_BUFFER, 0, m_uniformBufferCameraData, 0, sizeof(CameraData)));

        // Size of Lights is the total number of lights + an int that stores the number of lights in the scene
        constexpr int LIGHTS_UB_SIZE = sizeof(PointLight) * NUM_TOTAL_LIGHTS + sizeof(int);
        GLCall(glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBufferLights));
        GLCall(glBufferData(GL_UNIFORM_BUFFER, LIGHTS_UB_SIZE, nullptr, GL_STATIC_DRAW));
        GLCall(glBindBufferRange(GL_UNIFORM_BUFFER, 1, m_uniformBufferLights, 0, LIGHTS_UB_SIZE));

        GLCall(glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBufferMaterial));
        GLCall(glBufferData(GL_UNIFORM_BUFFER, sizeof(Material), nullptr, GL_STATIC_DRAW));
        GLCall(glBindBufferRange(GL_UNIFORM_BUFFER, 2, m_uniformBufferMaterial, 0, sizeof(Material)));

        GLCall(glBindBuffer(GL_UNIFORM_BUFFER, 0));

        if (!m_fluidSurfaces->SetUniformBuffer("CameraData", 0))
        {
          LOG_ERROR("Unable to set CameraData uniform buffer on FluidSurfaces renderer");
          return false;
        }
        if (!m_fluidSurfaces->SetUniformBuffer("Lights", 1))
        {
          LOG_ERROR("Unable to set Lights uniform buffer on FluidSurfaces renderer");
          return false;
        }
        if (!m_fluidSurfaces->SetUniformBuffer("Material", 2))
        {
          LOG_ERROR("Unable to set Material uniform buffer on FluidSurfaces renderer");
          return false;
        }

        if (!m_particleRenderPass->SetUniformBuffer("CameraData", 0))
        {
          LOG_ERROR("Unable to set CameraData uniform buffer on FluidSurfaces renderer");
          return false;
        }
        if (!m_particleRenderPass->SetUniformBuffer("Lights", 1))
        {
          LOG_ERROR("Unable to set Lights uniform buffer on FluidSurfaces renderer");
          return false;
        }
        if (!m_particleRenderPass->SetUniformBuffer("Material", 2))
        {
          LOG_ERROR("Unable to set Material uniform buffer on FluidSurfaces renderer");
          return false;
        }

        if (!m_depthPass->SetUniformBuffer("CameraData", 0))
        {
          LOG_ERROR("Unable to set CameraData uniform buffer on Depth renderer");
          return false;
        }
        return true;
    }

    auto FluidRenderer::SetUpLights() -> void
    {
      PointLight light;
      light.ambient  = { .2f, .2f, .2f, .2f };
      light.diffuse  = { 1.f, 1.f, 1.f, 1.f };
      light.specular = { 1.f, 1.f, 1.f, 1.f };

      light.position = { -10, 20.f, 10.f, 1.f };

      PointLight light2;
      light2.ambient  = { .1f, .1f, .1f, 1.f };
      light2.diffuse  = { .3f, .3f, .3f, 1.f };
      light2.specular = { 1.f, 1.f, 1.f, 1.f };
      light.position = { 10, -20.f, -10.f, 1.f };

      int numLights = 1;
      constexpr int numLightsFieldOffset = sizeof(PointLight) * NUM_TOTAL_LIGHTS;

      glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBufferLights);
      // Upload light 1
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PointLight), &light);

      // Upload light 2
      glBufferSubData(GL_UNIFORM_BUFFER, sizeof(PointLight), sizeof(PointLight), &light2);

      // Upload number of lights
      glBufferSubData(GL_UNIFORM_BUFFER, numLightsFieldOffset, sizeof(int), &numLights);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    auto FluidRenderer::SetUpMaterial() -> void
    {
      Material material;
      material.ambient   = { 0.1f, 0.1f, 0.1f, 1.f };
      material.diffuse   = { 0.f, 1.f, 0.f, 1.f };
      material.specular  = { 1.f, 1.f, 1.f, 1.f };
      material.shininess = 250;

      glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBufferMaterial);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Material), &material);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    auto FluidRenderer::Update() -> void
    {
      m_cameraController.Update();
    }
 
    auto FluidRenderer::Render() -> void
    {
        /*
        GLCall(glBindVertexArray(m_currentVAO));
        GLCall(glEnable(GL_DEPTH_TEST));
        GLCall(glDrawArrays(GL_POINTS, 0, m_currentNumberOfParticles));
        GLCall(glBindVertexArray(0));
        */

        // Upload cameraData uniform buffer
        GLCall(glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBufferCameraData));

        auto view = m_cameraController.GetCamera().GetViewMatrix();
        auto projection = m_cameraController.GetCamera().GetProjectionMatrix();

        GLCall(glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view)));
        GLCall(glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), 
              glm::value_ptr(projection)));
        GLCall(glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4) * 2,  sizeof(glm::mat4), 
              glm::value_ptr(glm::inverse(view))));
        GLCall(glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4) * 3,  sizeof(glm::mat4), 
              glm::value_ptr(glm::inverse(projection))));
        GLCall(glBindBuffer(GL_UNIFORM_BUFFER, 0));


        // for (int i = 0; i < 4; i++)
        // {
        //     m_surfaceSmoothingPass->SetUnfilteredSurfaces(
        //         m_surfaceSmoothingPass->GetSmoothedSurfaces());
        //     m_surfaceSmoothingPass->Render();
        // }

        m_particleRenderPass->Render();
        m_depthPass->Render();

        m_filterPass->SetInputTexture(m_depthPass->GetBuffer());
        m_filterPass->Render();

        m_textureRenderer->SetTexture(
            m_filteringEnabled ? m_filterPass->GetBuffer() : m_depthPass->GetBuffer()
        );

        Clear();
        m_textureRenderer->Render();
    }
}
