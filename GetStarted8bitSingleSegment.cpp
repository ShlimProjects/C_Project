//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStarted8bitSingleSegment.cpp : C++ demo program for Agilent Acqiris Digitizers
//----------------------------------------------------------------------------------------
//  (C) Copyright 1999, 2000-2009 Agilent Technologies, Inc.
//
//////////////////////////////////////////////////////////////////////////////////////////
#include <fstream>
#include <iostream>
using std::cout; using std::endl;

#include "AcqirisImport.h" // Common Import for all Agilent Acqiris product families
#include "AcqirisD1Import.h" // Import for Agilent Acqiris Digitizers

// Macro for status code checking
char ErrMsg[256];
#define CHECK_API_CALL(f, s) { if (s)\
{ Acqrs_errorMessage(VI_NULL, s, ErrMsg, 256); cout<<f<<": "<<ErrMsg<<endl; } }

//////////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    ViStatus status; // All API functions return a status code that needs to be checked

    cout << "Agilent Acqiris - GetStarted8bitSingleSegment" << endl;

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

    ViInt32 nbrSamples = 1000, nbrSegments = 1;
    status = AcqrsD1_configMemory(instrID, nbrSamples, nbrSegments);
    CHECK_API_CALL("AcqrsD1_configMemory", status);

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

    // Wait for interrupt to signal the end of acquisition with a timeout of 2 seconds
    // Note: The maximum value is 10 seconds. See 'Reference Manual' for more details.
    status = AcqrsD1_waitForEndOfAcquisition(instrID, 2000);
    CHECK_API_CALL("AcqrsD1_waitForEndOfAcquisition", status);

    if (status != VI_SUCCESS)
    {
        // Acquisition did not complete successfully
        // Note: In case of a timeout, 'AcqrsD1_forceTrig' (software trigger) may be used.
        // See 'Reference Manual' for more details.
        status = AcqrsD1_stopAcquisition(instrID);
        CHECK_API_CALL("AcqrsD1_stopAcquisition", status);
        cout << "\nThe acquisition has been stopped - data invalid!\n";
        return status;
    }

    // Readout of the waveform ///////////////////////////////////////////////////////////

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
    ViInt8 * adcArrayP = new ViInt8[readPar.dataArraySize];

    status = AcqrsD1_readData(instrID, 1, &readPar, adcArrayP, &dataDesc, &segDesc);
    CHECK_API_CALL("AcqrsD1_readData", status);

    // Write the waveform into a file
    std::ofstream outFile("Acqiris.data");
    outFile << "# Agilent Acqiris Waveform Channel 1\n";
    outFile << "# Samples acquired: " << dataDesc.returnedSamplesPerSeg << "\n";
    ViInt32 firstPoint = dataDesc.indexFirstPoint;
    outFile << "# Voltage\n";
    for (ViInt32 i = firstPoint; i < firstPoint + dataDesc.returnedSamplesPerSeg; i++)
        outFile << int(adcArrayP[i]) * dataDesc.vGain - dataDesc.vOffset << "\n"; // Volts
    outFile.close();
    delete [] adcArrayP;

    // Close the instrument
    status = Acqrs_close(instrID);
    CHECK_API_CALL("Acqrs_close", status);
    // Free remaining resources
    status = Acqrs_closeAll();
    CHECK_API_CALL("Acqrs_closeAll", status);

    return status;
}
