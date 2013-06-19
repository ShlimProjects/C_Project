//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedTC84x.cpp
//
//  This is a very simple sample program to demonstrate the TC840 or TC842 instrument.
//  It starts an acquisition and writes the resulting data to a file called "TC84x.data".
//
//----------------------------------------------------------------------------------------
//
//  Copyright Agilent Technologies Inc. 2000, 200-2009
//
//////////////////////////////////////////////////////////////////////////////////////////

#include <AcqirisImport.h>
#include <AcqirisT3Import.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // Initialize the instrument
    ViSession idInstr;
    ViStatus status = Acqrs_InitWithOptions((ViRsrc)"PCI::INSTR0", VI_FALSE,
	    VI_FALSE, "CAL=0", &idInstr);

    if (status != VI_SUCCESS)
	    return printf("No instrument found.\n"), 1;

    // Configure mode standard
    ViInt32 modeStandard = 1;
    ViInt32 modifier = 1;     // enable multi-starts
    ViInt32 flags = 0;        // If you don't have an input signal, set this to 2
                              // to enable the on-board test signal
    status = AcqrsT3_configMode(idInstr, modeStandard, modifier, flags);

    // Configure channels, common on negative slope, other left on positive
    // This permits to use the same signal for the common as for the other channels
    ViInt32 slope = 1;
    ViReal64 threshold = 0.0;
    status = AcqrsT3_configChannel(idInstr, -1, slope, threshold, 0);

    // Prepare readout structure and buffer
    size_t const arraySize = 53248;
    char * const dataArrayP = new char[arraySize];

    AqT3ReadParameters readParam;
    ::memset(&readParam, 0, sizeof(readParam));
    readParam.dataArray = dataArrayP;
    readParam.dataSizeInBytes = arraySize;
    readParam.nbrSamples = 0;
    readParam.dataType = ReadReal64;
    readParam.readMode = AqT3ReadStandard;

    // Calibrate instrument (as we explicitely specified not to on init)
    status = Acqrs_calibrate(idInstr);

	FILE* outFile = fopen("TC84x.data", "w");
    if (outFile == NULL)
        return printf("Couldn't open output file \"TC84x.data\"");

    long const nbrAcq = 1;
    for (long nAcq = 0 ; nAcq < nbrAcq ; ++nAcq)
    {
	    // Start acquisitions
	    status = AcqrsT3_acquire(idInstr);

        // Wait for end of acquisition
        status = AcqrsT3_waitForEndOfAcquisition(idInstr, 8000);

        AqT3DataDescriptor dataDesc;
        ::memset(&dataDesc, 0, sizeof(dataDesc));

        // Read acquired data
        status = AcqrsT3_readData(idInstr, 0, &readParam, &dataDesc);
        printf("got %d samples\n", (int)dataDesc.nbrSamples);

        // We can assume the number of returned samples is a multiple of 12
        for (int n = 0 ; n < dataDesc.nbrSamples/12 ; ++n)
        {
            ViReal64 *countP = ((ViReal64 *)dataDesc.dataPtr) + n*12;

            fprintf(outFile, "%li", nAcq * 128 + n);

            // countP contains 12 64 bit floating point values corresponding
	        // to the time measure of the 12 channels in units of seconds
            for(int i=0; i<12; ++i)
				fprintf(outFile, "\t%g", countP[i]);
            fprintf(outFile, "\n");

	    }


    }

    fclose(outFile);

    // Stop the acquisition
    status = AcqrsT3_stopAcquisition(idInstr);

    // Close the instruments
    status = Acqrs_closeAll();

    // Cleanup readout buffer
    delete[] dataArrayP;

    return 0;
}

