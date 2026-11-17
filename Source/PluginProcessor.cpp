#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>


juce::AudioProcessorValueTreeState::ParameterLayout SCTUNEProcessor::layout()
{
    using F = juce::AudioParameterFloat;
    using I = juce::AudioParameterInt;
    using B = juce::AudioParameterBool;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addFloat = [&p](const char* id,
                         const char* name,
                         float min,
                         float max,
                         float initial)
    {
        juce::NormalisableRange<float> range(min, max, 1.0f);

        p.push_back(std::make_unique<F>(
            juce::ParameterID(id, 1),
            name,
            range,
            initial,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction(
                    [](float value, int)
                    {
                        return juce::String(
                            juce::roundToInt(value));
                    })
                .withValueFromStringFunction(
                    [](const juce::String& text)
                    {
                        return text.getFloatValue();
                    })));
    };

    addFloat("retune", "Retune Speed", 0, 100, 90);
    addFloat("humanize", "Humanize", 0, 100, 0);
    addFloat("formant", "Formant Color", -100, 100, 0);
    addFloat("mix", "Mix", 0, 100, 100);

    p.push_back(std::make_unique<I>("key", "Key", 0, 11, 0));
    p.push_back(std::make_unique<I>("scale", "Scale", 0, 2, 1));
    p.push_back(std::make_unique<B>("bypass", "Bypass", false));

    return {p.begin(), p.end()};
}

SCTUNEProcessor::SCTUNEProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"SCTUNE",layout()){}
bool SCTUNEProcessor::isBusesLayoutSupported(const BusesLayout& b) const { auto s=b.getMainOutputChannelSet(); return s==b.getMainInputChannelSet()&&(s==juce::AudioChannelSet::mono()||s==juce::AudioChannelSet::stereo()); }
void SCTUNEProcessor::prepareToPlay(double sampleRate,int){sr=sampleRate; ring.setSize(2,16384);ring.clear();ringWrite=0;analysis.fill(0);analysisWrite=analysisCounter=0;phase=0;ratio=desiredRatio=1;}
float SCTUNEProcessor::detectPitch() const {
 // Normalized difference function, simple monophonic pitch estimator.
 const int minLag=juce::jmax(24,(int)(sr/900));
 const int maxLag=juce::jmin(analysisSize/3,(int)(sr/85));
 float best=0; int bestLag=0;
 for(int lag=minLag;lag<=maxLag;lag++){
  double corr=0,a2=0,b2=0;
  for(int j=0;j<2048;j+=4){const float a=analysis[(analysisWrite+j)&(analysisSize-1)];const float b=analysis[(analysisWrite+j+lag)&(analysisSize-1)];corr+=a*b;a2+=a*a;b2+=b*b;}
  float score=(float)(corr/std::sqrt((a2+1e-10)*(b2+1e-10)));
  if(score>best){best=score;bestLag=lag;}
 }
 return best>0.7f && bestLag>0 ? (float)(sr/bestLag):0;
}
float SCTUNEProcessor::targetFrequency(float hz,int key,int scale) const {
 if(hz<=0)return 0;
 float note=69+12*std::log2(hz/440.0f),bestNote=std::round(note);
 if(scale!=2){static constexpr int major[]={0,2,4,5,7,9,11};static constexpr int minor[]={0,2,3,5,7,8,10};const int* degrees=scale==0?major:minor;
  float error=100;
  for(int oct=-2;oct<=10;oct++)for(int d=0;d<7;d++){float candidate=(float)(12*oct+key+degrees[d]);float e=std::abs(candidate-note);if(e<error){error=e;bestNote=candidate;}}
 }
 return 440*std::pow(2.0f,(bestNote-69)/12);
}
float SCTUNEProcessor::shiftedSample(int channel,float speed){
 const int size=ring.getNumSamples();const auto* r=ring.getReadPointer(channel);
 auto head=[&](float p){float delay=128.0f+p*2048.0f;float pos=(float)ringWrite-delay;while(pos<0)pos+=size;int a=(int)pos%size;int b=(a+1)%size;return r[a]+(r[b]-r[a])*(pos-std::floor(pos));};
 float p1=phase,p2=std::fmod(phase+0.5f,1.0f);
 float w1=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*p1);
 float w2=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*p2);
 (void)speed;
 return (head(p1)*w1+head(p2)*w2)/(w1+w2+1e-5f);
}
void SCTUNEProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&){
 juce::ScopedNoDenormals guard;
 const int n=buffer.getNumSamples(),channels=juce::jmin(2,buffer.getNumChannels());
 if(n==0||channels==0)return;
 const float retune=state.getRawParameterValue("retune")->load()/100;
 const float humanize=state.getRawParameterValue("humanize")->load()/100;
 const float formant=state.getRawParameterValue("formant")->load()/100;
 const float mix=state.getRawParameterValue("mix")->load()/100;
 const int key=(int)state.getRawParameterValue("key")->load(),scale=(int)state.getRawParameterValue("scale")->load();
 const bool bypass=state.getRawParameterValue("bypass")->load()>0.5f;
 float inPeak=0,outPeak=0;
 // Streaming analysis and per-sample dual-head delay pitch shifting.
 for(int i=0;i<n;i++){
  float mono=0;for(int ch=0;ch<channels;ch++){float s=buffer.getSample(ch,i);mono+=s;inPeak=juce::jmax(inPeak,std::abs(s));ring.setSample(ch,ringWrite,s);}
  mono/=channels;analysis[analysisWrite]=mono;analysisWrite=(analysisWrite+1)&(analysisSize-1);
  if(++analysisCounter>=1024){analysisCounter=0;float hz=detectPitch();detectedPitch.store(hz);
   if(hz>0){float target=targetFrequency(hz,key,scale);float strength=retune*(1-0.75f*humanize);desiredRatio=juce::jlimit(0.7f,1.4f,std::pow(target/hz,strength));}
   else desiredRatio=1;
  }
  ratio+=(desiredRatio-ratio)*(0.0001f+retune*0.003f);
  for(int ch=0;ch<channels;ch++){
   float dry=buffer.getSample(ch,i);
   float wet=shiftedSample(ch,ratio);
   // Formant Color is an intentionally modest timbral tilt, not true formant shifting.
   float colored=wet+formant*0.12f*(wet-dry);
   float output=bypass?dry:(dry*(1-mix)+colored*mix);
   buffer.setSample(ch,i,output);outPeak=juce::jmax(outPeak,std::abs(output));
  }
  phase+=(1-ratio)/2048.0f;phase-=std::floor(phase);
  ringWrite=(ringWrite+1)%ring.getNumSamples();
 }
 inputMeter.store(inPeak);outputMeter.store(outPeak);
}
juce::AudioProcessorEditor* SCTUNEProcessor::createEditor(){return new SCTUNEEditor(*this);}
void SCTUNEProcessor::getStateInformation(juce::MemoryBlock& b){auto xml=state.copyState().createXml();copyXmlToBinary(*xml,b);}
void SCTUNEProcessor::setStateInformation(const void* data,int bytes){auto xml=getXmlFromBinary(data,bytes);if(xml&&xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new SCTUNEProcessor();}
