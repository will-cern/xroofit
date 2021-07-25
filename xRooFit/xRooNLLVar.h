#pragma once

#include "xRooFit/xRooFit.h"

class RooAbsPdf;
class RooAbsData;
class RooAbsCollection;

#include "RooLinkedList.h"

class RooAbsReal;

class RooNLLVar;
class RooConstraintSum;

#include "Fit/FitConfig.h"

#include "xRooFit.h"

class xRooNLLVar : public std::shared_ptr<RooAbsReal> {

public:


    xRooNLLVar(const std::shared_ptr<RooAbsPdf>& pdf, const std::shared_ptr<RooAbsData>& data, const RooLinkedList& opts = RooLinkedList());
    ~xRooNLLVar();

    // whenever implicitly converted to a RooAbsReal we will make sure our globs are set
    RooAbsReal* get() const { return func().get(); }
    RooAbsReal* operator->() const { return get(); }

    void reinitialize();

    std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> generate(bool expected=false,int seed=0);
    //std::shared_ptr<const RooFitResult> snapshot();

    std::shared_ptr<const RooFitResult> minimize(const std::shared_ptr<ROOT::Fit::FitConfig>& = nullptr);

    void SetFitConfig(const std::shared_ptr<ROOT::Fit::FitConfig>& in) { fFitConfig = in; }
    std::shared_ptr<ROOT::Fit::FitConfig> fitConfig(); // returns fit config, or creates a default one if not existing

    double pll(const char* parName, double value, const xRooFit::Asymptotics::PLLType& pllType = xRooFit::Asymptotics::TwoSided);
    double sigma_mu(const char* parName, double value, double prime_value);

    std::shared_ptr<RooAbsReal> func() const; // will assign globs when called
    std::shared_ptr<RooAbsPdf> pdf() const { return fPdf; }
    RooAbsData* data() const; // returns the data hidden inside the NLLVar if there is some


    RooNLLVar* nllTerm() const;
    RooConstraintSum* constraintTerm() const;

    // change the dataset - will check globs are the same
    Bool_t setData(const std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>>& _data);

    std::shared_ptr<RooAbsPdf> fPdf;
    std::shared_ptr<RooAbsData> fData;
    std::shared_ptr<RooAbsCollection> fGlobs;

    RooLinkedList fOpts;
    std::shared_ptr<ROOT::Fit::FitConfig> fFitConfig;

    std::shared_ptr<RooAbsCollection> fFuncVars;
    std::shared_ptr<RooAbsCollection> fConstVars;
    std::string fFuncCreationLog; // messaging from when function was last created -- to save from printing to screen



};