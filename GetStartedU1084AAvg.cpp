//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedU1084AAvg.cpp : A simple example for using the U1084A Averager
//----------------------------------------------------------------------------------------
//  Copyright Agilent Technologies, Inc. 2009. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////////////////////
// This GetStarted puts the U1084A in Averaging mode and acquires a single averaged 
// trace. It then writes the acquired trace to a file called 'Acqiris.data' in the
// current directory. The functions in this file are in the order in which they are
// first called.

#include <iostream>
#include <fstream>

#include "AcqirisImport.h"
#include "AcqirisD1Import.h"


using std::cout;
using std::endl;
using std::ofstream;


#define NBR_SAMPLES 10240


// Macro for status code checking
char ErrMsg[256];
#define CHECK_API_CALL(f, s) { if (s)\
{ Acqrs_errorMessage(VI_NULL, s, ErrMsg, 256); cout<<f<<": "<<ErrMsg<<endl; } }
//////////////////////////////////////////////////////////////////////////////////////////
// Initialize the instrument identified by 'rsrcName'.
// We chose to initialize without calibration and do the calibration later.
void InitInstrument(ViRsrc rsrcName, ViSession& instrId)
{
    ViStatus status = VI_SUCCESS;

    // Initialize the instrument
    cout << "Initializing instrument" << endl;
    status = Acqrs_InitWithOptions(rsrcName, VI_FALSE, VI_FALSE, "CAL=0", &instrId);
    CHECK_API_CALL("InitWithOptions", status);

    // Display some basic information about the instrument
    char devName[32] = { 0 };
    ViInt32 serialNbr = 0, busNbr = 0, slotNbr = 0;
    status = Acqrs_getInstrumentData(instrId, devName, &serialNbr, &busNbr, &slotNbr);
    CHECK_API_CALL("getInstrumentData", status);
    cout << "Using: " << devName << " (SN=" << serialNbr << ") at bus #" << busNbr << ", slot #" << slotNbr << "\n" << endl;
}


