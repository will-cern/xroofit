
#include "xRooFit/xRooFit.h"


#include "RooDataSet.h"
#include "RooSimultaneous.h"

#define protected public
#include "RooFitResult.h"
#undef protected

#include "RooArgSet.h"
#include "RooRandom.h"
#include "RooAbsPdf.h"
#include "TUUID.h"
#include "RooProdPdf.h"
#include "RooGamma.h"
#include "RooPoisson.h"
#include "RooGaussian.h"
#include "RooBifurGauss.h"
#include "RooLognormal.h"
#include "RooRandom.h"
#include "RooBinning.h"

#include "RooStats/AsymptoticCalculator.h"
#include "Math/GenAlgoOptions.h"

#define private public
#include "RooMinimizer.h"
#undef private

#include "coutCapture.h"

xRooNLLVar xRooFit::createNLL(const std::shared_ptr<RooAbsPdf> pdf, const std::shared_ptr<RooAbsData> data, const RooLinkedList& nllOpts) {
    return xRooNLLVar(pdf,data,nllOpts);
}


xRooNLLVar xRooFit::createNLL(RooAbsPdf& pdf, RooAbsData* data, const RooLinkedList& nllOpts) {
    return createNLL(std::shared_ptr<RooAbsPdf>(&pdf,[](RooAbsPdf*){}),std::shared_ptr<RooAbsData>(data,[](RooAbsData*){}),nllOpts);
}

xRooNLLVar xRooFit::createNLL(RooAbsPdf& pdf, RooAbsData* data, const RooCmdArg & 	arg1,
                             const RooCmdArg & 	arg2,const RooCmdArg & 	arg3 ,
                             const RooCmdArg & 	arg4,const RooCmdArg & 	arg5 ,const RooCmdArg & 	arg6,const RooCmdArg & 	arg7,const RooCmdArg & 	arg8  ) {

    RooLinkedList l ;
    l.Add((TObject*)&arg1) ;  l.Add((TObject*)&arg2) ;
    l.Add((TObject*)&arg3) ;  l.Add((TObject*)&arg4) ;
    l.Add((TObject*)&arg5) ;  l.Add((TObject*)&arg6) ;
    l.Add((TObject*)&arg7) ;  l.Add((TObject*)&arg8) ;
    return createNLL(pdf,data,l) ;

}

std::shared_ptr<const RooFitResult> xRooFit::fitTo(RooAbsPdf& pdf, const std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>>& data, const RooLinkedList& nllOpts, const ROOT::Fit::FitConfig& fitConf) {
    return xRooNLLVar(std::shared_ptr<RooAbsPdf>(&pdf,[](RooAbsPdf*){}),data,nllOpts).minimize(std::shared_ptr<ROOT::Fit::FitConfig>(const_cast<ROOT::Fit::FitConfig*>(&fitConf),[](ROOT::Fit::FitConfig*){}));
}

std::shared_ptr<const RooFitResult> xRooFit::fitTo(RooAbsPdf& pdf, const std::pair<RooAbsData*,const RooAbsCollection*>& data, const RooLinkedList& nllOpts, const ROOT::Fit::FitConfig& fitConf) {
    return xRooNLLVar(pdf,data,nllOpts).minimize(std::shared_ptr<ROOT::Fit::FitConfig>(const_cast<ROOT::Fit::FitConfig*>(&fitConf),[](ROOT::Fit::FitConfig*){}));
}



