//
//  ApplicationOverlay.cpp
//  interface/src/ui/overlays
//
//  Created by Benjamin Arnold on 5/27/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "InterfaceConfig.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>

#include <glm/gtc/type_ptr.hpp>

#include <avatar/AvatarManager.h>
#include <DeferredLightingEffect.h>
#include <GLMHelpers.h>
#include <gpu/GLBackend.h>
#include <gpu/GLBackendShared.h>
#include <GLMHelpers.h>
#include <OffscreenUi.h>
#include <CursorManager.h>
#include <PerfStat.h>

#include "AudioClient.h"
#include "audio/AudioScope.h"
#include "Application.h"
#include "ApplicationOverlay.h"

#include "Util.h"
#include "ui/Stats.h"
#include "ui/AvatarInputs.h"

const vec4 CONNECTION_STATUS_BORDER_COLOR{ 1.0f, 0.0f, 0.0f, 0.8f };
const float CONNECTION_STATUS_BORDER_LINE_WIDTH = 4.0f;
static const float ORTHO_NEAR_CLIP = -10000;
static const float ORTHO_FAR_CLIP = 10000;

ApplicationOverlay::ApplicationOverlay()
{
    auto geometryCache = DependencyManager::get<GeometryCache>();
    _domainStatusBorder = geometryCache->allocateID();
    _magnifierBorder = geometryCache->allocateID();

    // Once we move UI rendering and screen rendering to different
    // threads, we need to use a sync object to deteremine when
    // the current UI texture is no longer being read from, and only
    // then release it back to the UI for re-use
    auto offscreenUi = DependencyManager::get<OffscreenUi>();
    connect(offscreenUi.data(), &OffscreenUi::textureUpdated, this, [&](GLuint textureId) {
        auto offscreenUi = DependencyManager::get<OffscreenUi>();
        offscreenUi->lockTexture(textureId);
        assert(!glGetError());
        std::swap(_uiTexture, textureId);
        if (textureId) {
            offscreenUi->releaseTexture(textureId);
        }
    });
}

ApplicationOverlay::~ApplicationOverlay() {
}

// Renders the overlays either to a texture or to the screen
void ApplicationOverlay::renderOverlay(RenderArgs* renderArgs) {
    PROFILE_RANGE(__FUNCTION__);
    CHECK_GL_ERROR();
    PerformanceWarning warn(Menu::getInstance()->isOptionChecked(MenuOption::PipelineWarnings), "ApplicationOverlay::displayOverlay()");

    // TODO move to Application::idle()?
    Stats::getInstance()->updateStats();
    AvatarInputs::getInstance()->update();

    buildFramebufferObject();

    // Execute the batch into our framebuffer

    gpu::Batch batch;

    // 1) bind the framebuffer
    //_overlayFramebuffer->bind();
    batch.setFramebuffer(_overlayFramebuffer);

    // 2) clear it...
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glm::vec4 color { 0.0f, 0.0f, 0.0f, 0.0f };
    float depth = 1.0f;
    int stencil = 0;
    batch.clearFramebuffer(gpu::Framebuffer::BUFFER_COLORS | gpu::Framebuffer::BUFFER_DEPTH, color, depth, stencil);
    //batch.clearColorFramebuffer(_overlayFramebuffer->getBufferMask(), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

    int width = _overlayFramebuffer ? _overlayFramebuffer->getWidth() : 0;
    int height = _overlayFramebuffer ? _overlayFramebuffer->getHeight() : 0;

    glViewport(0, 0, width, height);

    qDebug() << "ApplicationOverlay::renderOverlay()... ";
    qDebug() << "    renderArgs->batch:" << (void*)renderArgs->_batch;
    qDebug() << "    renderArgs->_viewport:" << renderArgs->_viewport.z << "," << renderArgs->_viewport.w;
    qDebug() << "    getDeviceSize:" << qApp->getDeviceSize();
    qDebug() << "    getCanvasSize:" << qApp->getCanvasSize();
    qDebug() << "    _overlayFramebuffer size:" << width << "," << height;

    // Now render the overlay components together into a single texture
    renderOverlays(renderArgs); // renders Scripts Overlay and AudioScope
    renderStatsAndLogs(renderArgs);  // currently renders nothing
    renderDomainConnectionStatusBorder(renderArgs); // renders the connected domain line
    renderQmlUi(renderArgs); // renders a unit quad with the QML UI texture

    //_overlayFramebuffer->release(); // now we're done for later composition
    batch.setFramebuffer(nullptr);
    CHECK_GL_ERROR();
}

