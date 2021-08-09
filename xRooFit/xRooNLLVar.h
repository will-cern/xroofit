#pragma once

#include "xRooFit/xRooFit.h"

class RooAbsPdf;
class RooAbsData;
class RooAbsCollection;

#include "RooLinkedList.h"

class RooAbsReal;

class RooNLLVar;
class RooConstraintSum;
class RooRealVar;

#include "Fit/FitConfig.h"

#include "xRooFit.h"
#include <map>

class xRooNLLVar : public std::shared_ptr<RooAbsReal> {

public:


    xRooNLLVar(const std::shared_ptr<RooAbsPdf>& pdf, const std::shared_ptr<RooAbsData>& data, const RooLinkedList& opts = RooLinkedList());
    ~xRooNLLVar();

    // whenever implicitly converted to a RooAbsReal we will make sure our globs are set
    RooAbsReal* get() const { return func().get(); }
    RooAbsReal* operator->() const { return get(); }

    void reinitialize();

    std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> getData() const; // returns pointer to data and snapshot of globs
    std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> generate(bool expected=false,int seed=0);
    //std::shared_ptr<const RooFitResult> snapshot();

    std::shared_ptr<const RooFitResult> minimize(const std::shared_ptr<ROOT::Fit::FitConfig>& = nullptr);

    void SetFitConfig(const std::shared_ptr<ROOT::Fit::FitConfig>& in) { fFitConfig = in; }
    std::shared_ptr<ROOT::Fit::FitConfig> fitConfig(); // returns fit config, or creates a default one if not existing

    std::pair<double,double> pll(const char* parName, double value, const xRooFit::Asymptotics::PLLType& pllType = xRooFit::Asymptotics::TwoSided);
    std::pair<double,double> sigma_mu(const char* parName, double value, double prime_value);

    class xRooHypoTestResult {
    public:
        std::pair<double,double> pll() const; // observed test statistic value
        std::pair<double,double> sigma_mu() const; // estimate of sigma_mu parameter

        // leave nSigma=NaN for observed p-value
        double pNull_asymp(double nSigma=std::numeric_limits<double>::quiet_NaN()) const;
        double pAlt_asymp(double nSigma=std::numeric_limits<double>::quiet_NaN()) const;
        double pCLs_asymp(double nSigma=std::numeric_limits<double>::quiet_NaN()) const { return (pNull_asymp(nSigma)==0) ? 0 : (pNull_asymp(nSigma)/pAlt_asymp(nSigma)); }

        double pNull_toys(double nSigma=std::numeric_limits<double>::quiet_NaN()) const;
        double pAlt_toys(double nSigma=std::numeric_limits<double>::quiet_NaN()) const;
        double pCLs_toys(double nSigma=std::numeric_limits<double>::quiet_NaN()) const { return (pNull_toys(nSigma)==0) ? 0 : (pNull_toys(nSigma)/pAlt_toys(nSigma)); }

        void addNullToys(xRooNLLVar& nllFunc, int nToys);
        void addAltToys(xRooNLLVar& nllFunc, int nToys);

        RooRealVar& mu_hat() const; // throws exception if ufit not available

        std::string fPOIName;
        xRooFit::Asymptotics::PLLType fPllType;
        double fNullVal=1; double fAltVal=0;

        std::shared_ptr<const RooFitResult> ufit;
        std::shared_ptr<const RooFitResult> null_cfit; // required for test statistic value
        std::shared_ptr<const RooFitResult> alt_cfit; // required for sigma_mu estimate and alt toys
        std::shared_ptr<const RooFitResult> asimov_ufit;
        std::shared_ptr<const RooFitResult> asimov_cfit;

        std::vector<double> nullToys; // would have to save these vectors for specific: null_cfit (genPoint), ufit, poiName, pllType, nullVal
        std::vector<double> altToys;

    };

//    class xRooHypoTester {
//        void AddNLLVar(xRooNLLVar &nllVar, const RooArgList &extraPars = {});
//
//        void LoadFile(const char *file); // load fits and toy results from a file
//        void SaveAs(const char *file); // save results to a file
//
//        xRooHypoTestResult hypoTest(const char* poiName, double value, double alt_value, const xRooFit::Asymptotics::PLLType& pllType);
//
//    };

    // use alt_value = nan to skip the asimov calculations
    xRooHypoTestResult hypoTest(const char* parName, double value, double alt_value = std::numeric_limits<double>::quiet_NaN(), const xRooFit::Asymptotics::PLLType& pllType = xRooFit::Asymptotics::Unknown);



    class xRooHypoSpace {
        void ReadFile(const char* fitsFile);
        void SaveAs(const char* output); // saves fit results of the hypospace to given file

        void runLimit(const char* parName, double alt_value);

        void runMinos(const char* parName) {
            // assumes is pll is approximately quadratic: pll =  ( (mu - mu_hat)/sigma_mu )^2
            // so to find where pll = X,
            //   could simply rearrange to give: mu = mu_hat +/- sqrt(X)*sigma_mu
            //   but sigma_mu can have mild dependence on mu, i.e.:
            //    sigma_mu(mu) = (mu - mu_hat)/sqrt( pll(mu) )
            // use an iterative algorithm:
            //   start with some guess for result: mu_guess
            //   update mu_guess = mu_guess - d*( mu_guess - (mu_hat +/- X*sigma_mu(mu_guess)) )
            //  iterate until change in mu_guess is small enough for desired precision
        }
    };


    std::shared_ptr<RooAbsReal> func() const; // will assign globs when called
    std::shared_ptr<RooAbsPdf> pdf() const { return fPdf; }
    RooAbsData* data() const; // returns the data hidden inside the NLLVar if there is some

    // get the Nll value for a specific entry.
    // total nll should be all these values + constraint term + extended term
    double getEntryVal(size_t entry);

    RooNLLVar* nllTerm() const;
    RooConstraintSum* constraintTerm() const;

    // change the dataset - will check globs are the same
    Bool_t setData(const std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>>& _data);

    // using shared ptrs everywhere, even for RooLinkedList which needs custom deleter to clear itself
    // but still work ok for assignment operations
    std::shared_ptr<RooAbsPdf> fPdf;
    std::shared_ptr<RooAbsData> fData;
    std::shared_ptr<RooAbsCollection> fGlobs;

    std::shared_ptr<RooLinkedList> fOpts;
    std::shared_ptr<ROOT::Fit::FitConfig> fFitConfig;

    std::shared_ptr<RooAbsCollection> fFuncVars;
    std::shared_ptr<RooAbsCollection> fConstVars;
    std::string fFuncCreationLog; // messaging from when function was last created -- to save from printing to screen



};