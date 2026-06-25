// Minimal JUCE plugin reproducing the Ardour/Mixbus "first click after a pointer move is
// dropped" bug in an embedded WebBrowserComponent editor, on macOS.
//
// Symptom: in Ardour/Mixbus the first mousedown on a control that carries a native `title`
// tooltip, after the pointer moves onto it, is silently dropped — you must click twice. The
// same VST3 in Reaper, and this plugin's Standalone build, are unaffected.
//
// Why: hovering a control with a `title` installs macOS tooltip tracking, whose
// MouseEntered/MouseExited crossings leave GDK-quartz's per-display pointer-under-window
// cache stale. GDK then resolves the next mousedown against the wrong GdkWindow and consumes
// it in its own run loop, never calling [NSApplication sendEvent:], so the press reaches no
// NSView. The press re-establishes the state, so the second click works.
//
// The native label at the top shows, via a swizzle, how many mouseDown events the embedded
// WKWebView received vs how many Ardour's own GTK content view (GdkQuartzView) received. On
// a dropped click NEITHER advances — proving the press reaches no NSView.

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstring>
#include <optional>
#include <vector>
#include "NativeMouseProbe.h"

static const char* const kHtml = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><style>
  body { font-family: system-ui, sans-serif; background:#1a1d22; color:#dde; margin:0;
         padding:16px; font-size:13px; }
  h2 { margin:0 0 4px; font-size:15px; }
  p { color:#9aa; max-width:560px; line-height:1.45; margin:6px 0; }
  button { font-size:14px; padding:8px 14px; margin:6px 6px 0 0; border-radius:5px;
           border:1px solid #455; background:#28323f; color:#fff; cursor:pointer; }
  button:hover { background:#33414f; }
  .row { margin-top:8px; }
  #log { margin-top:16px; font:14px ui-monospace, Menlo, monospace; color:#9fe; }
  code { background:#222; padding:1px 4px; border-radius:3px; }
</style></head><body>
  <h2>Ardour / Mixbus — first click after a pointer move is dropped</h2>
  <p>On macOS, the first <code>mousedown</code> on a control after the pointer moves onto it
     is silently dropped — you must click twice; a second click on the same spot works, then
     moving to another control drops the first click again. The same VST3 in <b>Reaper</b>,
     and this plugin's <b>Standalone</b> build, are unaffected.</p>
  <p>The trigger is a native <code>title</code> tooltip on the control. Hovering it installs
     macOS tooltip tracking, whose crossing events leave Ardour's GTK-quartz
     pointer-under-window cache stale, so GDK consumes the next press in its own run loop
     without forwarding it to the plugin's NSView. The label above proves it: on a dropped
     click <b>neither native counter advances</b> — the press reaches no NSView at all.</p>

  <p><b>Click these alternately, moving the pointer between them.</b> The buttons
     <b>with</b> a <code>title</code> drop the first click after each move; the ones
     <b>without</b> a <code>title</code> are clean.</p>
  <div class="row">with <code>title</code>:
    <button title="tooltip">A</button><button title="tooltip">B</button><button title="tooltip">C</button>
  </div>
  <div class="row">no <code>title</code>:
    <button>X</button><button>Y</button><button>Z</button>
  </div>

  <div id="log">page clicks received: 0  (a dropped click does not increment this)</div>
  <script>
    var n = 0, log = document.getElementById('log');
    document.addEventListener('click', function (e) {
      if (e.target.tagName === 'BUTTON') {
        n++; log.textContent = 'page clicks received: ' + n + '   (last: ' + e.target.textContent + ')';
      }
    });
  </script>
</body></html>)HTML";

//==============================================================================
class ReproProcessor final : public juce::AudioProcessor
{
public:
    ReproProcessor()
        : juce::AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "WebView Click Repro"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReproProcessor)
};

//==============================================================================
class ReproEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ReproEditor(ReproProcessor& p)
        : juce::AudioProcessorEditor(p),
          web(juce::WebBrowserComponent::Options {}.withResourceProvider(
              [](const juce::String&) -> std::optional<juce::WebBrowserComponent::Resource> {
                  const auto* bytes = reinterpret_cast<const std::byte*>(kHtml);
                  return juce::WebBrowserComponent::Resource {
                      std::vector<std::byte>(bytes, bytes + std::strlen(kHtml)), "text/html" };
              }))
    {
        info.setColour(juce::Label::backgroundColourId, juce::Colour(0xff101216));
        info.setColour(juce::Label::textColourId, juce::Colours::aqua);
        addAndMakeVisible(info);
        addAndMakeVisible(web);
        web.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
        setSize(600, 380);
        startTimerHz(8); // refresh the native counters in the label
    }

    void resized() override
    {
        auto r = getLocalBounds();
        info.setBounds(r.removeFromTop(26));
        web.setBounds(r);
    }

    void timerCallback() override
    {
        info.setText("native mouseDowns reaching a view — WKWebView: " + juce::String(nativeMouseDowns())
                         + "    GdkQuartzView: " + juce::String(gtkViewMouseDowns()),
                     juce::dontSendNotification);
    }

private:
    juce::Label               info;
    juce::WebBrowserComponent  web;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReproEditor)
};

juce::AudioProcessorEditor* ReproProcessor::createEditor() { return new ReproEditor(*this); }

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReproProcessor(); }