void ApplicationOverlay::renderQmlUi(RenderArgs* renderArgs) {
    PROFILE_RANGE(__FUNCTION__);
    if (_uiTexture) {
        gpu::Batch batch;
        auto geometryCache = DependencyManager::get<GeometryCache>();

        geometryCache->useSimpleDrawPipeline(batch);
        batch.setProjectionTransform(mat4());
        batch.setModelTransform(Transform());
        batch.setViewTransform(Transform());
        batch._glBindTexture(GL_TEXTURE_2D, _uiTexture);

        geometryCache->renderUnitQuad(batch, glm::vec4(1));
        
        renderArgs->_context->syncCache();
        renderArgs->_context->render(batch);
    }
}

void ApplicationOverlay::renderOverlays(RenderArgs* renderArgs) {
    PROFILE_RANGE(__FUNCTION__);

    gpu::Batch batch;
    auto geometryCache = DependencyManager::get<GeometryCache>();
    geometryCache->useSimpleDrawPipeline(batch);
    auto textureCache = DependencyManager::get<TextureCache>();
    batch.setResourceTexture(0, textureCache->getWhiteTexture());
    int width = renderArgs->_viewport.z;
    int height = renderArgs->_viewport.w;
    mat4 legacyProjection = glm::ortho<float>(0, width, height, 0, -1000, 1000);
    batch.setProjectionTransform(legacyProjection);
    batch.setModelTransform(Transform());
    batch.setViewTransform(Transform());
    batch._glLineWidth(1.0f); // default
    
    {
        // Render all of the Script based "HUD" aka 2D overlays.
        // note: we call them HUD, as opposed to 2D, only because there are some cases of 3D HUD overlays, like the
        // cameral controls for the edit.js
        qApp->getOverlays().renderHUD(renderArgs);
        
        // Render the audio scope
        int width = _overlayFramebuffer ? _overlayFramebuffer->getWidth() : 0;
        int height = _overlayFramebuffer ? _overlayFramebuffer->getHeight() : 0;
        DependencyManager::get<AudioScope>()->render(renderArgs, width, height);
    }
    
    renderArgs->_context->syncCache();
    renderArgs->_context->render(batch);
}

void ApplicationOverlay::renderRearViewToFbo(RenderArgs* renderArgs) {
}

void ApplicationOverlay::renderRearView(RenderArgs* renderArgs) {
}

void ApplicationOverlay::renderStatsAndLogs(RenderArgs* renderArgs) {
    //  Display stats and log text onscreen

    // Determine whether to compute timing details

    /*
    //  Show on-screen msec timer
    if (Menu::getInstance()->isOptionChecked(MenuOption::FrameTimer)) {
        auto canvasSize = qApp->getCanvasSize();
        quint64 mSecsNow = floor(usecTimestampNow() / 1000.0 + 0.5);
        QString frameTimer = QString("%1\n").arg((int)(mSecsNow % 1000));
        int timerBottom =
            (Menu::getInstance()->isOptionChecked(MenuOption::Stats))
            ? 80 : 20;
        drawText(canvasSize.x - 100, canvasSize.y - timerBottom,
            0.30f, 0.0f, 0, frameTimer.toUtf8().constData(), WHITE_TEXT);
    }
    */
}

