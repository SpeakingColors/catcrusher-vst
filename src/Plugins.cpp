// Plugin entry point.
#include "common/FxBase.h"
#include "crusher/CatCrusher.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CatCrusher(); }
