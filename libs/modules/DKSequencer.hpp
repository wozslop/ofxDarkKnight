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

#ifndef DKSequencer_hpp
#define DKSequencer_hpp

#include "DKModule.hpp"

/*
 A step sequencer with a value per step, driven by an EXTERNAL clock.

 One knob per step, in the manner of a Korg SQ-1: dial in a value for each step and
 the sequencer emits the current step's value on "result". A step set to 0 is a rest,
 so no separate on/off switch per step is needed.

 Wire a phase ramp into "phase". ABLETON LINK and MIDI CLOCK IN both publish
 "1 Beat" / "2 Beats" / "1 Bar" / "2 Bars" / "4 Bars" / "8 Bars" as 0..1 ramps under
 identical slider names, so either drives this interchangeably. Which ramp you wire IS
 the clock division: 16 steps against "1 Bar" gives sixteenth notes, against "4 Bars"
 gives quarter notes. There is deliberately no internal clock and no tempo parameter -
 the transport is whatever you patch in, exactly as DKLfo consumes its "time".

 The step is addressed by phase rather than advanced by a counter, so the output is a
 pure function of the incoming ramp: in sync the instant it is patched, unaffected by
 pausing or by being added mid-session, and unable to drift. A counter would also need
 a pulse to count, and DKWire::update() only transfers DK_SLIDER - the framework has no
 trigger wire - so a counter was not implementable regardless.

 Because every step is an ordinary bound slider, each one is independently patchable:
 another LFO or sequencer can modulate a single step's value.
 */
class DKSequencer : public DKModule
{
private:
    static const int STEPS = 16;

    float phase;
    float result;
    int length;
    float stepValue[STEPS];

public:
    void setup()
    {
        phase = 0.0f;
        result = 0.0f;
        length = STEPS;

        // Default to a rising ramp rather than all zeroes, so the module does something
        // visible as soon as a clock is wired in instead of sitting silent until all
        // sixteen knobs have been set by hand.
        for (int i = 0; i < STEPS; i++) stepValue[i] = (float)i / (float)(STEPS - 1);
    }

    void update()
    {
        int active = (int)ofClamp((float)length, 1.0f, (float)STEPS);

        // Clamp below 1.0 so a phase of exactly 1.0 lands on the last step rather than
        // indexing one past the end of the pattern.
        int step = (int)(ofClamp(phase, 0.0f, 0.999999f) * active);

        result = stepValue[step];
    }

    void addModuleParameters()
    {
        gui->addSlider("phase", 0, 1, 0)->setPrecision(4)->bind(phase);
        gui->addSlider("length", 1, STEPS, STEPS)->bind(length);

        // Sixteen sliders in a collapsible folder keeps the panel compact. DKLiveShader
        // does the same for its shader parameters, which confirms sliders nested in a
        // folder are still reachable by the wire hit-test.
        ofxDatGuiFolder * steps = gui->addFolder("STEPS");
        for (int i = 0; i < STEPS; i++)
        {
            steps->addSlider(ofToString(i + 1), 0, 1, stepValue[i])
                 ->setPrecision(4)
                 ->bind(stepValue[i]);
        }

        gui->addSlider("result", 0, 1, 0)->setPrecision(4)->bind(result);
    }
};

#endif /* DKSequencer_hpp */
