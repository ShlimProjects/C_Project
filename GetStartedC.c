/*****************************************************************************************
 *
 *  GetStartedC.c : C demo program for Agilent Acqiris Digitizers
 *----------------------------------------------------------------------------------------
 *  (C) Copyright 1999, 2000-2009 Agilent Technologies, Inc.
 *
 ****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "AcqirisImport.h" /* Common Import for all Agilent Acqiris product families */
#include "AcqirisD1Import.h" /* Import for Agilent Acqiris Digitizers */

/* Macro for status code checking */
char ErrMsg[256];
#define CHECK_API_CALL(f, s) do { if (s) { Acqrs_errorMessage(VI_NULL, s, ErrMsg, 256); \
    fprintf(stderr, "%s: %s\n", f, ErrMsg); } } while (0)

/****************************************************************************************/
int main (int argc, char *argv[])
{
    ViStatus status; /* All API functions return a status code that needs to be checked */
    ViInt32 numInstr; /* Number of instruments */
    ViString rscStr; /* Resource string */
    ViString options; /* Initialization options */
    ViSession instrID; /* Instrument handle */

    ViReal64 sampInterval, delayTime; /* timebase */
    ViInt32 nbrSamples, nbrSegments; /* memory */
    ViReal64 fullScale, offset; /* vertical */
    ViInt32 coupling, bandwidth; /* vertical */
    ViInt32 trigCoupling, slope; /* trigger */
    ViReal64 level; /* trigger */

    AqReadParameters readPar;
    AqDataDescriptor dataDesc;
    AqSegmentDescriptor segDesc;
    char *adcArrayP = (char *)malloc(readPar.dataArraySize);
    FILE *outFile = fopen("Acqiris.data", "w");
    ViInt32 firstPoint, lastPoint, p;

    printf("%s\n", "Agilent Acqiris - GetStarted8bitSingleSegment");

    /*** Search for instruments *********************************************************/

    /* Find all digitizers (virtual multi-instruments or individual instruments) */

    /* The following call will automatically detect ASBus connections between digitizers
     * and combine connected digitizers (of identical model!) into multi-instruments.
     * The call returns the number of multi-instruments and/or single instruments. */
    status = AcqrsD1_multiInstrAutoDefine("", &numInstr);
    CHECK_API_CALL("AcqrsD1_multiInstrAutoDefine", status);

    if (numInstr < 1)
    {
        printf("%s\n", "No instrument found!");
        return -1; /* No instrument found */
    }
    /* Use the first digitizer */
    rscStr = "PCI::INSTR0";
    options = ""; /* No options necessary */

    printf("%d %s\n", (int)numInstr, "Agilent Acqiris Digitizer(s) found on your PC");

    /*** Initialization of the instrument ***********************************************/
    status = Acqrs_InitWithOptions(rscStr, VI_FALSE, VI_FALSE, options, &instrID);
    CHECK_API_CALL("Acqrs_InitWithOptions", status);

    /*** Configuration of the digitizer *************************************************/

    /* Configure timebase */
    sampInterval = 1.e-8, delayTime = 0.0;
    status = AcqrsD1_configHorizontal(instrID, sampInterval, delayTime);
    CHECK_API_CALL("AcqrsD1_configHorizontal", status);

    /* Configure memory */
    nbrSamples = 1000, nbrSegments = 1;
    status = AcqrsD1_configMemory(instrID, nbrSamples, nbrSegments);
    CHECK_API_CALL("AcqrsD1_configMemory", status);

    /* Configure vertical settings of channel 1 */
    fullScale = 1.0, offset = 0.0;
    coupling = 3, bandwidth = 0;
    status = AcqrsD1_configVertical(instrID, 1, fullScale, offset, coupling, bandwidth);
    CHECK_API_CALL("AcqrsD1_configVertical", status);

    /* Configure edge trigger on channel 1 */
    status = AcqrsD1_configTrigClass(instrID, 0, 0x00000001, 0, 0, 0.0, 0.0);
    CHECK_API_CALL("AcqrsD1_configTrigClass", status);

    /* Configure the trigger conditions of channel 1 (internal trigger) */
    trigCoupling = 0, slope = 0;
    level = 20.0; /* In % of vertical full scale when using internal trigger */
    status = AcqrsD1_configTrigSource(instrID, 1, trigCoupling, slope, level, 0.0);
    CHECK_API_CALL("AcqrsD1_configTrigSource", status);

    /*** Acquisition of a waveform ******************************************************/

    /* Start the acquisition */
    status = AcqrsD1_acquire(instrID);
    CHECK_API_CALL("AcqrsD1_acquire", status);

    /* Wait for interrupt to signal the end of acquisition with a timeout of 2 seconds
     * Note: The maximum value is 10 seconds. See 'Reference Manual' for more details. */
    status = AcqrsD1_waitForEndOfAcquisition(instrID, 2000);
    CHECK_API_CALL("AcqrsD1_waitForEndOfAcquisition", status);

    if (status != VI_SUCCESS)
    {
        /* Acquisition did not complete successfully
         * Note: In case of a timeout, 'AcqrsD1_forceTrig' (software trigger) may be used.
         * See 'Reference Manual' for more details. */
        status = AcqrsD1_stopAcquisition(instrID);
        CHECK_API_CALL("AcqrsD1_stopAcquisition", status);
        printf("\n%s\n", "The acquisition has been stopped - data invalid!");
        return status;
    }

    /*** Readout of the waveform ********************************************************/

    /* Retrieval of the memory settings */
    status = AcqrsD1_getMemory(instrID, &nbrSamples, &nbrSegments);
    CHECK_API_CALL("AcqrsD1_getMemory", status);

    /* Definition of the read parameters for raw ADC readout */
    readPar.dataType = ReadInt8; /* 8bit, raw ADC values data type */
    readPar.readMode = ReadModeStdW; /* Single-segment read mode */
    readPar.firstSegment = 0;
    readPar.nbrSegments = 1;
    readPar.firstSampleInSeg = 0;
    readPar.nbrSamplesInSeg = nbrSamples;
    readPar.segmentOffset = 0;
    readPar.dataArraySize = (nbrSamples + 32) * sizeof(char); /* Array size in bytes */
    readPar.segDescArraySize = sizeof(AqSegmentDescriptor);

    readPar.flags = 0;
    readPar.reserved = 0;
    readPar.reserved2 = 0;
    readPar.reserved3 = 0;

    /* Read the channel 1 waveform as raw ADC values */
    adcArrayP = (char *)malloc(readPar.dataArraySize);

    status = AcqrsD1_readData(instrID, 1, &readPar, adcArrayP, &dataDesc, &segDesc);
    CHECK_API_CALL("AcqrsD1_readData", status);

    /* Write the waveform into a file */
    outFile = fopen("Acqiris.data", "w");
    fprintf(outFile, "%s\n", "# Agilent Acqiris Waveform Channel 1");
    fprintf(outFile, "%s: %d\n", "# Samples acquired: ", (int)dataDesc.returnedSamplesPerSeg);
    fprintf(outFile, "%s\n", "# Voltage");
    firstPoint = dataDesc.indexFirstPoint;
    lastPoint = firstPoint + dataDesc.returnedSamplesPerSeg;
    for (p = firstPoint; p < lastPoint; ++p)
        fprintf(outFile, "%f\n", (int)adcArrayP[p] * dataDesc.vGain - dataDesc.vOffset); /* Volts */
    fclose(outFile);
    free(adcArrayP);

    /* Close the instrument */
    status = Acqrs_close(instrID);
    CHECK_API_CALL("Acqrs_close", status);
    /* Free remaining resources */
    status = Acqrs_closeAll();
    CHECK_API_CALL("Acqrs_closeAll", status);

    return status;
}
