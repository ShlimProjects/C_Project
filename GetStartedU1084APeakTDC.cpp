//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedU1084APeakTDC.cpp : A simple example for using the U1084A PeakTDC
//----------------------------------------------------------------------------------------
//  Copyright Agilent Technologies, Inc. 2008, 2009. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////////////////////
// This GetStarted puts the U1084A in PeakTDC mode and acquires a single accumulated
// peak histogram. It then writes the acquired peak histogram, and the last trace which
// contributed to it, to a file called 'Acqiris.data' in the current directory.
// The functions in this file are in the order in which they are first called.

#include <iostream>
#include <fstream>

#include "AcqirisImport.h"
#include "AcqirisD1Import.h"


using std::cout;
using std::endl;
using std::ofstream;


#define NBR_SAMPLES 1024

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

    // Configure the module for PeakTDC mode
    ViInt32 const mode = 5;                 // PeakTDC mode
    ViInt32 const modifier = 0;             // Not used for PeakTDC
    ViInt32 const flags = 0;                // Not used for PeakTDC
    status = AcqrsD1_configMode(instrId, mode, modifier, flags);
    CHECK_API_CALL("configMode", status);

    // Configure basic digitizer settings which also apply for PeakTDC
    ViInt32 const nbrConv = 2;              // Combine channels 2 by 2
    ViInt32 const usedChannels = 0x1;       // Use Channel 1
    status = AcqrsD1_configChannelCombination(instrId, nbrConv, usedChannels);
    CHECK_API_CALL("configChannelCombination", status);

    ViReal64 const sampInterval = 2.5e-10;  // Sample interval 250ps <=> Sampling rate 4GS/s
    ViReal64 const trigDelay = 0.0;         // No trigger delay (has no effect for PeakTDC)
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
    ViInt32 const trigSource = 1;           // Trig on channel 1
    ViInt32 const validatePattern = 0;      // Currently unused
    ViInt32 const holdType = 0;             // Currently unused
    ViReal64 const holdoffTime = 0.0;       // Currently unused
    ViReal64 const reserved = 0.0;          // Currently unused
    status = AcqrsD1_configTrigClass(instrId, trigClass, trigSource, validatePattern, holdType, holdoffTime, reserved);
    CHECK_API_CALL("configTrigClass", status);

    ViInt32 const trigChannel = 1;          // Configure trigger for channel 1
    ViInt32 const trigCoupling = 0;         // DC coupling for trigger
    ViInt32 const trigSlope = 0;            // Trig on positive slope
    ViReal64 const trigLevel1 = 0.0;        // Trig on zero level
    ViReal64 const trigLevel2 = 0.0;        // Not used for edge trigger
    status = AcqrsD1_configTrigSource(instrId, trigChannel, trigCoupling, trigSlope, trigLevel1, trigLevel2);
    CHECK_API_CALL("configTrigSource", status);
    
    // Configure the Control I/O connectors
    ViInt32 connector = 1;                  // Configure Control I/O A
    ViInt32 signal = 31;                    // Custom Signal From FPGA (Out)
    ViInt32 qualifier1 = 0;                 // Currently unused
    ViReal64 qualifier2 = 0.0;              // Currently unused
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

    // Configure PeakTDC-specific settings
    // - settings common to all channels
    ViInt32 nbrSamples = NBR_SAMPLES;       // Number of samples in the acquisition
    status = AcqrsD1_configAvgConfig(instrId, 0, "NbrSamples", &nbrSamples);
    CHECK_API_CALL("configAvgConfig(NbrSamples)", status);

    ViInt32 nbrWaveforms = 100;             // Number of acquisitions to accumulate into the histogram
    status = AcqrsD1_configAvgConfig(instrId, 0, "NbrWaveforms", &nbrWaveforms);
    CHECK_API_CALL("configAvgConfig(NbrWaveforms)", status);

    ViInt32 trigAlways = 1;                 // Enable 'TrigAlways' mode
    status = AcqrsD1_configAvgConfig(instrId, 0, "TrigAlways", &trigAlways);
    CHECK_API_CALL("configAvgConfig(TrigAlways)", status);

    // - per-channel settings
    ViInt32 invertData = 1;                 // Invert the ADC data (permits to detect negative peaks)
    status = AcqrsD1_configAvgConfig(instrId, 1, "InvertData", &invertData);
    CHECK_API_CALL("configAvgConfig(InvertData)", status);

    ViReal64 startDeltaPosPeakV = 0.002;     // Minimum difference between successive samples to identify
                                             // the rising edge of a peak, in Volts.
    status = AcqrsD1_configAvgConfig(instrId, 1, "StartDeltaPosPeakV", &startDeltaPosPeakV);
    if (status == ACQIRIS_WARN_SETUP_ADAPTED)
    {
        // Due to the grain of the start delta setting, there is a good chance the requested value
        // cannot be applied exactly and will be adapted. In that case, we print out the actual value.
        status = AcqrsD1_getAvgConfig(instrId, 1, "StartDeltaPosPeakV", &startDeltaPosPeakV);
        CHECK_API_CALL("getAvgConfig(StartDeltaPosPeakV)", status);
        cout << "Actual start delta applied: " << startDeltaPosPeakV << endl;
    } else
    {
        CHECK_API_CALL("configAvgConfig(StartDeltaPosPeakV)", status);
    }

    ViReal64 validDeltaPosPeakV = 0.002;    // Minimum difference between successive samples to identify
                                            // the falling edge of a peak, in Volts.
    status = AcqrsD1_configAvgConfig(instrId, 1, "ValidDeltaPosPeakV", &validDeltaPosPeakV);
    if (status == ACQIRIS_WARN_SETUP_ADAPTED)
    {
        // Get the atually applied value, as for 'StartDeltaPosPeakV'
        status = AcqrsD1_getAvgConfig(instrId, 1, "ValidDeltaPosPeakV", &validDeltaPosPeakV);
        CHECK_API_CALL("getAvgConfig(ValidDeltaPosPeakV)", status);
        cout << "Actual valid delta applied: " << validDeltaPosPeakV << endl;
    } else
    {
        CHECK_API_CALL("configAvgConfig(ValidDeltaPosPeakV)", status);
    }

    ViInt32 noiseBaseEnabled = 1;           // Enable base line substraction
    status = AcqrsD1_configAvgConfig(instrId, 1, "NoiseBaseEnable", &noiseBaseEnabled);
    CHECK_API_CALL("configAvgConfig(NoiseBaseEnable", status);

    ViReal64 noiseBase = 0.0;               // Base line value, in Volts
                                            // Note that the available values for 'noiseBase' depend on
                                            // the configured full scale and offset, as well as the
                                            // 'InvertData' setting, so these should be set before the
                                            // noise base.
    status = AcqrsD1_configAvgConfig(instrId, 1, "NoiseBase", &noiseBase);
    if (status == ACQIRIS_WARN_SETUP_ADAPTED)
    {
        // Get the atually applied value, as for 'StartDeltaPosPeakV'
        status = AcqrsD1_getAvgConfig(instrId, 1, "NoiseBase", &noiseBase);
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

    status = AcqrsD1_waitForEndOfAcquisition(instrId, 10000);
    if (status == ACQIRIS_ERROR_ACQ_TIMEOUT)
    {
        cout << "Acquisition timed out" << endl;
        status = AcqrsD1_stopAcquisition(instrId);
        CHECK_API_CALL("stopAcquisition", status);
    } else
    {
        CHECK_API_CALL("waitForEndOfAcquisition", status);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
// Read the data from the last acquisition
void ReadData(ViSession const instrId)
{
    ViStatus status = VI_SUCCESS;

    // The U1084A permits to read the last trace which contributed to the histogram.
    // Note that this is not an average, but only the data acquired from the last
    // trigger. It can be useful to put the peaks into context with the acquisition
    // data.

    // Set up the read parameters for reading out the last trace
    cout << "Reading last trace" << endl;
    ViInt8 trace[NBR_SAMPLES + 32];     // The buffer needs to be slightly larger than the
                                        // actual amount of requested data, to compensate for
                                        // alignment issues
    AqSegmentDescriptor segDescTrace;
    AqReadParameters readParamTrace;

    // - general settings
    readParamTrace.dataType = ReadInt8;     // The last trace is in signed 8 bit format.
    readParamTrace.readMode = ReadModeStdW; // In PeakTDC mode, 'ReadModeStdW' will read the
                                            // last trace which contributed to the histogram.

    // - which samples to read
    readParamTrace.firstSampleInSeg = 0;    // We read the whole waveform
    readParamTrace.nbrSamplesInSeg = NBR_SAMPLES;

    // - which segments to read
    readParamTrace.firstSegment = 0;
    readParamTrace.nbrSegments = 1;         // ReadModeStdW cannot read more than one segment at a time
    readParamTrace.segmentOffset = 0;       // Not used for ReadModeStdW

    // - size of the data and segment descriptor arrays
    readParamTrace.dataArraySize = sizeof(trace);
    readParamTrace.segDescArraySize = sizeof(segDescTrace);

    // - these fields must be zero
    readParamTrace.flags = 0;
    readParamTrace.reserved = 0;
    readParamTrace.reserved2 = 0;
    readParamTrace.reserved3 = 0;

    // Now perform the actual readout of the last trace
    AqDataDescriptor dataDescTrace;
    status = AcqrsD1_readData(instrId, 1, &readParamTrace, trace, &dataDescTrace, &segDescTrace);
    CHECK_API_CALL("readData(trace)", status);


    // Set up the read parameters for reading the peak histogram
    cout << "Reading histogram" << endl;
    ViUInt32 histogram[NBR_SAMPLES + 32];
    AqSegmentDescriptorAvg segDescHisto;

    // Readout parameters are essentially the same, except read mode, data type and array sizes
    AqReadParameters readParamHisto = readParamTrace;
    readParamHisto.dataType = ReadInt32;
    readParamHisto.readMode = ReadModeHistogram;
    readParamHisto.dataArraySize = sizeof(histogram);
    readParamHisto.segDescArraySize = sizeof(segDescHisto);

    // Read the histogram
    AqDataDescriptor dataDescHisto;
    status = AcqrsD1_readData(instrId, 1, &readParamHisto, histogram, &dataDescHisto, &segDescHisto);
    CHECK_API_CALL("readData(histogram)", status);


    // Write the data to the output file
    int nbrReturnedSamples = (dataDescTrace.returnedSamplesPerSeg < dataDescHisto.returnedSamplesPerSeg)?
		dataDescTrace.returnedSamplesPerSeg : dataDescHisto.returnedSamplesPerSeg;
    ofstream outFile("Acqiris.data");
    if (outFile)
    {
        outFile << "Last Trace\tHistogram\n";
        for (int i = 0; i < nbrReturnedSamples; ++i)
        {
            ViInt8 traceSample = trace[i + dataDescTrace.indexFirstPoint];
            double histoSample = (double)histogram[i + dataDescHisto.indexFirstPoint] / dataDescHisto.nbrAvgWforms;
            outFile << (int)traceSample << "\t" << histoSample << "\n";
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