std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> xRooFit::generateFrom(RooAbsPdf& pdf, const std::shared_ptr<const RooFitResult>& fr, bool expected, int seed) {

    std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> out;

    if (!fr) return out;

    auto _allVars = std::unique_ptr<RooAbsCollection>(pdf.getVariables());
    auto _snap = std::unique_ptr<RooAbsCollection>( _allVars->snapshot() );
    *_allVars = fr->constPars();
    *_allVars = fr->floatParsFinal();


    // determine globs from fr constPars
    auto _globs = std::unique_ptr<RooAbsCollection>(fr->constPars().selectByAttrib("global",true));

    bool doBinned = false;
    RooAbsPdf::GenSpec** gs = nullptr;

    if(seed==0) seed = RooRandom::randomGenerator()->Integer(std::numeric_limits<uint32_t>::max());
    RooRandom::randomGenerator()->SetSeed(seed);

    TString uuid = TUUID().AsString();

    std::function<std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooArgSet>>(RooAbsPdf*)> genSubPdf;

    genSubPdf = [&](RooAbsPdf* _pdf) {
        std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooArgSet>> out;
        //std::unique_ptr<RooArgSet> _obs(_pdf->getParameters(*pars)); // using this "trick" to get observables can produce 'error' msg because of RooProdPdf trying to setup partial integrals
        std::unique_ptr<RooArgSet> _obs(_pdf->getVariables());_obs->remove(fr->constPars(),true,true);_obs->remove(fr->floatParsFinal(),true,true); // use this instead

        if(!_globs->empty()) {
            RooArgSet* toy_gobs = new RooArgSet(uuid+"_globs");
            //ensure we use the gobs from the model ...
            RooArgSet t; t.add(*_globs);
            std::unique_ptr<RooArgSet> globs(_pdf->getObservables(t));
            globs->snapshot(*toy_gobs);
            if (!toy_gobs->empty() && !dynamic_cast<RooSimultaneous*>(_pdf)) { // if was simPdf will call genSubPdf on each subpdf so no need to generate here
                if(!expected) {
                    *toy_gobs = *std::unique_ptr<RooDataSet>(_pdf->generate(*globs, 1))->get();
                } else {
                    // loop over pdfs in top-level prod-pdf,
                    auto pp = dynamic_cast<RooProdPdf*>(_pdf);
                    if (pp) {
                        for(auto pdf : pp->pdfList()) {
                            auto gob = std::unique_ptr<RooArgSet>( pdf->getObservables(*globs) );
                            if (gob->empty()) continue;
                            if (gob->size()>1) {
                                Warning("generate","%s contains multiple global obs: %s",pdf->GetName(),gob->contentsString().c_str());
                                continue;
                            }
                            RooRealVar &rrv = dynamic_cast<RooRealVar &>(*gob->first());
                            std::unique_ptr<RooArgSet> cpars(pdf->getParameters(*globs));

                            bool foundServer = false;
                            // note : this will work only for this type of constraints
                            // expressed as RooPoisson, RooGaussian, RooLognormal, RooGamma
                            TClass * cClass = pdf->IsA();
                            if ( cClass != RooGaussian::Class() && cClass != RooPoisson::Class() &&
                                 cClass != RooGamma::Class() && cClass != RooLognormal::Class() &&
                                 cClass != RooBifurGauss::Class()  ) {
                                TString className =  (cClass) ?  cClass->GetName() : "undefined";
                                oocoutW((TObject*)0,Generation) << "AsymptoticCalculator::MakeAsimovData:constraint term "
                                                                << pdf->GetName() << " of type " << className
                                                                << " is a non-supported type - result might be not correct " << std::endl;
                            }

                            // in case of a Poisson constraint make sure the rounding is not set
                            if (cClass == RooPoisson::Class() ) {
                                RooPoisson * pois = dynamic_cast<RooPoisson*>(pdf);
                                assert(pois);
                                pois->setNoRounding(true);
                            }

                            // look at server of the constraint term and check if the global observable is part of the server
                            RooAbsArg * arg = pdf->findServer(rrv);
                            if (!arg) {
                                // special case is for the Gamma where one might define the global observable n and you have a Gamma(b, n+1, ...._
                                // in this case n+1 is the server and we don;t have a direct dependency, but we want to set n to the b value
                                // so in case of the Gamma ignore this test
                                if ( cClass != RooGamma::Class() ) {
                                    oocoutE((TObject*)0,Generation) << "AsymptoticCalculator::MakeAsimovData:constraint term "
                                                                    << pdf->GetName() << " has no direct dependence on global observable- cannot generate it " << std::endl;
                                    continue;
                                }
                            }

                            // loop on the server of the constraint term
                            // need to treat the Gamma as a special case
                            // the mode of the Gamma is (k-1)*theta where theta is the inverse of the rate parameter.
                            // we assume that the global observable is defined as ngobs = k-1 and the theta parameter has the name theta otherwise we use other procedure which might be wrong
                            RooAbsReal * thetaGamma = 0;
                            if ( cClass == RooGamma::Class() ) {
                                RooFIter itc(pdf->serverMIterator() );
                                for (RooAbsArg *a2 = itc.next(); a2 != 0; a2 = itc.next()) {
                                    if (TString(a2->GetName()).Contains("theta") ) {
                                        thetaGamma = dynamic_cast<RooAbsReal*>(a2);
                                        break;
                                    }
                                }
                                if (thetaGamma == 0) {
                                    oocoutI((TObject*)0,Generation) << "AsymptoticCalculator::MakeAsimovData:constraint term "
                                                                    << pdf->GetName() << " is a Gamma distribution and no server named theta is found. Assume that the Gamma scale is  1 " << std::endl;
                                }

                            }
                            RooFIter iter2(pdf->serverMIterator() );
                            for (RooAbsArg *a2 = iter2.next(); a2 != 0; a2 = iter2.next()) {
                                RooAbsReal * rrv2 = dynamic_cast<RooAbsReal *>(a2);
                                if (rrv2 && !rrv2->dependsOn(*gob) && !rrv2->isConstant() ) {


                                    // found server not depending on the gob
                                    if (foundServer) {
                                        oocoutE((TObject*)0,Generation) << "AsymptoticCalculator::MakeAsimovData:constraint term "
                                                                        << pdf->GetName() << " constraint term has more server depending on nuisance- cannot generate it " <<
                                                                        std::endl;
                                        foundServer = false;
                                        break;
                                    }
                                    if (thetaGamma && thetaGamma->getVal() > 0)
                                        rrv.setVal( rrv2->getVal() / thetaGamma->getVal() );
                                    else
                                        rrv.setVal( rrv2->getVal() );
                                    foundServer = true;

                                }
                            }

                            if (!foundServer) {
                                oocoutE((TObject*)0,Generation) << "AsymptoticCalculator::MakeAsimovData - can't find nuisance for constraint term - global observables will not be set to Asimov value " << pdf->GetName() << std::endl;
                                std::cerr << "Parameters: " << std::endl;
                                cpars->Print("V");
                                std::cerr << "Observables: " << std::endl;
                                gob->Print("V");
                            }

                        }
                    } else {
                        Error("generate","Cannot generate global observables, pdf is: %s::%s",_pdf->ClassName(),_pdf->GetName());
                    }
                    *toy_gobs = *globs;
                }

            }
            out.second.reset(toy_gobs);
        } // end of globs generation

        RooRealVar w("weightVar","weightVar",1);
        if (auto s = dynamic_cast<RooSimultaneous*>(_pdf)) {
            // do subpdf's individually
            _obs->add(w);
            out.first.reset(new RooDataSet(uuid,
                                           TString::Format("%s %s", _pdf->GetTitle(), (expected) ? "Expected" : "Toy"),
                                           *_obs, "weightVar"));

            for(auto& c : s->indexCat()) {
#if ROOT_VERSION_CODE >= ROOT_VERSION(6,22,00)
                std::string cLabel = c.first.c_str();
#else
                std::string cLabel = c->GetName();
#endif
                auto p = s->getPdf(cLabel.c_str());
                if (!p) continue;
                auto toy = genSubPdf( p );
                if (toy.second && out.second) *const_cast<RooArgSet*>(out.second.get()) = *toy.second;
                _obs->setCatLabel(s->indexCat().GetName(),cLabel.c_str());
                for(int i = 0; i < toy.first->numEntries();i++) {
                    *_obs = *toy.first->get(i);
                    out.first->add(*_obs, toy.first->weight());
                }
            }
            return out;
        }

        std::map<RooRealVar *, std::shared_ptr<RooAbsBinning>> binnings;

        for (auto &o : *_obs) {
            auto r = dynamic_cast<RooRealVar *>(o);
            if (!r) continue;
            if (_pdf->isBinnedDistribution(*r)) {
                binnings[r] = std::shared_ptr<RooAbsBinning>(r->getBinning().clone(r->getBinning().GetName()));
                auto res = _pdf->binBoundaries(*r, r->getMin(), r->getMax());
                std::vector<double> boundaries;
                boundaries.reserve(res->size());
                for (auto &rr : *res) {if(boundaries.empty() || std::abs(boundaries.back()-rr) > 1e-3 || std::abs(boundaries.back()-rr)>1e-5*boundaries.back()) boundaries.push_back(rr); } // sometimes get virtual duplicates of boundaries
                r->setBinning(RooBinning(boundaries.size() - 1, &boundaries[0]));
                delete res;
            }
        }

        // now can generate
        if (_obs->empty()) {
            // no observables, create a single dataset with 1 entry ... why 1 entry??
            _obs->add(w);
            RooArgSet _tmp; _tmp.add(w);
            out.first.reset( new RooDataSet("","Toy",_tmp,"weightVar"));
            out.first->add(_tmp);
        } else {
            if (_pdf->canBeExtended()) {
                out.first.reset(_pdf->generate(*_obs, RooFit::Extended(), RooFit::ExpectedData(expected)));
            } else {
                if (expected) {
                    // use AsymptoticCalculator because generate expected not working correctly on unextended pdf?
                    // TODO: Can the above code for expected globs be used instead, or what about replace above code with ObsToExpected?
                    out.first.reset(RooStats::AsymptoticCalculator::GenerateAsimovData(*_pdf, *_obs));
                } else {
                    out.first.reset(_pdf->generate(*_obs, RooFit::ExpectedData(expected)));
                }
            }
        }
        out.first->SetName(TUUID().AsString());

        for (auto& b : binnings) {
            auto v = b.first;
            auto binning = b.second;
            v->setBinning(*binning);
            // range of variable in dataset may be less than in the workspace
            // if e.g. generate for a specific channel. So need to expand ranges to match
            auto x = dynamic_cast<RooRealVar *>(out.first->get()->find(v->GetName()));
            auto r = x->getRange();
            if (r.first > binning->lowBound()) x->setMin(binning->lowBound());
            if (r.second < binning->highBound()) x->setMax(binning->highBound());
        }
        return out;
    };

    out = genSubPdf(&pdf);

    *_allVars = *_snap;

    return out;

}

