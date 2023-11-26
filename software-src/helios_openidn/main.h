#pragma once

#include "./IDNNode.h"
#include "./DACHWInterface.h"
#include "./HWBridge.h"
#include "./SPIDevAdapter.h"
#include "./HeliosAdapter.h"
#include "./DummyAdapter.h"

#ifdef BUILD_PI
#include "../pi-specific/BCMAdapter.h"
#endif


void stopNetworkThread();
void startNetworkThread();