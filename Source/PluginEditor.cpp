#include "PluginEditor.h"
#include <cmath>
namespace {const juce::Colour purple(0xffa12dff),panel(0xff1b1427),muted(0xff9a8fa9);}
SCTUNEEditor::SCTUNEEditor(SCTUNEProcessor& p):AudioProcessorEditor(&p),processor(p){
 setSize(980,580);
 setupKnob(retune,"RETUNE SPEED");setupKnob(humanize,"HUMANIZE");setupKnob(formant,"FORMANT COLOR");setupKnob(mix,"MIX");
 key.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},1);
 scale.addItemList({"MAJOR","MINOR","CHROMATIC"},1);
 for(auto* c:{&key,&scale}){c->setColour(juce::ComboBox::backgroundColourId,panel);c->setColour(juce::ComboBox::textColourId,juce::Colours::white);c->setColour(juce::ComboBox::outlineColourId,purple);addAndMakeVisible(*c);}
 preset.addItemList({"HARD SC","SOFT CLOUD","ROBOT MODE"},1);preset.setSelectedId(1,juce::dontSendNotification);
 preset.setColour(juce::ComboBox::backgroundColourId,panel);preset.setColour(juce::ComboBox::textColourId,juce::Colours::white);addAndMakeVisible(preset);
 preset.onChange=[this]{auto set=[this](const char* id,float value){auto* param=processor.state.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));};
  switch(preset.getSelectedId()){case 1:set("retune",100);set("humanize",0);set("mix",100);break;case 2:set("retune",45);set("humanize",70);set("mix",75);break;case 3:set("retune",100);set("humanize",0);set("mix",100);set("formant",-40);break;default:break;}};
 bypass.setColour(juce::ToggleButton::textColourId,juce::Colours::white);bypass.setColour(juce::ToggleButton::tickColourId,purple);addAndMakeVisible(bypass);
 aRetune=std::make_unique<SA>(processor.state,"retune",retune);aHumanize=std::make_unique<SA>(processor.state,"humanize",humanize);
 aFormant=std::make_unique<SA>(processor.state,"formant",formant);aMix=std::make_unique<SA>(processor.state,"mix",mix);
 aKey=std::make_unique<CA>(processor.state,"key",key);aScale=std::make_unique<CA>(processor.state,"scale",scale);
 aBypass=std::make_unique<BA>(processor.state,"bypass",bypass);
 startTimerHz(30);
}
void SCTUNEEditor::setupKnob(juce::Slider& s,const juce::String& name){s.setName(name);s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,25);s.setColour(juce::Slider::rotarySliderFillColourId,purple);s.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff41354d));s.setColour(juce::Slider::thumbColourId,juce::Colours::white);s.setColour(juce::Slider::textBoxTextColourId,juce::Colours::white);s.setColour(juce::Slider::textBoxBackgroundColourId,panel);addAndMakeVisible(s);}
void SCTUNEEditor::paint(juce::Graphics& g){
 g.fillAll(juce::Colour(0xff08070c));
 g.setColour(panel);g.fillRoundedRectangle(10,10,960,75,18);g.fillRoundedRectangle(10,95,960,280,16);g.fillRoundedRectangle(10,385,960,185,16);
 g.setColour(purple);g.setFont(juce::Font(juce::FontOptions(40,juce::Font::bold)));g.drawText("☁",27,18,65,55,juce::Justification::centred);
 g.setColour(juce::Colours::white);g.drawText("SCTUNE",105,18,300,55,juce::Justification::centredLeft);
 g.setFont(juce::Font(juce::FontOptions(13)));g.setColour(muted);g.drawText("PRESET",490,22,100,20,juce::Justification::centredLeft);
 // Animated waveform panel with live input/output peak meters.
 auto area=juce::Rectangle<float>(35,110,910,245);g.setColour(juce::Colour(0xff101019));g.fillRoundedRectangle(area,12);
 float level=juce::jlimit(0.0f,1.0f,processor.inputMeter.load());
 float output=juce::jlimit(0.0f,1.0f,processor.outputMeter.load());
 for(int i=0;i<155;i++){float x=50+i*5.65f;float h=12+std::abs(std::sin(i*0.39f)*std::cos(i*0.13f))*105*juce::jmax(0.08f,level);g.setColour(purple.withAlpha(0.22f+0.42f*std::abs(std::sin(i*0.1f))));g.fillRect(x,231-h,2.5f,h*2);}
 g.setColour(purple);juce::Path path;for(int i=0;i<300;i++){float x=50+i*2.92f;float y=230+10*std::sin(i*0.12f)+4*std::sin(i*0.44f);if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}g.strokePath(path,juce::PathStrokeType(2.0f));
 g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(12)));g.drawText("INPUT",47,326,100,18,juce::Justification::left);g.drawText("OUTPUT",845,326,90,18,juce::Justification::right);
 g.setColour(purple);g.fillRoundedRectangle(48,315,140*level,5,2);g.fillRoundedRectangle(788,315,140*output,5,2);
 float pitch=processor.detectedPitch.load();g.setColour(juce::Colours::white.withAlpha(0.75f));g.drawText(pitch>0?juce::String(pitch,1)+" Hz":"NO PITCH",430,327,120,20,juce::Justification::centred);
 g.setFont(juce::Font(juce::FontOptions(14,juce::Font::bold)));g.setColour(muted);
 g.drawText("KEY",45,396,60,22,juce::Justification::left);g.drawText("SCALE",215,396,70,22,juce::Justification::left);
 for(const auto& item:std::array<std::pair<juce::String,int>,4>{{{"HUMANIZE",395},{"RETUNE SPEED",550},{"FORMANT COLOR",705},{"MIX",855}}})g.drawText(item.first,item.second-65,535,145,25,juce::Justification::centred);
}
void SCTUNEEditor::resized(){preset.setBounds(560,27,210,35);bypass.setBounds(820,30,110,30);key.setBounds(45,424,130,36);scale.setBounds(215,424,155,36);humanize.setBounds(350,403,135,130);retune.setBounds(505,390,135,143);formant.setBounds(660,403,135,130);mix.setBounds(815,403,135,130);}
