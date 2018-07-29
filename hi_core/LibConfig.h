/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#ifndef LIB_CONFIG_H
#define LIB_CONFIG_H

#include "BuildVersion.h"


#define USE_OLD_FILE_FORMAT 0
#define HI_USE_BACKWARD_COMPATIBILITY 1

#ifndef NUM_POLYPHONIC_VOICES
#if HISE_IOS
#define NUM_POLYPHONIC_VOICES 128
#else
#define NUM_POLYPHONIC_VOICES 256
#endif
#endif

#ifndef HISE_NUM_PLUGIN_CHANNELS
#define HISE_NUM_PLUGIN_CHANNELS 2
#endif


#define NUM_GLOBAL_VARIABLES 128
#define NUM_MIC_POSITIONS 8
#define NUM_MAX_CHANNELS 16


#ifndef HISE_SMOOTH_FIRST_MOD_BUFFER
#define HISE_SMOOTH_FIRST_MOD_BUFFER 0
#endif


#ifndef HISE_EVENT_RASTER
#define HISE_EVENT_RASTER 8
#endif

#ifndef HISE_CONTROL_RATE_DOWNSAMPLING_FACTOR
#define HISE_CONTROL_RATE_DOWNSAMPLING_FACTOR HISE_EVENT_RASTER
#endif

#ifndef HISE_USE_BACKWARDS_COMPATIBLE_TIMESTAMPS
#define HISE_USE_BACKWARDS_COMPATIBLE_TIMESTAMPS 1
#endif 

#ifndef HISE_USE_SQUARED_TIMEVARIANT_MOD_VALUES_BUG
#define HISE_USE_SQUARED_TIMEVARIANT_MOD_VALUES_BUG 1
#endif

#ifndef HISE_PLAY_ALL_CROSSFADE_GROUPS_WHEN_EMPTY
#define HISE_PLAY_ALL_CROSSFADE_GROUPS_WHEN_EMPTY 1
#endif

namespace hise { using namespace juce;




#if ENABLE_STARTUP_LOG
class StartupLogger
{
public:
	static void log(const String& message);;
private:
	static File getLogFile();
	static void init();
	static bool isInitialised;
	static double timeToLastCall;
};

#define LOG_START(x) StartupLogger::log(x);
#else
#define LOG_START(x) DBG(x);
#endif

#ifndef USE_RELATIVE_PATH_FOR_AUDIO_FILES
#define USE_RELATIVE_PATH_FOR_AUDIO_FILES 1
#endif


#define DONT_INCLUDE_FLOATING_LAYOUT_IN_FRONTEND 1

#if USE_BACKEND // make sure it's either backend or frontend...
#undef USE_FRONTEND
#define USE_FRONTEND 0
#endif

} // namespace hise

#endif
