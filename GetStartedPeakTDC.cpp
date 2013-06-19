//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedPeakTDC.cpp: Example program for PeakTDC on AP240 modules
//
//----------------------------------------------------------------------------------------
//
//  Copyright Agilent Technologies Inc., 2000, 2001-2009
//
//////////////////////////////////////////////////////////////////////////////////////////

#include <AcqirisImport.h>
#include <AcqirisD1Import.h>
#include <stdio.h>


struct SegmentHeader
{
    unsigned long timeStampHi : 24;
    unsigned long tag : 8;
    unsigned long timeStampLo;
};

struct PeakData
{
    long amplitude : 20;
    unsigned long unused : 4;
    unsigned long tag : 8;
    unsigned long position;
};

struct GateHeader
{
    unsigned long position : 24;
    unsigned long tag : 8;
    unsigned long length;
};


void PrintoutData(char const *dataP, long length)
{
    char const * const dataEndP = dataP + length;

    while (dataP < dataEndP)
    {
        switch (dataP[3])
        {
        case 0x00:
            {
                GateHeader const *gateP = (GateHeader const *)dataP;
                dataP += sizeof(GateHeader);

                printf("# Gate: pos %lu len %lu\n", gateP->position, gateP->length);

                dataP += gateP->length;

            }
            break;

        case 0x10:
            {
                PeakData const *peakP = (PeakData const *)dataP;
                dataP += sizeof(PeakData);

                printf("Peak: pos %lu ampl %d\n", peakP->position / 16, peakP->amplitude / 16);

            }
            break;

        case 0x04:
            {
                SegmentHeader const *segP = (SegmentHeader const *)dataP;
                dataP += sizeof(SegmentHeader);

                printf("# Segment: %06lx:%08lx\n", segP->timeStampHi, segP->timeStampLo);

            }
            break;

        }
    }

}


