#pragma once

#include "hal/Handle/ADC.h"
#include "hal/Handle/FPGA.h"
#include "hal/Handle/HICANN.h"


#include "sthal/FPGA.h"
#include "sthal/Wafer.h"
#include "sthal/HICANN.h"
#include "sthal/ReadFloatingGates.h"
#include "sthal/ExperimentRunner.h"
#include "sthal/Settings.h"

#include "sthal/DNCLoopbackConfigurator.h"
#include "sthal/DontProgramFloatingGatesHICANNConfigurator.h"
#include "sthal/HICANNConfigurator.h"
#include "sthal/HICANNReadoutConfigurator.h"
#include "sthal/HICANNv4Configurator.h"
#include "sthal/JustResetHICANNConfigurator.h"
#include "sthal/NOPHICANNConfigurator.h"
#include "sthal/NoFGConfigurator.h"
#include "sthal/NoResetNoFGConfigurator.h"
#include "sthal/ParallelHICANNNoFGConfigurator.h"
#include "sthal/ProgrammAndReadFloatingGatesConfigurator.h"
#include "sthal/VerifyConfigurator.h"
#include "sthal/ReadRepeaterTestdataConfigurator.h"

#include "sthal/MagicHardwareDatabase.h"
#include "sthal/YAMLHardwareDatabase.h"

#include "sthal/Calibration.h"

#include "sthal/ADCHandleFactory.h"
#include "sthal/ADCHwHandleFactory.h"
#include "sthal/ADCRemoteHwHandleFactory.h"

#include "sthal/ESSConfig.h"

#if defined(HAVE_ESS)
#include "sthal/ESSHardwareDatabase.h"
#include "sthal/ESSRunner.h"
#endif
