//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedSARmode.cpp : C++ demo program for Agilent Acqiris Digitizers
//----------------------------------------------------------------------------------------
//  Copyright (C) Agilent Technologies, Inc. 2007-2009
//
//////////////////////////////////////////////////////////////////////////////////////////
#include <fstream>
#include <iostream>
#include <sstream>
using std::cout; using std::endl;

#include "AcqirisImport.h" // Common Import for all Agilent Acqiris product families
#include "AcqirisD1Import.h" // Import for Agilent Acqiris Digitizers

// Macro for status code checking
char ErrMsg[256];
#define CHECK_API_CALL(f, s) { if (s)\
{ Acqrs_errorMessage(VI_NULL, s, ErrMsg, 256); cout<<f<<": "<<ErrMsg<<endl; } }


// Constants
static ViInt32 const nbrSARLoops = 10;

//////////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    ViStatus status; // All API functions return a status code that needs to be checked

    cout << "Agilent Acqiris - GetStartedSARmode\n\n";

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
    ViSession instrID; // Instrument handle

    status = Acqrs_InitWithOptions(rscStr, VI_FALSE, VI_FALSE, options, &instrID);
    CHECK_API_CALL("Acqrs_InitWithOptions", status);

    // Configuration of the digitizer ////////////////////////////////////////////////////

    // Configure timebase
    ViReal64 sampInterval = 1.e-8, delayTime = 0.0;
    status = AcqrsD1_configHorizontal(instrID, sampInterval, delayTime);
    CHECK_API_CALL("AcqrsD1_configHorizontal", status);

	// Enable SAR mode
    status = AcqrsD1_configMode(instrID, 0, 0, 10); // 10 = SAR
    CHECK_API_CALL("AcqrsD1_configMode (Does this device support SAR mode ?)", status);

    ViInt32 nbrSamples = 1000, nbrSegments = 1, nbrBanks = 10;
    status = AcqrsD1_configMemoryEx(instrID, 0, nbrSamples, nbrSegments, nbrBanks, 0);
    CHECK_API_CALL("AcqrsD1_configMemoryEx", status);

    // Configure vertical settings of channel 1
    ViReal64 fullScale = 1.0, offset = 0.0;
    ViInt32 coupling = 3, bandwidth = 0;
    status = AcqrsD1_configVertical(instrID, 1, fullScale, offset, coupling, bandwidth);
    CHECK_API_CALL("AcqrsD1_configVertical", status);

    // Configure edge trigger on channel 1
    status = AcqrsD1_configTrigClass(instrID, 0, 0x00000001, 0, 0, 0.0, 0.0);
    CHECK_API_CALL("AcqrsD1_configTrigClass", status);

    // Configure the trigger conditions of channel 1 (internal trigger)
    ViInt32 trigCoupling = 0, slope = 0;
    ViReal64 level = 20.0; // In % of vertical full scale when using internal trigger
    status = AcqrsD1_configTrigSource(instrID, 1, trigCoupling, slope, level, 0.0);
    CHECK_API_CALL("AcqrsD1_configTrigSource", status);

    // Acquisition of a waveform /////////////////////////////////////////////////////////

    // Start the acquisition
    status = AcqrsD1_acquire(instrID);
    CHECK_API_CALL("AcqrsD1_acquire", status);

    // Retrieval of the memory settings
    status = AcqrsD1_getMemory(instrID, &nbrSamples, &nbrSegments);
    CHECK_API_CALL("AcqrsD1_getMemory", status);

    // Definition of the read parameters for raw ADC readout
    AqReadParameters readPar;
    readPar.dataType = ReadInt8; // 8bit, raw ADC values data type
    readPar.readMode = ReadModeStdW; // Single-segment read mode
    readPar.firstSegment = 0;
    readPar.nbrSegments = 1;
    readPar.firstSampleInSeg = 0;
    readPar.nbrSamplesInSeg = nbrSamples;
    readPar.segmentOffset = 0;
    readPar.dataArraySize = (nbrSamples + 32) * sizeof(ViInt8); // Array size in bytes
    readPar.segDescArraySize = sizeof(AqSegmentDescriptor);

    readPar.flags = 0;
    readPar.reserved = 0;
    readPar.reserved2 = 0;
    readPar.reserved3 = 0;

    // Read the channel 1 waveform as raw ADC values
    AqDataDescriptor dataDesc;
    AqSegmentDescriptor segDesc;
    ViUInt64 timeStamp, previousStamp = 0;
    ViInt8 * adcArrayP = new ViInt8[readPar.dataArraySize];

    for (ViInt32 acq = 1; acq <= nbrSARLoops; acq++) // For a 'true' SAR mode, this loop is supposed to be infinite.
    {
        // Wait for interrupt to signal the end of acquisition with a timeout of 100ms
        // Note: The maximum value is 10 seconds. See 'Reference Manual' for more details.
        status = AcqrsD1_waitForEndOfAcquisition(instrID, 100);
        CHECK_API_CALL("AcqrsD1_waitForEndOfAcquisition", status);

        if (status != VI_SUCCESS)
        {
            // Acquisition did not complete successfully, force a software trigger.
            status = AcqrsD1_forceTrig(instrID);
            CHECK_API_CALL("AcqrsD1_forceTrigger", status);

            status = AcqrsD1_waitForEndOfAcquisition(instrID, 100);
            CHECK_API_CALL("AcqrsD1_waitForEndOfAcquisition", status);
        }

        // Readout of the waveform ///////////////////////////////////////////////////////////
        status = AcqrsD1_readData(instrID, 1, &readPar, adcArrayP, &dataDesc, &segDesc);
        CHECK_API_CALL("AcqrsD1_readData", status);

	    // Compute the 64bit timestamps value out of the high and low 32bit values
	    timeStamp = ((ViUInt64)segDesc.timeStampHi<<32) + segDesc.timeStampLo;

	    // Compares the timestamps with the one of the first segment of the previous acquisition (in ms)
	    ViReal64 diff = ViReal64(timeStamp - previousStamp) * 1.0e-9;

        if (previousStamp != 0)
	        cout << "Acq: " << acq << " - TimeStamp difference : " << diff << " ms." << endl;

        previousStamp = timeStamp;

        // Write the waveform into a file
        std::string fileName;
        std::stringstream acqStr;
        acqStr << acq;
        fileName += "AcqirisLoop" + acqStr.str() + ".data";
        std::ofstream outFile(fileName.c_str());
        outFile << "# Agilent Acqiris Waveform Channel 1, loop Nbr" << acq << "\n";
        outFile << "# Samples acquired: " << dataDesc.returnedSamplesPerSeg << "\n";
        ViInt32 firstPoint = dataDesc.indexFirstPoint;
        outFile << "# Voltage\n";
        for (ViInt32 i = firstPoint; i < firstPoint + dataDesc.returnedSamplesPerSeg; i++)
            outFile << int(adcArrayP[i]) * dataDesc.vGain - dataDesc.vOffset << "\n"; // Volts
        outFile.close();

        // The data has been read, free the current bank.
        status = AcqrsD1_freeBank(instrID, 0);
        CHECK_API_CALL("AcqrsD1_freeBank", status);
    }

    delete [] adcArrayP;

    status = AcqrsD1_stopAcquisition(instrID);
    CHECK_API_CALL("AcqrsD1_stopAcquisition", status);

    // Close the instrument
    status = Acqrs_close(instrID);
    CHECK_API_CALL("Acqrs_close", status);

    // Free remaining resources
    status = Acqrs_closeAll();
    CHECK_API_CALL("Acqrs_closeAll", status);

    return status;
}
