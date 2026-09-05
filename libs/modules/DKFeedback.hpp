/*
 Copyright (C) 2019 Luis Fernando García Pérez [http://luiscript.com]

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#ifndef DKFeedback_hpp
#define DKFeedback_hpp

#include "DKModule.hpp"

/*
 Zoom/pan feedback - motion trails.

 Each frame the module composites the incoming texture, then re-draws its OWN previous
 output over it, translated by centerX/centerY and scaled by zoom. Repeating that every
 frame is what produces trails: a zoom slightly above 1.0 pushes the history outward,
 slightly below pulls it inward.

 Ported from the Feedback class of the 2023 Processing sketch "cdbvj". That version
 transformed the shared output canvas, which only produced feedback because every
 effect wrote into one buffer. ofxDarkKnight has no shared canvas - modules are
 wired - so this module has to hold its own history instead.

 Holding history is also why `decay` exists and the original had no equivalent:
 re-compositing history every frame needs attenuation or it never fades. decay is the
 alpha the previous frame is drawn with. 1.0 never forgets, 0.0 disables feedback.

 Draw ORDER is load-bearing. The input goes down first and the decayed history over
 it, not the reverse. Sources are frequently opaque - DKWebCam allocates its FBO as
 GL_RGB, with no alpha channel at all (DKWebCam.cpp:34) - so an input drawn last
 covers the history completely and the module degrades to a silent passthrough.

 This order also keeps the maths bounded: the result is
 decay*transform(prev) + (1-decay)*input, a leaky integrator. Compositing the history
 additively instead would sum to input/(1-decay), saturating to white - 20x at
 decay 0.95.

 Two FBOs are required because a framebuffer cannot be sampled and written in the
 same pass. The composite happens in `scratch` and is then blitted into `output`,
 rather than swapping which buffer is returned, because live DKWireConnections cache
 a raw ofFbo* - see the resolution-change crash fixed in #5. getFbo() must always
 return the same address, so `output` is the only buffer downstream ever sees.
 */
class DKFeedback : public DKModule
{
private:
    ofFbo output;
    ofFbo scratch;
    ofFbo * input;

    float centerX;
    float centerY;
    float zoom;
    float decay;

    void allocate(int w, int h)
    {
        // Value members, so their addresses survive reallocation and the pointer
        // handed out by getFbo() stays valid across a resolution change.
        output.allocate(w, h, GL_RGBA, 4);
        scratch.allocate(w, h, GL_RGBA, 4);
        reset();
    }

public:
    void setup()
    {
        input = nullptr;
        centerX = 0.0f;
        centerY = 0.0f;
        zoom = 1.01f;
        decay = 0.95f;

        allocate(getModuleWidth(), getModuleHeight());

        addInputConnection(DKConnectionType::DK_FBO);
        addOutputConnection(DKConnectionType::DK_FBO);
    }

    void update()
    {
    }

    /*
     The FBO work happens in draw(), not update(), to match DKLiveShader and DKPreview.

     Alpha blending is enabled explicitly and the style is pushed/popped around it.
     That is not defensive boilerplate: DKMediaPool::update() ends with
     ofDisableBlendMode() (DKMediaPool.cpp:119), which turns blending off globally, and
     modules are iterated from an unordered_map, so whether blending happened to be on
     when this module ran was down to iteration order. decay is applied as alpha, so
     without this the whole effect silently degraded to a passthrough.
     */
    void draw()
    {
        if (!output.isAllocated()) return;

        float w = output.getWidth();
        float h = output.getHeight();

        ofPushStyle();
        ofEnableAlphaBlending();

        scratch.begin();
        ofClear(0, 0, 0, 0);

        if (input != nullptr)
        {
            ofSetColor(255);
            input->draw(0, 0, w, h);
        }

        ofPushMatrix();
        ofTranslate(w * 0.5f + centerX * w * 0.5f, h * 0.5f + centerY * h * 0.5f);
        ofScale(zoom, zoom);
        ofSetColor(255, 255, 255, (int)(ofClamp(decay, 0.0f, 1.0f) * 255.0f));
        output.draw(-w * 0.5f, -h * 0.5f, w, h);
        ofPopMatrix();

        scratch.end();

        output.begin();
        ofClear(0, 0, 0, 0);
        ofSetColor(255);
        scratch.draw(0, 0);
        output.end();

        ofPopStyle();
    }

    void addModuleParameters()
    {
        addSlider("centerX", centerX, -1.0f, 1.0f, 0.0f, 4);
        addSlider("centerY", centerY, -1.0f, 1.0f, 0.0f, 4);
        addSlider("zoom", zoom, 0.0f, 2.0f, 1.01f, 4);
        addSlider("decay", decay, 0.0f, 1.0f, 0.95f, 4);
    }

    void setFbo(ofFbo * fbo)
    {
        input = fbo;
    }

    ofFbo * getFbo()
    {
        return &output;
    }

    void onResolutionChanged(int w, int h)
    {
        allocate(w, h);
    }

    void reset()
    {
        if (!output.isAllocated()) return;

        output.begin();
        ofClear(0, 0, 0, 0);
        output.end();

        scratch.begin();
        ofClear(0, 0, 0, 0);
        scratch.end();
    }
};

#endif /* DKFeedback_hpp */
