// Plugin entry point.
#include "catalog/CATalog.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CATalogProcessor(); }