void ApplicationOverlay::renderDomainConnectionStatusBorder(RenderArgs* renderArgs) {
    auto geometryCache = DependencyManager::get<GeometryCache>();
    static std::once_flag once;
    std::call_once(once, [&] {
        QVector<vec2> points;
        static const float B = 0.99f;
        points.push_back(vec2(-B));
        points.push_back(vec2(B, -B));
        points.push_back(vec2(B));
        points.push_back(vec2(-B, B));
        points.push_back(vec2(-B));
        geometryCache->updateVertices(_domainStatusBorder, points, CONNECTION_STATUS_BORDER_COLOR);
    });
    auto nodeList = DependencyManager::get<NodeList>();
    if (nodeList && !nodeList->getDomainHandler().isConnected()) {
        gpu::Batch batch;
        auto geometryCache = DependencyManager::get<GeometryCache>();
        geometryCache->useSimpleDrawPipeline(batch);
        batch.setProjectionTransform(mat4());
        batch.setModelTransform(mat4());
        batch.setResourceTexture(0, DependencyManager::get<TextureCache>()->getWhiteTexture());
        batch._glLineWidth(CONNECTION_STATUS_BORDER_LINE_WIDTH);

        // TODO animate the disconnect border for some excitement while not connected?
        //double usecs = usecTimestampNow();
        //double secs = usecs / 1000000.0;
        //float scaleAmount = 1.0f + (0.01f * sin(secs * 5.0f));
        //batch.setModelTransform(glm::scale(mat4(), vec3(scaleAmount)));

        geometryCache->renderVertices(batch, gpu::LINE_STRIP, _domainStatusBorder);
        renderArgs->_context->syncCache();
        renderArgs->_context->render(batch);
    }
}

void ApplicationOverlay::buildFramebufferObject() {
    PROFILE_RANGE(__FUNCTION__);

    QSize desiredSize = qApp->getDeviceSize();
    int currentWidth = _overlayFramebuffer ? _overlayFramebuffer->getWidth() : 0;
    int currentHeight = _overlayFramebuffer ? _overlayFramebuffer->getHeight() : 0;
    QSize frameBufferCurrentSize(currentWidth, currentHeight);
    
    if (_overlayFramebuffer && desiredSize == frameBufferCurrentSize) {
        // Already built
        return;
    }
    
    if (_overlayFramebuffer) {
        _overlayFramebuffer.reset();
        _overlayDepthTexture.reset();
        _overlayColorTexture.reset();
    }

    _overlayFramebuffer = gpu::FramebufferPointer(gpu::Framebuffer::create());

   auto colorFormat = gpu::Element(gpu::VEC4, gpu::NUINT8, gpu::RGBA);
   auto width = desiredSize.width();
   auto height = desiredSize.height();

   auto defaultSampler = gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_POINT);
   _overlayColorTexture = gpu::TexturePointer(gpu::Texture::create2D(colorFormat, width, height, defaultSampler));
   _overlayFramebuffer->setRenderBuffer(0, _overlayColorTexture);


   auto depthFormat = gpu::Element(gpu::SCALAR, gpu::FLOAT, gpu::DEPTH);
   _overlayDepthTexture = gpu::TexturePointer(gpu::Texture::create2D(depthFormat, width, height, defaultSampler));

   _overlayFramebuffer->setDepthStencilBuffer(_overlayDepthTexture, depthFormat);
   
   
    /*    
    
    // This code essentially created a frame buffer, then sets a bunch of the parameters for that texture.
    _overlayFramebuffer = new QOpenGLFramebufferObject(fboSize, QOpenGLFramebufferObject::Depth);

    GLfloat borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    gpu::Batch batch;
    batch._glBindTexture(GL_TEXTURE_2D, getOverlayTexture());
    batch._glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    batch._glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    batch._glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    batch._glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    batch._glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    batch._glBindTexture(GL_TEXTURE_2D, 0);
    */

    /*
    glBindTexture(GL_TEXTURE_2D, getOverlayTexture());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindTexture(GL_TEXTURE_2D, 0);
    */



    /**** Example code...
    batch._glBindTexture(GL_TEXTURE_2D, texture);
    batch._glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    batch._glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);


    // this stuff is what would actually render from the texture

    geometryCache->useSimpleDrawPipeline(batch);
    batch.setViewportTransform(glm::ivec4(0, 0, deviceSize.width(), deviceSize.height()));
    batch.setModelTransform(Transform());
    batch.setViewTransform(Transform());
    batch.setProjectionTransform(mat4());
    batch._glBindTexture(GL_TEXTURE_2D, texture);
    geometryCache->renderUnitQuad(batch, vec4(vec3(1), _alpha));
    ****/

}
