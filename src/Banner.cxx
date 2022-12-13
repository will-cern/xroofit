/*
* Project: xRooFit
* Author:
*   Will Buttinger, RAL 2022
*
* Copyright (c) 2022, CERN
*
* Redistribution and use in source and binary forms,
* with or without modification, are permitted according to the terms
* listed in LICENSE (http://roofit.sourceforge.net/license.txt)
 */

#include "RooFit.h"

#include "Rtypes.h"
#include "Riostream.h"
#include "TEnv.h"

#include "xRooFitVersion.h"


using namespace std;

Int_t doxroofitBanner();

static Int_t dummyFMB = doxroofitBanner() ;

Int_t doxroofitBanner()

{
#ifndef __xROOFIT_NOBANNER
    cout << "\033[1mxRooFit -- Create/Explore/Modify Workspaces -- Development ongoing\033[0m " << endl
         << "                xRooFit : http://gitlab.cern.ch/will/xroofit" << endl << "                Version: " << GIT_COMMIT_HASH
         << " [" << GIT_COMMIT_DATE << "]" <<
         endl;
#endif
    (void) dummyFMB;
    return 0 ;
}