std::shared_ptr<ROOT::Fit::FitConfig> xRooFit::defaultFitConfig() {
    auto fFitConfig = std::make_shared<ROOT::Fit::FitConfig>();
    auto &fitConfig = *fFitConfig;
    fitConfig.SetParabErrors(true); // will use to run hesse after fit
    fitConfig.MinimizerOptions().SetMinimizerType("Minuit2");
    fitConfig.MinimizerOptions().SetErrorDef(0.5); // ensures errors are +/- 1 sigma ..IMPORTANT
    fitConfig.MinimizerOptions().SetMaxFunctionCalls(
            0);  // calls per iteration. if left as 0 will set automatically to 500*nPars below
    fitConfig.MinimizerOptions().SetMaxIterations(
            0); // if left as 0 will set automatically to 500*nPars
    fitConfig.MinimizerOptions().SetStrategy(0);
    //fitConfig.MinimizerOptions().SetTolerance(
    //        1); // default is 0.01 (i think) but roominimizer uses 1 as default - use specify with ROOT::Math::MinimizerOptions::SetDefaultTolerance(..)
    fitConfig.MinimizerOptions().SetPrintLevel(-2);
    fitConfig.MinimizerOptions().SetExtraOptions(ROOT::Math::GenAlgoOptions());
    // have to const cast to set extra options
    auto extraOpts = const_cast<ROOT::Math::IOptions *>(fitConfig.MinimizerOptions().ExtraOptions());
    extraOpts->SetValue("StrategySequence", "012");
    //extraOpts->SetValue("BoundaryCheck",0.01); // warn if within 1% of a boundary
    return fFitConfig;
}