int main(int argc, char *argv[])
{
	ViSession idInstrument;
	ViStatus status = Acqrs_InitWithOptions((ViRsrc)"PCI::INSTR0", VI_FALSE,
			VI_FALSE, "CAL=0", &idInstrument);

	if (status != VI_SUCCESS)
		return fprintf(stderr, "ERROR: Instrument not found.\n"), 1;
    
    status = Acqrs_calibrate(idInstrument);
    
	// Configure instrument mode and timebase
	ViInt32 modePeakTDC = 5;
	status = AcqrsD1_configMode(idInstrument, modePeakTDC, 0, 0);

    long const idChannel = 1;
    status = AcqrsD1_configChannelCombination(idInstrument, 2, idChannel);

    ViReal64 fullscale = 1.0;        // fullscale value in Volts
    ViReal64 offset = 0.0;           // offset value in Volts
    ViInt32 coupling = 3;            // coupling DC, 50 ohm
    ViInt32 bandwidth = 0;           // no bandwidth limit
    status = AcqrsD1_configVertical(idInstrument, idChannel, fullscale,
                                    offset, coupling, bandwidth);

	ViReal64 sampInterval = 0.5e-9;  // sampling interval in seconds
	ViReal64 delayTime = 0.0;        // trigger delay time in seconds
	status = AcqrsD1_configHorizontal(idInstrument, sampInterval, delayTime);

	// Configure edge trigger on channel 1
	ViInt32 trigClass = 0; 				
	ViInt32 sourcePattern = 0x1;		// Trigger on Channel 1	
	status = AcqrsD1_configTrigClass(idInstrument, trigClass, sourcePattern, 0x0, 0, 0.0, 0.0);

	// Configure the trigger conditions of channel 1 internal trigger
	ViInt32 trigCoupling = 0;			// DC coupling
	ViInt32 trigSlope = 0;				// Positive slope
	ViReal64 trigLevel = 10.0;			// Trigger level = +10% of FSR (i.e. + 50 mV)		
	status = AcqrsD1_configTrigSource(idInstrument, 1, trigCoupling, trigSlope, trigLevel, 0.0);

	// Configure analyzer parameters
	ViInt32 nbrSamples = 2048;
    ViInt32 nbrSegments = 1;
    ViInt32 invertData = 1;
	ViReal64 startPeak = 0.02;    // start hysteresis in Volts
	ViReal64 validPeak = 0.02;    // valid hysteresis in Volts
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "NbrSamples", &nbrSamples);
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "NbrSegments", &nbrSegments);
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "InvertData", &invertData);
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "StartDeltaPosPeakV", &startPeak);
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "ValidDeltaPosPeakV", &validPeak);

	// Configure gates parameters
	ViInt32 gateType = 1;         // 1 = user defined, 2 = threshold

	status = AcqrsD1_configAvgConfig(idInstrument, 1, "GateType", &gateType);

    ViInt32 const nbrGates = 3;
    ViInt32 const lenGate = 512;
    AqGateParameters gateParamP[nbrGates];

    for (int nGate = 0 ; nGate < nbrGates ; ++nGate)
    {
        gateParamP[nGate].GatePos = lenGate * (3 * nGate / 2);
        gateParamP[nGate].GateLength = lenGate;
    }

	status = AcqrsD1_configSetupArray(idInstrument, 1, 0, nbrGates, gateParamP);


	// Perform acquisitions (a few one for fun, only the last will be read)
    status = AcqrsD1_acquire(idInstrument);
    status = AcqrsD1_processData(idInstrument, 1, 1);
    status = AcqrsD1_waitForEndOfProcessing(idInstrument, 1000);
    status = AcqrsD1_processData(idInstrument, 1, 1);
    status = AcqrsD1_waitForEndOfProcessing(idInstrument, 1000);
    status = AcqrsD1_processData(idInstrument, 1, 1);
    status = AcqrsD1_waitForEndOfProcessing(idInstrument, 1000);
    status = AcqrsD1_processData(idInstrument, 1, 1);
    status = AcqrsD1_waitForEndOfProcessing(idInstrument, 1000);
    status = AcqrsD1_processData(idInstrument, 1, 2);
    status = AcqrsD1_waitForEndOfProcessing(idInstrument, 1000);

	printf("# Acquired %d segments, %d samples\n", (int)nbrSegments, (int)nbrSamples);


	// Readout gated waveform data (optional)
	long const nbrSamplePerSeg = (8 + lenGate) * nbrGates;
	long const nbrBytesAlloc = (8 + nbrSamplePerSeg) * nbrSegments;

	char *dataArray = new char[nbrBytesAlloc];

	AqReadParameters readParam;
	AqDataDescriptor dataDesc;
	readParam.dataType = ReadInt8;
	readParam.readMode = ReadModeSSRW;
	readParam.firstSegment = 0;
	readParam.nbrSegments = nbrSegments;
    readParam.firstSampleInSeg = 0;
	readParam.nbrSamplesInSeg = 0;
	readParam.segmentOffset = 0;
	readParam.dataArraySize = nbrBytesAlloc;
    readParam.segDescArraySize = 0;
    readParam.flags = 0;
	readParam.reserved = 0;
	readParam.reserved2 = 0;
	readParam.reserved3 = 0;

	status = AcqrsD1_readData(idInstrument, idChannel, &readParam, dataArray, &dataDesc, NULL);
    if (status < VI_SUCCESS)
        fprintf(stderr, "Error: readData: %d (%08x)\n", (int)status, (int)status);
	else
	{
		// Printout the data
		printf("# Read %d bytes: %d segments of %d values\n", (int)dataDesc.actualDataSize,
			(int)dataDesc.returnedSegments, (int)dataDesc.returnedSamplesPerSeg);

		PrintoutData(dataArray, dataDesc.actualDataSize);
	}

	// Readout peaks data
	readParam.dataType = ReadInt32;
	readParam.readMode = ReadModePeak;
	readParam.firstSegment = 0;
	readParam.nbrSegments = nbrSegments;
    readParam.firstSampleInSeg = 0;
	readParam.nbrSamplesInSeg = 0;
	readParam.segmentOffset = 0;
	readParam.dataArraySize = nbrBytesAlloc;
    readParam.segDescArraySize = 0;
    readParam.flags = 0;
	readParam.reserved = 0;
	readParam.reserved2 = 0;
	readParam.reserved3 = 0;

	status = AcqrsD1_readData(idInstrument, idChannel, &readParam, dataArray, &dataDesc, NULL);
    if (status < VI_SUCCESS)
        fprintf(stderr, "Error: readData: %d (%08x)\n", (int)status, (int)status);
	else
	{
		// Printout the data
		printf("# Read %d bytes: %d segments of %d values\n", (int)dataDesc.actualDataSize,
			(int)dataDesc.returnedSegments, (int)dataDesc.returnedSamplesPerSeg);

		PrintoutData(dataArray, dataDesc.actualDataSize);
	}

	delete[] dataArray;
    status = Acqrs_closeAll();

	return 0;
}

