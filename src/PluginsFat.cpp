// Plugin entry point (Fat Cat).
#include "common/FxBase.h"
#include "fatcat/FatCat.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FatCat(); }