//////////////////////////////////////////////////////////////////////////////////////////
void Configure(ViSession const instrId)
{
    ViStatus status = VI_SUCCESS;

    cout << "Configuring" << endl;

    // Configure the module for Averager mode
    ViInt32 const mode = 2;                 // Averager mode
    ViInt32 const modifier = 0;             // Not used for Averager
    ViInt32 const flags = 0;                // Normal mode
    status = AcqrsD1_configMode(instrId, mode, modifier, flags);
    CHECK_API_CALL("configMode", status);

    // Configure basic digitizer settings which also apply for Averager
    ViInt32 const nbrConv = 2;              // Combine channels 2 by 2
    ViInt32 const usedChannels = 0x1;       // Use Channel 1
    status = AcqrsD1_configChannelCombination(instrId, nbrConv, usedChannels);
    CHECK_API_CALL("configChannelCombination", status);

    ViReal64 const sampInterval = 2.5e-10;  // Sample interval 250ps <=> Sampling rate 4GS/s
    ViReal64 const trigDelay = 0.0;         // No trigger delay (has no effect for Averager)
    status = AcqrsD1_configHorizontal(instrId, sampInterval, trigDelay);
    CHECK_API_CALL("configHorizontal", status);

    ViInt32 const channel = 1;              // We use channel 1 (as chosen with configChannelCombination)
    ViReal64 const fullScale = 0.1;         // 100 mV full scale
    ViReal64 const offset = 0.0;            // No offset
    ViInt32 const coupling = 3;             // DC coupling 50 Ohm
    ViInt32 const bandwidth = 0;            // No bandwidth limit
    status = AcqrsD1_configVertical(instrId, channel, fullScale, offset, coupling, bandwidth);
    CHECK_API_CALL("configVertical", status);

    // Configure the trigger system
    ViInt32 const trigClass = 0;            // Edge trigger
    ViInt32 const trigSource = 0x80000000;  // Trig on external trigger
    ViInt32 const validatePattern = 0;      // Currently unused
    ViInt32 const holdType = 0;             // Currently unused
    ViReal64 const holdoffTime = 0.0;       // Currently unused
    ViReal64 const reserved = 0.0;          // Currently unused
    status = AcqrsD1_configTrigClass(instrId, trigClass, trigSource, validatePattern, holdType, holdoffTime, reserved);
    CHECK_API_CALL("configTrigClass", status);

    ViInt32 const trigChannel = -1;         // Configure external trigger
    ViInt32 const trigCoupling = 3;         // 50 Ohm DC coupling for trigger
    ViInt32 const trigSlope = 0;            // Trig on positive slope
    ViReal64 const trigLevel1 = 1000.0;     // Trig at 1 V
    ViReal64 const trigLevel2 = 0.0;        // Not used for edge trigger
    status = AcqrsD1_configTrigSource(instrId, trigChannel, trigCoupling, trigSlope, trigLevel1, trigLevel2);
    CHECK_API_CALL("configTrigSource", status);
    
    // Configure the 'Trigger Out' output
    ViInt32 connector = 9;                  // 'Trigger Out' connector
    ViInt32 signal = 0;                     // Offset in mV
    ViInt32 qualifier1 = 1;                 // Resynchronize to sampling clock
    ViReal64 qualifier2 = 0.0;              // Unused for 'Trigger Out', set to 0.0
    status = AcqrsD1_configControlIO(instrId, connector, signal, qualifier1, qualifier2);
    CHECK_API_CALL("configControlIO", status);
    
    // Configure the Control I/O connectors
    connector = 1;                  // Configure Control I/O A
    signal = 31;                    // Custom Signal From FPGA (Out)
    qualifier1 = 0;                 // Currently unused
    qualifier2 = 0.0;               // Currently unused
    status = AcqrsD1_configControlIO(instrId, connector, signal, qualifier1, qualifier2);
    CHECK_API_CALL("configControlIO(I/O A)", status);
    
    connector = 2;                          // Configure Control I/O B
    signal = 21;                            // Acquisition Active (Out)
    status = AcqrsD1_configControlIO(instrId, connector, signal, qualifier1, qualifier2);
    CHECK_API_CALL("configControlIO(I/O B)", status);
    
    connector = 3;                          // Configure Control I/O C
    signal = 1;                             // Enable Acquisition (In)
    status = AcqrsD1_configControlIO(instrId, connector, signal, qualifier1, qualifier2);
    CHECK_API_CALL("configControlIO(I/O C)", status);

    // Configure Averager-specific settings
    // - settings common to all channels
    ViInt32 nbrSamples = NBR_SAMPLES;       // Number of samples in the acquisition
    status = AcqrsD1_configAvgConfigInt32(instrId, 0, "NbrSamples", nbrSamples);
    CHECK_API_CALL("configAvgConfig(NbrSamples)", status);

    ViInt32 nbrWaveforms = 100;             // Number of acquisitions to accumulate
    status = AcqrsD1_configAvgConfigInt32(instrId, 0, "NbrWaveforms", nbrWaveforms);
    CHECK_API_CALL("configAvgConfig(NbrWaveforms)", status);

    ViInt32 trigAlways = 1;                 // Enable 'TrigAlways' mode
    status = AcqrsD1_configAvgConfigInt32(instrId, 0, "TrigAlways", trigAlways);
    CHECK_API_CALL("configAvgConfig(TrigAlways)", status);
    
    ViInt32 syncOnTrigOutSync = 1;          // Synchronize the acquisition to the resynchronized
                                            // 'Trigger Out' signal
    status = AcqrsD1_configAvgConfigInt32(instrId, 0, "SyncOnTrigOutSync", syncOnTrigOutSync);
    CHECK_API_CALL("configAvgConfig(syncOnTrigOutSync)", status);

    // - per-channel settings
    ViInt32 invertData = 1;                 // Invert the ADC data for channel 1
    status = AcqrsD1_configAvgConfigInt32(instrId, 1, "InvertData", invertData);
    CHECK_API_CALL("configAvgConfig(InvertData)", status);

    ViInt32 thresholdEnabled = 1;           // Enable threshold
    status = AcqrsD1_configAvgConfigInt32(instrId, 1, "ThresholdEnable", thresholdEnabled);
    CHECK_API_CALL("configAvgConfig(ThresholdEnable)", status);

    ViReal64 threshold = 0.0;               // Threshold value, in Volts
                                            // Note that the available values for 'threshold' depend on
                                            // the configured full scale and offset, as well as the
                                            // 'InvertData' setting, so these should be set before the
                                            // threshold.
    status = AcqrsD1_configAvgConfigReal64(instrId, 1, "Threshold", threshold);
    if (status == ACQIRIS_WARN_SETUP_ADAPTED)
    {
        // Due to the grain of the threshold setting, there is a good chance the requested value
        // cannot be applied exactly and will be adapted. In that case, we print out the actual value.
        status = AcqrsD1_getAvgConfigReal64(instrId, 1, "Threshold", &threshold);
        CHECK_API_CALL("getAvgConfig(Threshold)", status);
        cout << "Actual threshold applied: " << threshold << endl;
    } else
    {
        CHECK_API_CALL("configAvgConfig(Threshold)", status);
    }

    ViInt32 noiseBaseEnabled = 1;           // Enable base line substraction; For Averager, threshold must
                                            // enabled for base line substraction to work.
    status = AcqrsD1_configAvgConfigInt32(instrId, 1, "NoiseBaseEnable", noiseBaseEnabled);
    CHECK_API_CALL("configAvgConfig(NoiseBaseEnable)", status);

    ViReal64 noiseBase = 0.0;               // Base line value, in Volts; cannot be higher than threshold.
                                            // Note that the available values for 'noiseBase' depend on
                                            // the configured full scale and offset, as well as the
                                            // 'InvertData' setting, so these should be set before the
                                            // noise base.
    status = AcqrsD1_configAvgConfigReal64(instrId, 1, "NoiseBase", noiseBase);
    if (status == ACQIRIS_WARN_SETUP_ADAPTED)
    {
        // Due to the grain of the base line setting, there is a good chance the requested value
        // cannot be applied exactly and will be adapted. In that case, we print out the actual value.
        status = AcqrsD1_getAvgConfigReal64(instrId, 1, "NoiseBase", &noiseBase);
        CHECK_API_CALL("getAvgConfig(NoiseBase)", status);
        cout << "Actual noise base applied: " << noiseBase << endl;
    } else
    {
        CHECK_API_CALL("configAvgConfig(NoiseBase)", status);
    }

    // Now calibrate the instrument
    cout << "Calibrating" << endl;
    status = Acqrs_calibrate(instrId);
    CHECK_API_CALL("calibrate", status);
}


