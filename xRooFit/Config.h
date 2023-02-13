/*
 * Project: RooFit
 * Author:
 *   Will Buttinger, RAL 2022
 *
 * Copyright (c) 2022, CERN
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted according to the terms
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)
 */

/*
 * This file MUST differ from the one in the ROOT build
 * It controls the definitions of the classes so that the ROOT classes
 * in the RoofFit::Detail::XRooFit namespace can coexist with
 * these classes if built on top of ROOT.
 */

// Define XROOFIT_USE_PRAGMA_ONCE if you want to use "pragma once" instead of
// header guards
#ifndef XROOFIT_USE_PRAGMA_ONCE
# define XROOFIT_USE_PRAGMA_ONCE
#endif

#ifndef xRooFit_Config_h_xRooFit
#define xRooFit_Config_h_xRooFit


// ROOT configuration: all of xRooFit is placed into a detail namespace
#ifdef XROOFIT_NAMESPACE
#undef XROOFIT_NAMESPACE
#endif




# ifdef XROOFIT_NAMESPACE
#  define BEGIN_XROOFIT_NAMESPACE namespace XROOFIT_NAMESPACE {
#  define END_XROOFIT_NAMESPACE } // namespace XROOFIT_NAMESPACE
# else
#ifdef BEGIN_XROOFIT_NAMESPACE
#undef BEGIN_XROOFIT_NAMESPACE
#endif
#  define BEGIN_XROOFIT_NAMESPACE
#ifdef END_XROOFIT_NAMESPACE
#undef END_XROOFIT_NAMESPACE
#endif
#  define END_XROOFIT_NAMESPACE
# endif

#endif
