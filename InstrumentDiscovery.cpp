//////////////////////////////////////////////////////////////////////////////////////////
//
//  InstrumentDiscovery.cpp : C++ demo program for Agilent Acqiris Instruments
//----------------------------------------------------------------------------------------
//  (C) Copyright 1999, 2000-2009 Agilent Technologies, Inc.
//
//////////////////////////////////////////////////////////////////////////////////////////
#include <fstream>
#include <iostream>
using std::cout; using std::endl;
#include <string.h>

#include "AcqirisImport.h" // Common Import for all Agilent Acqiris product families

// Macro for status code checking
char ErrMsg[256];
#define CHECK_API_CALL(f, s) { if (s)\
{ Acqrs_errorMessage(VI_NULL, s, ErrMsg, 256); cout<<f<<": "<<ErrMsg<<endl; } }

//////////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    ViStatus status; // All API functions return a status code that needs to be checked

    cout << "Agilent Acqiris - InstrumentDiscovery" << endl;

    // Search for instruments ////////////////////////////////////////////////////////////
    ViInt32 numInstr; // Number of instruments
    ViConstString rscStr; // Resource string
    ViConstString options; // Initialization options

    bool simulation = false; // Simulation flag, set to true to simulate digitizers
    if (simulation)
    {
        // Set the simulation options BEFORE initializing simulated digitizers
        status = Acqrs_setSimulationOptions("M2M");
        CHECK_API_CALL("Acqrs_setSimulationOptions", status)

        // Simulate a digitizer
        numInstr = 1;
        rscStr = "PCI::DC271";
        options = "simulate=TRUE"; // Initialization options must be set to simulation
    }
    else 
    {
        // Find all instruments (virtual multi-instruments or individual instruments)

        // The following call will find the number of Agilent Acqiris instruments on the
        // computer (regardless of their connection(s) to ASBus for digitizers).
        status = Acqrs_getNbrInstruments(&numInstr);
        CHECK_API_CALL("Acqrs_getNbrInstruments", status);

        if (numInstr < 1)
        {
            cout << "No instrument found!" << endl;
            return -1; // No instrument found
        }
        options = "cal=0"; // Skip self-calibration
    }
    cout << numInstr << " Agilent Acqiris instrument(s) found on your PC\n\n";

    // Loop over instruments
    for (int i = 0; i < numInstr; i++)
    {
        char resource[20]; // Resource string
        ViSession instrID; // Instrument handle

        // Initialization of the instrument //////////////////////////////////////////////
        if (!simulation)
        {
            sprintf(resource, "PCI::INSTR%d", i);
            rscStr = resource;
        }
        status = Acqrs_InitWithOptions((ViRsrc)rscStr, VI_FALSE, VI_FALSE, options, &instrID);
        CHECK_API_CALL("Acqrs_InitWithOptions", status);

        // Retrieve some basic information about the instrument
        ViInt32 devType;
        status = Acqrs_getDevType(instrID, &devType);
        CHECK_API_CALL("Acqrs_getDevType", status);

        char name[32];
        ViInt32 serialNbr, busNbr, slotNbr;
        status = Acqrs_getInstrumentData(instrID, name, &serialNbr, &busNbr, &slotNbr);
        CHECK_API_CALL("Acqrs_getInstrumentData", status);

        ViInt32 nbrChannels;
        status = Acqrs_getNbrChannels(instrID, &nbrChannels);
        CHECK_API_CALL("Acqrs_getNbrChannels", status);

        ViInt32 nbrADCBits;
        status = Acqrs_getInstrumentInfo(instrID, "NbrADCBits", &nbrADCBits);
        CHECK_API_CALL("Acqrs_getInstrumentInfo", status);

        char options[256];
        status = Acqrs_getInstrumentInfo(instrID, "Options", options);
        CHECK_API_CALL("Acqrs_getInstrumentInfo", status);

        switch (devType)
        {
        case 1: // digitizer
            cout <<"Instrument "<<i+1<<" is a "<<name<<" digitizer";
            strlen(options) ? cout <<" ("<<options<<"),\n" : cout <<",\n";
            cout <<nbrChannels<<" channel(s), "<<nbrADCBits<<"bit resolution, SN "
                 <<serialNbr<<".\n\n";
            break;
        case 2: // RC2xx generator
            cout <<"Instrument "<<i+1<<" is a "<<name<<" generator";
            strlen(options) ? cout <<" ("<<options<<"),\n" : cout <<",\n";
            cout <<nbrChannels<<" channel(s), SN "<<serialNbr<<".\n\n";
            break;
        case 4: // time-to-digital converter
            cout <<"Instrument "<<i+1<<" is a "<<name<<" time-to-digital converter";
            strlen(options) ? cout <<" ("<<options<<"),\n" : cout <<",\n";
            cout <<nbrChannels<<" channel(s), SN "<<serialNbr<<".\n\n";
            break;
        default: // unknown type
            cout << "Instrument is of unknown type!?\n";
            break;
        }
        // Close the instrument
        status = Acqrs_close(instrID);
        CHECK_API_CALL("Acqrs_close", status);
    }

    // Free remaining resources
    status = Acqrs_closeAll();
    CHECK_API_CALL("Acqrs_closeAll", status);

    return status;
}
