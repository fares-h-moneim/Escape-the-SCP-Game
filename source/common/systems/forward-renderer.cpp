#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include "glad/gl.h"
#include <GLFW/glfw3.h>

namespace our
{
    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config)
    {
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Then we check if there is a sky texture in the configuration
        if (config.contains("sky"))
        {
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));

            // We can draw the sky using the same shader used to draw textured objects
            ShaderProgram *skyShader = new ShaderProgram();
            skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
            skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
            skyShader->link();

            // TODO: (Req 10) Pick the correct pipeline state to draw the sky
            //  Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion should we pick?
            //  We will draw the sphere from the inside, so what options should we pick for the face culling.
            PipelineState skyPipelineState{};
            skyPipelineState.faceCulling.enabled = true;
            skyPipelineState.depthTesting.enabled = true;
            skyPipelineState.depthTesting.function = GL_LEQUAL;
            skyPipelineState.faceCulling.culledFace = GL_FRONT;

            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
            std::string skyTextureFile = config.value<std::string>("sky", "");
            Texture2D *skyTexture = texture_utils::loadImage(skyTextureFile, false);

            // Setup a sampler for the sky
            Sampler *skySampler = new Sampler();
            skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
            skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Combine all the aforementioned objects (except the mesh) into a material
            this->skyMaterial = new TexturedMaterial();
            this->skyMaterial->shader = skyShader;
            this->skyMaterial->texture = skyTexture;
            this->skyMaterial->sampler = skySampler;
            this->skyMaterial->pipelineState = skyPipelineState;
            this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            this->skyMaterial->alphaThreshold = 1.0f;
            this->skyMaterial->transparent = false;
        }

        // Then we check if there is a postprocessing shader in the configuration
        if (config.contains("postprocess"))
        {
            // TODO: (Req 11) Create a framebuffer
            glGenFramebuffers(1, &this->postprocessFrameBuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->postprocessFrameBuffer);
            // TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
            //  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
            //  The depth format can be (Depth component with 24 bits).
            colorTarget = texture_utils::empty(GL_RGBA8, windowSize);
            depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT24, windowSize);

            // parameters of glFramebufferTexture2D are: target, attachment, textarget, texture, level
            // 1. target: Specifies the framebuffer target.
            // target must be GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER, or GL_FRAMEBUFFER.
            // GL_FRAMEBUFFER is equivalent to GL_DRAW_FRAMEBUFFER.
            // 2. attachment: Specifies the attachment point of the framebuffer.
            // 3. attachment: must be GL_COLOR_ATTACHMENTi, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT or
            // GL_DEPTH_STENCIL_ATTACHMENT.
            // 4. textarget: Specifies a 2D texture target, or for cube map textures, which face is to be attached.
            // 5. texture: Specifies the texture object to attach to the framebuffer attachment point named by attachment.
            // 6. level: Specifies the mipmap level of texture to attach.

            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(),
                                   0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(),
                                   0);
            // TODO: (Req 11) Unbind the framebuffer just to be safe
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            // Create a vertex array to use for drawing the texture
            glGenVertexArrays(1, &postProcessVertexArray);

            // Create a sampler to use for sampling the scene texture in the post processing shader
            Sampler *postprocessSampler = new Sampler();
            postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram *postprocessShader = new ShaderProgram();
            postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
            postprocessShader->attach(config.value<std::string>("postprocess", ""), GL_FRAGMENT_SHADER);
            postprocessShader->link();

            // Create a post processing material
            postprocessMaterial = new TexturedMaterial();
            postprocessMaterial->shader = postprocessShader;
            postprocessMaterial->texture = colorTarget;
            postprocessMaterial->sampler = postprocessSampler;
            // The default options are fine but we don't need to interact with the depth buffer
            // so it is more performant to disable the depth mask
            postprocessMaterial->pipelineState.depthMask = false;
        }
        if (config.contains("fx"))
        {
            Sampler *fxSampler = new Sampler();
            fxSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            fxSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            fxSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            fxSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram *fxShader = new ShaderProgram();
            fxShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
            fxShader->attach(config.value<std::string>("fx", ""), GL_FRAGMENT_SHADER);
            fxShader->link();

            // Create a post processing material
            fxMaterial = new TexturedMaterial();
            fxMaterial->shader = fxShader;
            fxMaterial->texture = colorTarget;
            fxMaterial->sampler = fxSampler;
            // The default options are fine but we don't need to interact with the depth buffer
            // so it is more performant to disable the depth mask
            fxMaterial->pipelineState.depthMask = false;

            startTime = glfwGetTime();
            duration = 4.0f; // The effect lasts for 5 seconds
        }
        if (config.contains("shake"))
        {
            Sampler *shakeSampler = new Sampler();
            shakeSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            shakeSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            shakeSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            shakeSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram *shakeShader = new ShaderProgram();
            shakeShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
            shakeShader->attach(config.value<std::string>("shake", ""), GL_FRAGMENT_SHADER);
            shakeShader->link();

            // Create a post processing material
            shakeMaterial = new TexturedMaterial();
            shakeMaterial->shader = shakeShader;
            shakeMaterial->texture = colorTarget;
            shakeMaterial->sampler = shakeSampler;
            // The default options are fine but we don't need to interact with the depth buffer
            // so it is more performant to disable the depth mask
            shakeMaterial->pipelineState.depthMask = false;
        }
    }

    void ForwardRenderer::setIntensity(float intensity)
    {
        this->intensity = intensity;
    }

    void ForwardRenderer::destroy()
    {
        // Delete all objects related to the sky
        if (skyMaterial)
        {
            delete skySphere;
            delete skyMaterial->shader;
            delete skyMaterial->texture;
            delete skyMaterial->sampler;
            delete skyMaterial;
        }
        // Delete all objects related to post processing
        if (postprocessMaterial)
        {
            glDeleteFramebuffers(1, &postprocessFrameBuffer);
            glDeleteVertexArrays(1, &postProcessVertexArray);
            delete colorTarget;
            delete depthTarget;
            delete postprocessMaterial->sampler;
            delete postprocessMaterial->shader;
            delete postprocessMaterial;
        }

        if (fxMaterial)
        {
            delete fxMaterial->sampler;
            delete fxMaterial->shader;
            delete fxMaterial;
        }
    }

    void ForwardRenderer::render(World *world)
    {
        // First of all, we search for a camera and for all the mesh renderers
        CameraComponent *camera = nullptr;
        std::vector<Entity *> lightsources;
        opaqueCommands.clear();
        transparentCommands.clear();
        Entity *flash;
        for (auto entity : world->getEntities())
        {
            // if the entity has a lightsource component, add it to the lightsources vector

            if (entity->getComponent<LightComponent>())
            {
                lightsources.push_back(entity);
            }
            if (entity->name == "flashlight")
            {
                flash = entity;
            }
            // If we hadn't found a camera yet, we look for a camera in this entity
            if (!camera)
                camera = entity->getComponent<CameraComponent>();
            // If this entity has a mesh renderer component
            if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer)
            {
                // We construct a command from it
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;
                // if it is transparent, we add it to the transparent commands list
                if (command.material->transparent)
                {
                    transparentCommands.push_back(command);
                }
                else
                {
                    // Otherwise, we add it to the opaque command list
                    opaqueCommands.push_back(command);
                }
            }
        }

        // If there is no camera, we return (we cannot render without a camera)
        if (camera == nullptr)
            return;

        // Get the camera's local-to-world transformation matrix
        glm::mat4 cameraMatrix = camera->getOwner()->getLocalToWorldMatrix();

        shakeMaterial->shader->use();

        double currentTime = glfwGetTime();
        double elapsedTime = currentTime - startTime;
        shakeMaterial->shader->set("u_time", (GLfloat)currentTime);
        shakeMaterial->shader->set("u_shakeIntensity", 0.03f);

        fxMaterial->shader->use();
        if (elapsedTime < duration)
        {
            float intensity = (elapsedTime / duration);
            fxMaterial->shader->set("intensity", intensity);
        }
        else
        {
            fxMaterial->shader->set("intensity", 1.0f);
        }

        // Transform the origin to get the camera's position in world space
        glm::vec3 cameraPosition = glm::vec3(cameraMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        // Print the camera position using printf

        // TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward direction
        //  HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one

        glm::vec3 cameraForward = camera->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraForward](const RenderCommand &first, const RenderCommand &second)
                  {
                      // TODO: (Req 9) Finish this function
                      //  HINT: the following return should return true "first" should be drawn before "second".
                     float distance1 = glm::dot(cameraForward, first.center);
                     float distance2 = glm::dot(cameraForward, second.center);
                      return distance1 > distance2; });
        flash->getComponent<LightComponent>()->direction = cameraForward;

        // TODO: (Req 9) Get the camera ViewProjection matrix and store it in VP
        glm::mat4 VP = camera->getProjectionMatrix(windowSize) * camera->getViewMatrix();
        // TODO: (Req 9) Set the OpenGL viewport using viewportStart and viewportSize
        glViewport(0, 0, windowSize.x, windowSize.y);
        // TODO: (Req 9) Set the clear color to black and the clear depth to 1
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f);
        // TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the framebuffer)
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        // If there is a postprocess material, bind the framebuffer
        if (postprocessMaterial)
        {
            // TODO: (Req 11) bind the framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->postprocessFrameBuffer);
        }

        // TODO: (Req 9) Clear the color and depth buffers

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // TODO: (Req 9) Draw all the opaque commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        // If there is a sky material, draw the sky

        glEnable(GL_DEPTH_TEST);
        for (auto command : opaqueCommands)
        {
            command.material->setup();
            glm::mat4 transform = VP * command.localToWorld;
            command.material->shader->set("transform", transform);
            if (dynamic_cast<LitMaterial *>(command.material))
            {
                // TODO: nothing to do here just placing this to tell u this is where you would look at if anything goes wrong
                // and fragment shader
                // light-sources.cpp if smth is not parsed right
                // lit material.cpp if smth is not parsed right for material
                glm::vec3 cameraPos = camera->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                command.material->shader->set("view_pos", cameraPos);
                command.material->shader->set("model", command.localToWorld);
                GLint temp = lightsources.size();
                command.material->shader->set("number_of_lights", temp);
                for (int i = 0; i < lightsources.size(); i++)
                {
                    LightComponent *light = lightsources[i]->getComponent<LightComponent>();
                    std::string lightType = light->lightType;
                    if (lightType == "spot")
                    {
                        command.material->shader->set("lights[" + std::to_string(i) + "].type", 2);
                        //
                        command.material->shader->set("lights[" + std::to_string(i) + "].position", glm::vec3(lightsources[i]->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
                        command.material->shader->set("lights[" + std::to_string(i) + "].direction", light->direction);
                        command.material->shader->set("lights[" + std::to_string(i) + "].cutOff", glm::cos(glm::radians(light->innerAngle)));
                        command.material->shader->set("lights[" + std::to_string(i) + "].outerCutOff", glm::cos(glm::radians(light->outerAngle)));
                        // the decay of light
                        command.material->shader->set("lights[" + std::to_string(i) + "].constant", light->constant);
                        command.material->shader->set("lights[" + std::to_string(i) + "].linear", light->linear);
                        command.material->shader->set("lights[" + std::to_string(i) + "].quadratic", light->quadratic);
                        //
                    }
                    if (lightType == "point")
                    {
                        command.material->shader->set("lights[" + std::to_string(i) + "].type", 1);
                        //
                        command.material->shader->set("lights[" + std::to_string(i) + "].constant", light->constant);
                        command.material->shader->set("lights[" + std::to_string(i) + "].linear", light->linear);
                        command.material->shader->set("lights[" + std::to_string(i) + "].quadratic", light->quadratic);
                        //
                        command.material->shader->set("lights[" + std::to_string(i) + "].position", glm::vec3(lightsources[i]->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
                    }
                    if (lightType == "directional")
                    {
                        command.material->shader->set("lights[" + std::to_string(i) + "].type", 0);
                        command.material->shader->set("lights[" + std::to_string(i) + "].direction", light->direction);
                    }
                    command.material->shader->set("lights[" + std::to_string(i) + "].color", light->lightColor);
                }
            }
            command.mesh->draw();
        }

        if (this->skyMaterial)
        {
            // TODO: (Req 10) setup the sky material
            this->skyMaterial->setup();
            // TODO: (Req 10) Get the camera position
            glm::mat4 M = camera->getOwner()->getLocalToWorldMatrix();
            // TODO: (Req 10) Create a model matrix for the sy such that it always follows the camera (sky sphere center = camera position)
            glm::mat4 modelMatrix = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                M[3][0], M[3][1], M[3][2], 1.0f);
            // TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
            //  We can acheive the is by multiplying by an extra matrix after the projection but what values should we put in it?
            glm::mat4 alwaysBehindTransform = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f);
            glm::mat4 transform = alwaysBehindTransform * VP * modelMatrix;
            // TODO: (Req 10) set the "transform" uniform
            this->skyMaterial->shader->set("transform", transform);
            // TODO: (Req 10) draw the sky sphere
            this->skySphere->draw();
        }
        // TODO: (Req 9) Draw all the transparent commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (auto command : transparentCommands)
        {
            command.material->setup();
            glm::mat4 transform = VP * command.localToWorld;
            command.material->shader->set("transform", transform);
            if (dynamic_cast<LitMaterial *>(command.material))
            {
                // TODO: nothing to do here just placing this to tell u this is where you would look at if anything goes wrong
                // and fragment shader
                // light-sources.cpp if smth is not parsed right
                // lit material.cpp if smth is not parsed right for material
                glm::vec3 cameraPos = camera->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                command.material->shader->set("view_pos", cameraPos);
                command.material->shader->set("model", command.localToWorld);
                GLint temp = lightsources.size();
                command.material->shader->set("number_of_lights", temp);
                for (int i = 0; i < lightsources.size(); i++)
                {
                    LightComponent *light = lightsources[i]->getComponent<LightComponent>();
                    std::string lightType = light->lightType;
                    if (lightType == "spot")
                    {
                        command.material->shader->set("lights[" + std::to_string(i) + "].type", 2);
                        //
                        command.material->shader->set("lights[" + std::to_string(i) + "].position", glm::vec3(lightsources[i]->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
                        command.material->shader->set("lights[" + std::to_string(i) + "].direction", light->direction);
                        command.material->shader->set("lights[" + std::to_string(i) + "].cutOff", glm::cos(glm::radians(light->innerAngle)));
                        command.material->shader->set("lights[" + std::to_string(i) + "].outerCutOff", glm::cos(glm::radians(light->outerAngle)));
                        // the decay of light
                        command.material->shader->set("lights[" + std::to_string(i) + "].constant", light->constant);
                        command.material->shader->set("lights[" + std::to_string(i) + "].linear", light->linear);
                        command.material->shader->set("lights[" + std::to_string(i) + "].quadratic", light->quadratic);
                        //
                    }
                    if (lightType == "point")
                    {
                        command.material->shader->set("lights[" + std::to_string(i) + "].type", 1);
                        //
                        command.material->shader->set("lights[" + std::to_string(i) + "].constant", light->constant);
                        command.material->shader->set("lights[" + std::to_string(i) + "].linear", light->linear);
                        command.material->shader->set("lights[" + std::to_string(i) + "].quadratic", light->quadratic);
                        //
                        command.material->shader->set("lights[" + std::to_string(i) + "].position", glm::vec3(lightsources[i]->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
                    }
                    if (lightType == "directional")
                    {
                        command.material->shader->set("lights[" + std::to_string(i) + "].type", 0);
                        command.material->shader->set("lights[" + std::to_string(i) + "].direction", light->direction);
                    }
                    command.material->shader->set("lights[" + std::to_string(i) + "].color", light->lightColor);
                }
            }
            command.mesh->draw();
        }
        // If there is a postprocess material, apply postprocessing
        if (postprocessMaterial)
        {
            // TODO: (Req 11) Return to the default framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            this->postprocessMaterial->setup();
            glBindVertexArray(this->postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        if (fxMaterial)
        {
            fxMaterial->shader->use();
            // TODO: (Req 11) Return to the default framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            this->fxMaterial->setup();
            glBindVertexArray(this->postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        if (shakeMaterial && shake)
        {
            shakeMaterial->shader->use();
            // TODO: (Req 11) Return to the default framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            this->shakeMaterial->setup();
            glBindVertexArray(this->postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }

}