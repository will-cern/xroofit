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

    void Print(Option_t* opt="");

    xRooNLLVar(RooAbsPdf& pdf,const std::pair<RooAbsData*,const RooAbsCollection*>& data, const RooLinkedList& nllOpts = RooLinkedList());
    xRooNLLVar(const std::shared_ptr<RooAbsPdf>& pdf, const std::shared_ptr<RooAbsData>& data, const RooLinkedList& opts = RooLinkedList());
    xRooNLLVar(const std::shared_ptr<RooAbsPdf>& pdf, const std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>>& data, const RooLinkedList& opts = RooLinkedList());

    ~xRooNLLVar();

    // whenever implicitly converted to a RooAbsReal we will make sure our globs are set
    RooAbsReal* get() const { return func().get(); }
    RooAbsReal* operator->() const { return get(); }

    void reinitialize();

    void AddOption(const RooCmdArg& opt);

    std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> getData() const; // returns pointer to data and snapshot of globs
    std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> generate(bool expected=false,int seed=0);
    //std::shared_ptr<const RooFitResult> snapshot();

    std::shared_ptr<const RooFitResult> minimize(const std::shared_ptr<ROOT::Fit::FitConfig>& = nullptr);

    void SetFitConfig(const std::shared_ptr<ROOT::Fit::FitConfig>& in) { fFitConfig = in; }
    std::shared_ptr<ROOT::Fit::FitConfig> fitConfig(); // returns fit config, or creates a default one if not existing

    std::pair<double,double> pll(const char* parName, double value, const xRooFit::Asymptotics::PLLType& pllType = xRooFit::Asymptotics::TwoSided);
    std::pair<double,double> sigma_mu(const char* parName, double value, double prime_value);

    class xRooHypoPoint {
    public:
        std::pair<double,double> pll(); // observed test statistic value
        std::pair<double,double> sigma_mu(); // estimate of sigma_mu parameter
        std::shared_ptr<const RooFitResult> ufit();
        std::shared_ptr<const RooFitResult> null_cfit();
        std::shared_ptr<const RooFitResult> alt_cfit();


        std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> data;

        // leave nSigma=NaN for observed p-value
        double pNull_asymp(double nSigma=std::numeric_limits<double>::quiet_NaN());
        double pAlt_asymp(double nSigma=std::numeric_limits<double>::quiet_NaN());
        double pCLs_asymp(double nSigma=std::numeric_limits<double>::quiet_NaN()) { return (pNull_asymp(nSigma)==0) ? 0 : (pNull_asymp(nSigma)/pAlt_asymp(nSigma)); }

        double pNull_toys(double nSigma=std::numeric_limits<double>::quiet_NaN());
        double pAlt_toys(double nSigma=std::numeric_limits<double>::quiet_NaN());
        double pCLs_toys(double nSigma=std::numeric_limits<double>::quiet_NaN()) { return (pNull_toys(nSigma)==0) ? 0 : (pNull_toys(nSigma)/pAlt_toys(nSigma)); }

        xRooHypoPoint generateNull();
        xRooHypoPoint generateAlt();

        RooRealVar& mu_hat(); // throws exception if ufit not available

        std::string fPOIName;
        xRooFit::Asymptotics::PLLType fPllType;
        double fNullVal=1; double fAltVal=0;

        std::shared_ptr<const RooAbsCollection> coords; // pars of the nll that will be held const alongside POI

        std::shared_ptr<const RooFitResult> fUfit,fNull_cfit,fAlt_cfit;
        std::shared_ptr<const RooFitResult> fGenFit; // if the data was generated, this is the fit is was generated from

        std::shared_ptr<xRooHypoPoint> fAsimov; // same as this point but pllType is twosided and data is expected post alt-fit

        std::vector<double> nullToys; // would have to save these vectors for specific: null_cfit (genPoint), ufit, poiName, pllType, nullVal
        std::vector<double> altToys;

        xRooNLLVar* nllVar = nullptr;

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
    xRooHypoPoint hypoPoint(const char* parName, double value, double alt_value = std::numeric_limits<double>::quiet_NaN(), const xRooFit::Asymptotics::PLLType& pllType = xRooFit::Asymptotics::Unknown);



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

    std::shared_ptr<RooArgSet> pars(bool stripGlobalObs=true);

    void Draw(Option_t* opt = "");

    std::shared_ptr<RooAbsReal> func() const; // will assign globs when called
    std::shared_ptr<RooAbsPdf> pdf() const { return fPdf; }
    RooAbsData* data() const; // returns the data hidden inside the NLLVar if there is some



    // get the Nll value for a specific entry.
    // total nll should be all these values + constraint term + extended term + simTerm [+binnedDataTerm if activated binnedL option]
    double getEntryVal(size_t entry);

    RooNLLVar* nllTerm() const;
    RooConstraintSum* constraintTerm() const;
    double extendedTerm() const;
    double simTerm() const;
    double binnedDataTerm() const;

    // change the dataset - will check globs are the same
    Bool_t setData(const std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>>& _data);
    Bool_t setData(const std::shared_ptr<RooAbsData>& data,const std::shared_ptr<const RooAbsCollection>& globs) {
        return setData(std::make_pair(data,globs));
    }

    // using shared ptrs everywhere, even for RooLinkedList which needs custom deleter to clear itself
    // but still work ok for assignment operations
    std::shared_ptr<RooAbsPdf> fPdf;
    std::shared_ptr<RooAbsData> fData;
    std::shared_ptr<const RooAbsCollection> fGlobs;

    std::shared_ptr<RooLinkedList> fOpts;
    std::shared_ptr<ROOT::Fit::FitConfig> fFitConfig;

    std::shared_ptr<RooAbsCollection> fFuncVars;
    std::shared_ptr<RooAbsCollection> fConstVars;
    std::shared_ptr<RooAbsCollection> fFuncGlobs;
    std::string fFuncCreationLog; // messaging from when function was last created -- to save from printing to screen



};