#include "RooAbsTestStatistic.h"
#include "TPRegexp.h"
#include "RooStringVar.h"

std::shared_ptr<const RooFitResult> xRooFit::minimize(RooAbsReal& nll, const std::shared_ptr<ROOT::Fit::FitConfig>& _fitConfig) {


    auto myFitConfig = _fitConfig ? _fitConfig : defaultFitConfig();
    auto& fitConfig = *myFitConfig;

    bool save=true;

    auto _nll = &nll;

    TString resultTitle = nll.getStringAttribute("data");
    // extract any user pars from the nll too
    RooArgList fUserPars;
    if(nll.getStringAttribute("userPars")) {
        TStringToken st(nll.getStringAttribute("userPars"),",");
        while (st.NextToken()) {
            TString parName = st;
            TString parVal = nll.getStringAttribute(parName);
            if (parVal.IsFloat()) fUserPars.addClone(RooRealVar(parName,parName,parVal.Atof()));
            else fUserPars.addClone(RooStringVar(parName,parName,parVal));
        }
    }

    auto _nllVars = std::unique_ptr<RooAbsCollection>(_nll->getVariables());

    std::unique_ptr<RooAbsCollection> constPars( _nllVars->selectByAttrib("Constant",kTRUE) );
    std::unique_ptr<RooAbsCollection> floatPars( _nllVars->selectByAttrib("Constant",kFALSE));

    int _progress = 0;
    double boundaryCheck = 0;
    std::string s;
    int logSize = 0;
    if (fitConfig.MinimizerOptions().ExtraOptions() ) {
        fitConfig.MinimizerOptions().ExtraOptions()->GetNamedValue("StrategySequence", s);
        fitConfig.MinimizerOptions().ExtraOptions()->GetIntValue("TrackProgress",_progress);
        fitConfig.MinimizerOptions().ExtraOptions()->GetRealValue("BoundaryCheck",boundaryCheck);
        fitConfig.MinimizerOptions().ExtraOptions()->GetIntValue("LogSize",logSize);
    }
    TString m_strategy = s;

    int printLevel  =   fitConfig.MinimizerOptions().PrintLevel();
    RooFit::MsgLevel msglevel = RooMsgService::instance().globalKillBelow();
    if(printLevel < 0) RooMsgService::instance().setGlobalKillBelow(RooFit::FATAL);

    //check how many parameters we have ... if 0 parameters then we wont run a fit, we just evaluate nll and return ...
    if(floatPars->getSize()==0 || fitConfig.MinimizerOptions().MaxFunctionCalls()==1) {
        std::shared_ptr<RooFitResult> result;
        RooArgList parsList;parsList.add(*floatPars);
        //construct an empty fit result ...
        result = std::make_shared<RooFitResult>(); // if put name here fitresult gets added to dir, we don't want that
        result->SetName(TUUID().AsString()); result->SetTitle(resultTitle);
        result->setFinalParList( parsList );
        result->setInitParList( parsList );
        constPars->add(fUserPars,true);
        result->setConstParList(  dynamic_cast<RooArgSet&>(*constPars) ); /* RooFitResult takes a snapshot */
        TMatrixDSym d; d.ResizeTo(parsList.size(),parsList.size());
        result->setCovarianceMatrix( d );
        result->setCovQual(-1);
        result->setMinNLL( _nll->getVal() );
        result->setEDM(0);
        result->setStatus(fitConfig.MinimizerOptions().MaxIterations()==0);
        if(printLevel < 0) RooMsgService::instance().setGlobalKillBelow(msglevel);
        return result;
    }




    int strategy =      fitConfig.MinimizerOptions().Strategy();
    //Note: AsymptoticCalculator enforces not less than 1 on tolerance - should we do so too?

    if (_progress) {
        //_nll = new ProgressMonitor(*_nll, _progress);
    }


    std::string logs;
    RooFitResult *out = nullptr;
    {
        auto logger = (logSize > 0) ? std::make_unique<cout_redirect>(logs, logSize) : nullptr;
        RooMinimizer _minimizer(*_nll);
        _minimizer.fitter()->Config() = fitConfig;

        bool autoMaxCalls = (_minimizer.fitter()->Config().MinimizerOptions().MaxFunctionCalls() == 0);
        if (autoMaxCalls) {
            _minimizer.fitter()->Config().MinimizerOptions().SetMaxFunctionCalls(500 * floatPars->size());
        }
        if (_minimizer.fitter()->Config().MinimizerOptions().MaxIterations() == 0) {
            _minimizer.fitter()->Config().MinimizerOptions().SetMaxIterations(500 * floatPars->size());
        }


        bool hesse = _minimizer.fitter()->Config().ParabErrors();
        _minimizer.fitter()->Config().SetParabErrors(
                false); // turn "off" so can run hesse as a separate step, appearing in status
        bool restore = !_minimizer.fitter()->Config().UpdateAfterFit();
        _minimizer.fitter()->Config().SetUpdateAfterFit(true); // note: seems to always take effect

        std::vector<TString> algNames;

        //gCurrentSampler = this;
        //gOldHandlerr = signal(SIGINT,toyInterruptHandlerr);

        TString fitName = TUUID().AsString();
        TString actualFirstMinimizer = _minimizer.fitter()->Config().MinimizerType();

        int status = 0;


        _minimizer.optimizeConst(2);
        // todo use the fitConfig to store which pars are const etc to track the state of this ... doing what RooMinimizer does in fact
        nll.constOptimizeTestStatistic(RooAbsArg::ConfigChange, true); // trigger a re-evaluate of which nodes to cache
        nll.constOptimizeTestStatistic(RooAbsArg::ValueChange, true); // update the cache values -- is this needed??
        for (int tries = 1, maxtries = 4; tries <= maxtries; ++tries) {
            TString minim = _minimizer.fitter()->Config().MinimizerType();
            TString algo = _minimizer.fitter()->Config().MinimizerAlgoType();
            status = _minimizer.minimize(minim, algo);
            if (tries == 1 && actualFirstMinimizer != _minimizer.fitter()->Config().MinimizerType())
                actualFirstMinimizer = _minimizer.fitter()->Config().MinimizerType();
            // RooMinimizer loses the useful status code, so here we will override it
            status = _minimizer.fitter()->Result().Status(); // note: Minuit failure is status code 4, minuit2 that is edm above max
            _minimizer._statusHistory.back().second = _minimizer.fitter()->Result().Status();
            minim = _minimizer.fitter()->Config().MinimizerType(); // may have changed value
            if (save)
                algNames.push_back(_minimizer.fitter()->Config().MinimizerType()
                                   + _minimizer.fitter()->Config().MinimizerAlgoType() +
                                   std::to_string(_minimizer.fitter()->Config().MinimizerOptions().Strategy()));
            //int status = _minimizer->migrad();
            if (status % 1000 == 0) break; //fit was good

            if (_minimizer.fitter()->Result().Status() == 4 && minim != "Minuit") {
                if (printLevel >= -1)
                    Warning("fitTo", "%s Hit max function calls of %d", fitName.Data(),
                            _minimizer.fitter()->Config().MinimizerOptions().MaxFunctionCalls());
                if (autoMaxCalls) {
                    if (printLevel >= -1) Warning("fitTo", "will try doubling this");
                    _minimizer.fitter()->Config().MinimizerOptions().SetMaxFunctionCalls(
                            _minimizer.fitter()->Config().MinimizerOptions().MaxFunctionCalls() * 2);
                    _minimizer.fitter()->Config().MinimizerOptions().SetMaxIterations(
                            _minimizer.fitter()->Config().MinimizerOptions().MaxIterations() * 2);
                    if (tries == maxtries) tries = maxtries - 1;
                    continue;
                }
            }

            if (tries >= maxtries) break; //giving up

            //NOTE: minuit2 seems to distort the tolerance in a weird way, so that tol becomes 100 times smaller than specified
            //Also note that if fits are failing because of edm over max, it can be a good idea to activate the Offset option when building nll
            if (printLevel >= -1)
                Warning("fitTo", "%s Status=%d (edm=%f, tol=%f, strat=%d), Rescanning #%d...", fitName.Data(), status,
                        _minimizer.fitter()->Result().Edm(),
                        _minimizer.fitter()->Config().MinimizerOptions().Tolerance(),
                        _minimizer.fitter()->Config().MinimizerOptions().Strategy(), tries);
            if (tries < maxtries) {
                _minimizer.minimize(minim, "Scan");
                if (save) algNames.push_back(_minimizer.fitter()->Config().MinimizerType() + "Scan");
            }
            if (tries == 2) { //up the strategy (if we can)
                int idx = m_strategy.Index('0' + strategy);
                if (idx != -1 && idx != m_strategy.Length() - 1) {
                    strategy = int(m_strategy(idx + 1) - '0');
                    _minimizer.setStrategy(strategy);
                    tries -= 2; // go back to 0 tries so that will do minimize then rescan at that strat and retry
                } else {
                    tries++; //move on to next
                }
            }
            if (tries == 3) { _minimizer.fitter()->Config().SetMinimizer("Minuit", "migradImproved"); }
            else {
                // put algo back after setting it to 'scan'
                _minimizer.fitter()->Config().SetMinimizer(minim, algo);
            }
        }

        /* Minuit2 status codes:
         * status = 0    : OK
              status = 1    : Covariance was mad  epos defined
               status = 2    : Hesse is invalid
               status = 3    : Edm is above max
               status = 4    : Reached call limit
               status = 5    : Any other failure

          For Minuit its basically 0 is OK, 4 is failure, I think?
         */


        if (printLevel >= -1 && status != 0) {
            Warning("fitTo", "%s final status is %d", fitName.Data(), status);
        }

        if (hesse) {
            //_nll->getVal(); // for reasons I dont understand, if nll evaluated before hesse call the edm is smaller? - and also becomes WRONG :-S
            _minimizer.hesse(); //note: I have seen that you can get 'full covariance quality' without running hesse ... is that expected?
            _minimizer._statusHistory.back().second = _minimizer.fitter()->Result().Status();
            auto _status = _minimizer.fitter()->Result().Status();
            if (_status != 0 && status == 0 && printLevel >= -1) {
                Warning("fitTo", "%s hesse status is %d", fitName.Data(), _status);
            }
        }

        //signal(SIGINT,gOldHandlerr);
        out = _minimizer.save(fitName, resultTitle);


        if (save) {
            //modify the statusHistory to use the algnames instead ..
            int i = 0;
            for (auto &s : algNames) { out->_statusHistory[i++].first = s; }
        }

        out->_constPars->addClone(fUserPars, true);


        if (boundaryCheck) {
            // check if any of the parameters are at their limits (potentially a problem with fit)
            // or their errors go over their limits (just a warning)
            RooFIter itr = floatPars->fwdIterator();
            RooAbsArg *a = 0;
            int limit_status = 0;
            std::string listpars;
            while ((a = itr.next())) {
                RooRealVar *v = dynamic_cast<RooRealVar *>(a);
                if (!v) continue;
                double vRange = v->getMax() - v->getMin();
                if (v->getMin() > v->getVal() - vRange * boundaryCheck ||
                    v->getMax() < v->getVal() + vRange * boundaryCheck) {
                    // within 0.01% of edge

                    // check if nll actually lower 'at' the boundary, if it is, refine the best fit to the limit value
                    auto tmp = v->getVal();
                    v->setVal(v->getMin());
                    double boundary_nll = _nll->getVal();
                    if (boundary_nll <= out->minNll()) {
                        ((RooRealVar *) out->_finalPars->find(v->GetName()))->setVal(v->getMin());
                        out->setMinNLL(boundary_nll);
                        //Info("fit","Corrected %s onto minimum @ %g",v->GetName(),v->getMin());
                    } else {
                        // not better, so restore value
                        v->setVal(tmp);
                    }

                    // if has a 'physical' range specified, don't warn if near the limit
                    if (v->hasRange("physical"))
                        limit_status = 900;
                    listpars += v->GetName();
                    listpars += ",";
                } else if (hesse && (v->getMin() > v->getVal() - v->getError() || v->getMax() < v->getVal()
                                                                                                + v->getError())) {
                    if (printLevel >= 0) {
                        Info("minimize", "PARLIM: %s (%f +/- %f) range (%f - %f)", v->GetName(), v->getVal(),
                             v->getError(),
                             v->getMin(), v->getMax());
                    }
                    limit_status = 9000;
                }
            }
            if (limit_status == 900) {
                if (printLevel >= 0)
                    Warning("miminize", "PARLIM: Parameters within %g%% limit in fit result: %s", boundaryCheck * 100,
                            listpars.c_str());
            } else if (limit_status > 0) {
                if (printLevel >= 0)
                    Warning("miminize", "PARLIM: Parameters near limit in fit result");
            }

            // store the limit check result
            out->_statusHistory.push_back(std::make_pair("BOUNDCHK", limit_status));
            out->_status += limit_status;
        }


        if (printLevel < 0) RooMsgService::instance().setGlobalKillBelow(msglevel);

        //before returning we will override _minLL with the actual NLL value ... offsetting could have messed up the value
        out->setMinNLL(_nll->getVal());



        // minimizer may have slightly altered the fitConfig (e.g. unavailable minimizer etc) so update for that ...
        if (fitConfig.MinimizerOptions().MinimizerType() != actualFirstMinimizer) {
            fitConfig.MinimizerOptions().SetMinimizerType(actualFirstMinimizer);
        }

        if (restore) {
            *floatPars = out->floatParsInit();
        }

        if (_progress) {
            //delete _nll;
        }
    }
    if (out && !logs.empty()) {
        // save logs to StringVar in constPars list
        out->_constPars->addClone(RooStringVar("log","log",logs.c_str()));
    }

    return std::shared_ptr<const RooFitResult>(out);

}