//////////////////////////////////////////////////////////////////////////////////////////
// Start the acquisition and wait for it to finish
void Acquire(ViSession const instrId)
{
    ViStatus status = VI_SUCCESS;

    cout << "Starting acquisition" << endl;
    status = AcqrsD1_acquire(instrId);
    CHECK_API_CALL("acquire", status);

    long timeout = 5000;    // 5 seconds timeout
    status = AcqrsD1_waitForEndOfAcquisition(instrId, timeout);

    if (status == ACQIRIS_ERROR_ACQ_TIMEOUT)
    {
        cout << "Acquisition timed out" << endl;
        status = AcqrsD1_stopAcquisition(instrId);
        CHECK_API_CALL("stopAcquisition", status);
    } else
    {
        CHECK_API_CALL("waitForEndOfAcquisition", status);
        cout << "Acquisition done" << endl;
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
// Read the data from the last acquisition
void ReadData(ViSession const instrId)
{
    ViStatus status = VI_SUCCESS;

    // Set up the read parameters for reading the accumulated waveform
    cout << "Reading average" << endl;
    ViUInt32 average[NBR_SAMPLES + 32];     // The buffer needs to be slightly larger than the
                                            // actual amount of requested data, to compensate for
                                            // alignment issues
    AqSegmentDescriptorAvg segDescAvg;

    AqReadParameters readParamAvg;
    // - general settings
    readParamAvg.dataType = ReadInt32;      // Read the average as 32 bit unsigned values
    readParamAvg.readMode = ReadModeAvgW;   // Use read mode 'Average'

    // - which samples to read
    readParamAvg.firstSampleInSeg = 0;      // We read the whole waveform
    readParamAvg.nbrSamplesInSeg = NBR_SAMPLES;

    // - which segments to read
    readParamAvg.firstSegment = 0;
    readParamAvg.nbrSegments = 1;
    readParamAvg.segmentOffset = 0;         // Not used for ReadModeAvgW

    // - size of the data and segment descriptor arrays
    readParamAvg.dataArraySize = sizeof(average);
    readParamAvg.segDescArraySize = sizeof(segDescAvg);

    // - these fields must be zero
    readParamAvg.flags = 0;
    readParamAvg.reserved = 0;
    readParamAvg.reserved2 = 0;
    readParamAvg.reserved3 = 0;

    // Read the average
    AqDataDescriptor dataDescAvg;
    status = AcqrsD1_readData(instrId, 1, &readParamAvg, average, &dataDescAvg, &segDescAvg);
    CHECK_API_CALL("readData(average)", status);


    // Write the data to the output file
    ofstream outFile("Acqiris.data");
    if (outFile)
    {
        outFile << "Average\n";
        for (int i = 0; i < dataDescAvg.returnedSamplesPerSeg; ++i)
        {
            // Note the use of 'dataDescAvg.indexFirstPoint' to locate the first
            // valid sample in the data array.
            ViInt32 avgSample = average[i + dataDescAvg.indexFirstPoint];
            outFile << avgSample << "\n";
        }
        outFile.close();
    }
    else
    {
        cout << "Could not open output file; discarding data" << endl;
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
// Allow the driver to do clean-up tasks before exiting
void CleanUp(ViSession const instrId)
{
    ViStatus status = VI_SUCCESS;

    cout << "Cleaning up" << endl;
    status = Acqrs_close(instrId);
    CHECK_API_CALL("close", status);
    status = Acqrs_closeAll();
    CHECK_API_CALL("closeAll", status);
    cout << "Done" << endl;
}


//////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    ViSession instrId = VI_NULL;
    
    InitInstrument((ViRsrc)"PCI::INSTR0", instrId);   // Adapt the 'rsrcName' as appropriate for your setup
    Configure(instrId);
    Acquire(instrId);
    ReadData(instrId);
    CleanUp(instrId);

    cout << "\nPress return to exit the program" << endl;
    getchar();

	return 0;
}

