//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedAvgVC.cpp : C++ demo program for Agilent Acqiris Averagers
//                        This example program shows how to use Agilent Acqiris Averagers
//----------------------------------------------------------------------------------------
//  Copyright Agilent Technologies, Inc. 1999-2010
//
//////////////////////////////////////////////////////////////////////////////////////////
#include <fstream>
#include <iostream>
#include <vector>
using std::cout; using std::endl;
using std::ofstream;
using std::vector;

#include "AcqirisImport.h" // Common Import for all Agilent Acqiris product families
#include "AcqirisD1Import.h" // Import for Agilent Acqiris Digitizers

// Macro for status code checking
char ErrMsg[256];
#define CHECK_API_CALL(f, s) { if (s)\
{ Acqrs_errorMessage(VI_NULL, s, ErrMsg, 256); cout<<f<<": "<<ErrMsg<<endl; } }

//////////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    ViStatus status = VI_SUCCESS; // All API functions return a status code that needs to be checked

    cout << "Agilent Acqiris - GetStartedAvgVC" << endl;


    // Search for instruments ////////////////////////////////////////////////////////////
    
    ViInt32 numInstr; // Number of instruments

    // Find all digitizers (virtual multi-instruments or individual instruments)

    // The following call will automatically detect ASBus connections between digitizers
    // and combine connected digitizers (of identical model!) into multi-instruments.
    // The call returns the number of multi-instruments and/or single instruments.
    status = AcqrsD1_multiInstrAutoDefine("", &numInstr);
    CHECK_API_CALL("AcqrsD1_multiInstrAutoDefine", status);

    if (numInstr < 1)
    {
        cout << "No instrument found!" << endl;
        return -1; // No instrument found
    }
    // Use the first digitizer
    ViChar rscStr[16] = "PCI::INSTR0"; // Resource string
    ViChar options[32] = ""; // No options necessary

    cout << numInstr << " Agilent Acqiris Digitizer(s) found on your PC\n";


    // Initialization of the instrument //////////////////////////////////////////////////
    
    ViSession instrID = VI_NULL; // Instrument handle

    status = Acqrs_InitWithOptions(rscStr, VI_FALSE, VI_FALSE, options, &instrID);
    CHECK_API_CALL("Acqrs_InitWithOptions", status);


    // Configuration of basic digitizer functionality ////////////////////////////////////

	ViReal64 const sampInterval = 10.0e-9;  // 100 MHz sampling rate
	ViReal64 const delayTime = 0.0;
	
	ViInt32 const nbrSamples = 1024;
	ViInt32 const nbrSegments = 1;
	
	ViInt32 const usedChannel = 1;          // This example uses channel 1

	ViInt32 const coupling = 3;             // DC coupling
	ViInt32 const bandwidth = 0;            // No bandwidth limit
	ViReal64 const fullScale = 0.5;         // 500 mV full scale
	ViReal64 const offset = 0.0;            // No offset
	
	ViInt32 const trigClass = 0;                        // Edge trigger
	ViInt32 const sourcePattern = 1 << (usedChannel-1); // Trigger on channel 1
	ViInt32 const trigCoupling = 0;                     // DC coupling
	ViInt32 const trigSlope = 0;                        // Positive slope
	ViReal64 const trigLevel = 10.0;	                // +10% of FSR (i.e. + 50 mV)
    
    // Apply the timebase configuration
	status = AcqrsD1_configHorizontal(instrID, sampInterval, delayTime);
	CHECK_API_CALL("AcqrsD1_configHorizontal", status);
    
    // Apply the memory configuration
    // This is not strictly necessary for averaging; segment size and count for averaging
    // mode are set below using 'AcqrsD1_configAvgConfig'.
	status = AcqrsD1_configMemory(instrID, nbrSamples, nbrSegments);
	CHECK_API_CALL("AcqrsD1_configMemory", status);

	// Apply vertical settings of channel 1
	status = AcqrsD1_configVertical(instrID, usedChannel, fullScale, offset, coupling, bandwidth);
	CHECK_API_CALL("AcqrsD1_configVertical", status);

	// Apply trigger settings for channel 1
	status = AcqrsD1_configTrigClass(instrID, trigClass, sourcePattern, 0x0, 0, 0.0, 0.0);
	CHECK_API_CALL("AcqrsD1_configTrigClass", status);

	// Apply trigger conditions for channel 1 internal trigger
	status = AcqrsD1_configTrigSource(instrID, usedChannel, trigCoupling, trigSlope, trigLevel, 0.0);
	CHECK_API_CALL("AcqrsD1_configTrigSource", status);


    // Configuration of averager functionality ///////////////////////////////////////////
    
	ViInt32 const mode = 2;                 // Averager mode
	ViInt32 const nbrWaveForms = 100;       // Average over 100 waveforms
	ViInt32 const ditherRange = 15;         // Enable dithering
	ViInt32 const trigResync = 1;           // Resynchronize trigger
	ViInt32 const startDelay = 0;
	ViInt32 const stopDelay = 0;

	status = AcqrsD1_configMode(instrID, mode, 0, 0);
	CHECK_API_CALL("AcqrsD1_configMode", status);
    
	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "NbrSamples", nbrSamples);
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(NbrSamples)", status);
	
	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "NbrSegments", nbrSegments);
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(NbrSegments)", status);
	
	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "StartDelay", startDelay);
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(StartDelay)", status);
	
	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "StopDelay", stopDelay);
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(StopDelay)", status);

	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "NbrWaveforms", nbrWaveForms); 
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(NbrWaveforms)", status);
	
	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "DitherRange", ditherRange); 
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(DitherRange)", status);
	
	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "TrigResync", trigResync); 
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(TrigResync)", status);
    
    
    // Configuration of Noise-Suppressed Averaging (NSA)  ////////////////////////////////
    
	ViInt32 const enableThreshold = 1;
	ViInt32 const enableNoiseBase = 1;
	ViReal64 const threshold = 0;       // Value is in Volts
	ViReal64 const noiseBase = -0.25;   // Value is in Volts
  
	// enable the NSA functionality 
	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "ThresholdEnable", enableThreshold); 
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(ThresholdEnable)", status);
	
	status = AcqrsD1_configAvgConfigInt32(instrID, 0, "NoiseBaseEnable", enableNoiseBase); 
	CHECK_API_CALL("AcqrsD1_configAvgConfigInt32(NoiseBaseEnable)", status);

	// set the base and threshold voltage values 
	status = AcqrsD1_configAvgConfigReal64(instrID, 0,"Threshold", threshold); 
	CHECK_API_CALL("AcqrsD1_configAvgConfigReal64(Threshold)", status);
	
	status = AcqrsD1_configAvgConfigReal64(instrID, 0, "NoiseBase", noiseBase); 
	CHECK_API_CALL("AcqrsD1_configAvgConfigReal64(NoiseBase)", status);


    // Acquisition ///////////////////////////////////////////////////////////////////////
    
	status = AcqrsD1_acquire(instrID);
	CHECK_API_CALL("AcqrsD1_acquire", status);
	
	// Wait for the interrupt to signal the end of the acquisition
	// with a timeout value of 10 seconds
	status = AcqrsD1_waitForEndOfAcquisition(instrID, 10000);

	if (status != VI_SUCCESS)
	{
        // Acquisition did not complete successfully
        // Note: 'AcqrsD1_forceTrig' (software trigger) is not supported for Averagers
        status = AcqrsD1_stopAcquisition(instrID);
        cout << "\nThe acquisition has been stopped - data invalid!" << endl;
        return status;
	}


    // Data Readout //////////////////////////////////////////////////////////////////////
    
	AqReadParameters		readParams;
	AqDataDescriptor		dataDesc;
	
	vector<ViInt32> dataArray(nbrSegments * (nbrSamples + 32));
	vector<AqSegmentDescriptorAvg> segDescArray(nbrSegments);

	readParams.dataType = ReadInt32;
	readParams.readMode = ReadModeAvgW;     
	readParams.firstSegment = 0;
	readParams.nbrSegments = nbrSegments;
	readParams.firstSampleInSeg = 0;
	readParams.nbrSamplesInSeg = nbrSamples;
	readParams.segmentOffset = nbrSamples;
	readParams.dataArraySize = static_cast<ViInt32>(dataArray.size() * sizeof(dataArray[0]));           // in bytes
	readParams.segDescArraySize = static_cast<ViInt32>(segDescArray.size() * sizeof(segDescArray[0]));  // in bytes

	readParams.flags = 0;
	readParams.reserved = 0;
	readParams.reserved2 = 0.0;
	readParams.reserved3 = 0.0;

	status = AcqrsD1_readData(instrID, usedChannel, &readParams, &dataArray[0], &dataDesc, &segDescArray[0]);
	CHECK_API_CALL("AcqrsD1_readData", status);

    // Save data to file /////////////////////////////////////////////////////////////////
	if(status >= 0)
	{
	    ofstream outFileD("Acqiris.data");

        size_t totalSamples = nbrSamples * nbrSegments;
        if (totalSamples + dataDesc.indexFirstPoint > dataArray.size())
            totalSamples = dataArray.size() - dataDesc.indexFirstPoint;
        
	    for (size_t i = 0; i < totalSamples; i++) 
		    outFileD << dataArray[dataDesc.indexFirstPoint + i] << endl;
		    
	    outFileD.close();
	    
	    cout << "Saved one averaged trace to \"Acqiris.data\"" << endl;
	}
    
    status = Acqrs_close(instrID);
	CHECK_API_CALL("Acqrs_close", status);
    
    status = Acqrs_closeAll();
	CHECK_API_CALL("Acqrs_closeAll", status);
	
    return 0;
}


