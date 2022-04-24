
#define protected public
#include "RooFitResult.h"
#include "RooNLLVar.h"
#undef protected

#include "xRooFit/xRooFit.h"

#include "RooCmdArg.h"
#include "RooAbsPdf.h"
#include "RooAbsData.h"

#include "RooConstraintSum.h"
#include "RooSimultaneous.h"
#include "RooAbsCategoryLValue.h"
#include "TPRegexp.h"

#include "RooRealVar.h"
#include "Math/ProbFunc.h"

#include "TPad.h"
#include "TSystem.h"

#include "coutCapture.h"

#include <chrono>

xRooNLLVar::~xRooNLLVar() {

}

xRooNLLVar::xRooNLLVar(RooAbsPdf& pdf,const std::pair<RooAbsData*,const RooAbsCollection*>& data, const RooLinkedList& nllOpts)
    : xRooNLLVar(std::shared_ptr<RooAbsPdf>(&pdf,[](RooAbsPdf*){}),std::make_pair(std::shared_ptr<RooAbsData>(data.first,[](RooAbsData*){}),std::shared_ptr<const RooAbsCollection>(data.second,[](const RooAbsCollection*){})),nllOpts) {

}


xRooNLLVar::xRooNLLVar(const std::shared_ptr<RooAbsPdf>& pdf,
                       const std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>>& data,
                       const RooLinkedList& opts) : fPdf(pdf), fData(data.first), fGlobs(data.second) {

    RooMsgService::instance().getStream(RooFit::INFO).removeTopic(RooFit::NumIntegration);

    fOpts = std::shared_ptr<RooLinkedList>(new RooLinkedList,[](RooLinkedList* l) { if(l) l->Delete(); delete l; } );

    for(int i=0; i< opts.GetSize(); i++) {
        if (strlen(opts.At(i)->GetName())==0) continue; // skipping "none" cmds
        if (strcmp(opts.At(i)->GetName(),"GlobalObservables")==0) {
            // will skip here to add with the obs from the function below
            // must match global observables
            auto gl = dynamic_cast<RooCmdArg*>(opts.At(i))->getSet(0);
            if (!fGlobs || !fGlobs->equals(*gl)) {
                throw std::runtime_error("GlobalObservables mismatch");
            }
        } else {
            fOpts->Add(opts.At(i)->Clone(
                    nullptr)); //nullptr needed because accessing Clone via TObject base class puts "" instead, so doesnt copy names
        }
    }
    if (fGlobs) {
        // add global observables opt with function obs
        auto _vars = std::unique_ptr<RooArgSet>( fPdf->getVariables() );
        auto _funcGlobs = std::unique_ptr<RooArgSet>(dynamic_cast<RooArgSet*>(_vars->selectCommon(*fGlobs)));
        fOpts->Add(RooFit::GlobalObservables(*_funcGlobs).Clone());
    }

    // if fit range specified, and pdf is a RooSimultaneous, may need to 'reduce' the model if some of the pdfs are in range and others are not
    if (auto range = dynamic_cast<RooCmdArg*>(fOpts->find("RangeWithName"))) {
        TString rangeName = range->getString(0);

        // reduce the data here for convenience, not really necessary because will happen inside RooNLLVar but still
        // fData.reset( fData->reduce(RooFit::SelectVars(*fData->get()),RooFit::CutRange(rangeName)) );

        if (auto s = dynamic_cast<RooSimultaneous*>(fPdf.get()); s) {
            auto &_cat = const_cast<RooAbsCategoryLValue &>(s->indexCat());
            std::vector<TString> chanPatterns;
            TStringToken pattern(rangeName, ",");
            bool hasRange(false);
            std::string noneCatRanges;
            while (pattern.NextToken()) {
                chanPatterns.emplace_back(pattern);
                if (_cat.hasRange(chanPatterns.back())) hasRange = true;
                else {
                    if (!noneCatRanges.empty()) noneCatRanges += ",";
                    noneCatRanges += chanPatterns.back();
                }
            }
            if (hasRange) {
                // must remove the ranges that referred to selections on channel category
                // otherwise RooFit will incorrectly evaluate the NLL (it creates a partition for each range given in the list, which all end up being equal)
                // the NLL would become scaled by the number of ranges given
                if (noneCatRanges.empty()) {
                    fOpts->Remove(range);
                    SafeDelete(range);
                } else {
                    range->setString(0,noneCatRanges.c_str());
                }
                // must reduce because category var has one of the ranges
                auto newPdf = std::make_shared<RooSimultaneous>(TString::Format("%s_reduced", s->GetName()),
                                                                "Reduced model", _cat);
                for (auto &c : _cat) {
                    auto _pdf = s->getPdf(c.first.c_str());
                    if (!_pdf) continue;
                    _cat.setIndex(c.second);
                    bool matchAny = false;
                    for (auto &p : chanPatterns) {
                        if (_cat.hasRange(p) && _cat.inRange(p)) {
                            matchAny = true;
                            break;
                        }
                    }
                    if (matchAny) {
                        newPdf->addPdf(*_pdf, c.first.c_str());
                    }
                }
                fPdf = newPdf;
            }
        }
    }

//    if (fGlobs) {
//        // must check GlobalObservables is in the list
//    }
//
//    if (auto globs = dynamic_cast<RooCmdArg*>(fOpts->find("GlobalObservables"))) {
//        // first remove any obs the pdf doesnt depend on
//        auto _vars = std::unique_ptr<RooAbsCollection>( fPdf->getVariables() );
//        auto _funcGlobs = std::unique_ptr<RooAbsCollection>(_vars->selectCommon(*globs->getSet(0)));
//        fGlobs.reset( std::unique_ptr<RooAbsCollection>(globs->getSet(0)->selectCommon(*_funcGlobs))->snapshot() );
//        globs->setSet(0,dynamic_cast<const RooArgSet&>(*_funcGlobs)); // globs in linked list has its own argset but args need to live as long as the func
//        /*RooArgSet toRemove;
//        for(auto a : *globs->getSet(0)) {
//            if (!_vars->find(*a)) toRemove.add(*a);
//        }
//        const_cast<RooArgSet*>(globs->getSet(0))->remove(toRemove);
//        fGlobs.reset( globs->getSet(0)->snapshot() );
//        fGlobs->setAttribAll("Constant",true);
//        const_cast<RooArgSet*>(globs->getSet(0))->replace(*fGlobs);*/
//    }


};


xRooNLLVar::xRooNLLVar(const std::shared_ptr<RooAbsPdf>& pdf, const std::shared_ptr<RooAbsData>& data, const RooLinkedList& opts) :
    xRooNLLVar(pdf,std::make_pair(data,std::shared_ptr<const RooAbsCollection>((opts.find("GlobalObservables")) ? dynamic_cast<RooCmdArg*>(opts.find("GlobalObservables"))->getSet(0)->snapshot() : nullptr)),opts)
     {



}



void xRooNLLVar::Print(Option_t*) {
    std::cout << "PDF: "; if(fPdf) fPdf->Print(); else std::cout << "<null>" << std::endl;
    std::cout << "Data: "; if(fData) fData->Print(); else std::cout << "<null>" << std::endl;
    std::cout << "NLL Options: " << std::endl;
    for(int i=0;i < fOpts->GetSize();i++) {
        auto c = dynamic_cast<RooCmdArg*>(fOpts->At(i));
        if (!c) continue;
        std::cout << " " << c->GetName() << " : ";
        if (c->getString(0)) std::cout << c->getString(0);
        else if(c->getSet(0) && !c->getSet(0)->empty()) std::cout << (c->getSet(0)->contentsString());
        else std::cout << c->getInt(0);
        std::cout << std::endl;
    }
    if(fFitConfig) {
        std::cout << "Fit Config: " << std::endl;
        std::cout << "  UseParabErrors: " << (fFitConfig->ParabErrors() ? "True" : "False") << "  [toggles HESSE algorithm]" << std::endl;
        std::cout << "  MinimizerOptions: " << std::endl;
        fFitConfig->MinimizerOptions().Print();
    }
}

void xRooNLLVar::reinitialize() {
    {
        cout_redirect c(fFuncCreationLog);
        // need to find all RooRealSumPdf nodes and mark them binned or unbinned as required
        RooArgSet s; fPdf->treeNodeServerList(&s,nullptr,true,false);
        bool isBinned=false;
        if (auto a = dynamic_cast<RooCmdArg*>(fOpts->find("Binned"));a && a->getInt(0)) isBinned=true;
        for(auto a : s) {
            if (a->InheritsFrom("RooRealSumPdf")) a->setAttribute("BinnedLikelihood",isBinned);
        }
        this->reset( fPdf->createNLL(*fData,*fOpts) );
    }

    fFuncVars.reset( std::shared_ptr<RooAbsReal>::get()->getVariables() );
    if(fGlobs) {fFuncGlobs.reset( fFuncVars->selectCommon(*fGlobs) );fFuncGlobs->setAttribAll("Constant",true);}
    fConstVars.reset( fFuncVars->selectByAttrib("Constant",true) ); // will check if any of these have floated
}

std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> xRooNLLVar::generate(bool expected,int seed) {
    auto fr = std::make_shared<RooFitResult>();
    fr->setFinalParList(RooArgList());
    RooArgList l; l.add((fFuncVars) ? *fFuncVars : *std::unique_ptr<RooAbsCollection>(fPdf->getParameters(*fData)));
    fr->setConstParList(l);
    fr->_constPars->setAttribAll("global",false);
    if(fGlobs) std::unique_ptr<RooAbsCollection>(fr->_constPars->selectCommon(*fGlobs))->setAttribAll("global",true);
    return xRooFit::generateFrom(*fPdf, fr,expected,seed);
}

std::shared_ptr<const RooFitResult> xRooNLLVar::minimize(const std::shared_ptr<ROOT::Fit::FitConfig>& _config) {
    auto out = xRooFit::minimize(*get(),(_config) ? _config : fitConfig());
    // add any pars that are const here that aren't in constPars list because they may have been
    // const-optimized and their values cached with the dataset, so if subsequently floated the
    // nll wont evaluate correctly
    //fConstVars.reset( fFuncVars->selectByAttrib("Constant",true) );
    out->_constPars->setAttribAll("global",false);
    if(fGlobs) std::unique_ptr<RooAbsCollection>(out->_constPars->selectCommon(*fGlobs))->setAttribAll("global",true);
    return out;
}

class AutoRestorer {
public:
    AutoRestorer(const RooAbsCollection& s, xRooNLLVar* nll=nullptr) : fSnap(s.snapshot()), fNll(nll) {
        fPars.add(s);
        if(fNll) fOldData = fNll->getData();
    }
    ~AutoRestorer() { ((RooAbsCollection&)fPars) = *fSnap; if(fNll) fNll->setData(fOldData); }
    RooArgSet fPars;
    std::unique_ptr<RooAbsCollection> fSnap;
    xRooNLLVar* fNll = nullptr;
    std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> fOldData;
};

std::shared_ptr<ROOT::Fit::FitConfig> xRooNLLVar::fitConfig() {
    if (!fFitConfig) fFitConfig = xRooFit::defaultFitConfig();
    return fFitConfig;
}

std::pair<double,double> xRooNLLVar::pll(const char* parName, double value, const xRooFit::Asymptotics::PLLType& pllType) {

    // start by floating everything and consting all the const vars
    if (!fFuncVars) {
        reinitialize();
    } else {
        fFuncVars->setAttribAll("Constant",false);
        fConstVars->setAttribAll("Constant",true);
    }

    auto poi = dynamic_cast<RooRealVar*>(fFuncVars->find(parName));
    if (!poi) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);

    AutoRestorer snap(*fFuncVars);

    poi->setConstant(false);
    auto ufit = minimize();
    if (ufit->status() != 0) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);
    auto cFactor = xRooFit::Asymptotics::CompatFactor(pllType, value, static_cast<RooAbsReal*>(ufit->floatParsFinal().find(parName))->getVal());
    if (cFactor == 0) return std::make_pair(0,0);


    poi->setConstant(true); poi->setVal(value);
    auto cfit = minimize();
    if (cfit->status() != 0) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);;

    //std::cout << cfit->minNll() << ":" << cfit->edm() << " " << ufit->minNll() << ":" << ufit->edm() << std::endl;

    return std::make_pair(2.*cFactor*(cfit->minNll()-ufit->minNll()),2.*cFactor*sqrt(pow(cfit->edm(),2)+pow(ufit->edm(),2)));
    //return 2.*cFactor*(cfit->minNll()+cfit->edm() - ufit->minNll()+ufit->edm());
}

std::pair<double,double> xRooNLLVar::sigma_mu(const char* parName, double value, double prime_value) {
    // this estimate involves:
    // 1. fit @ prime_value
    // 2. get expected data
    // 3. evaluate pll of expected data at value

    // start by floating everything and consting all the const vars
    if (!fFuncVars) {
        reinitialize();
    } else {
        fFuncVars->setAttribAll("Constant",false);
        fConstVars->setAttribAll("Constant",true);
    }
    auto poi = dynamic_cast<RooRealVar*>(fFuncVars->find(parName));
    if (!poi) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);

    AutoRestorer _snap(*fFuncVars);

    poi->setConstant(true); poi->setVal(prime_value);
    auto cfit_prime = minimize();
    if (cfit_prime->status () != 0) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);

    auto oldData = std::make_pair(fData,(fGlobs) ? std::shared_ptr<RooAbsCollection>(fGlobs->snapshot()) : nullptr);

    setData(generate(true));
    auto out = pll(parName,value);
    setData(oldData);
    return std::make_pair(std::abs(value - prime_value)/sqrt(out.first), out.second*0.5*std::abs(value - prime_value)/(out.first*sqrt(out.first)));

}

double xRooNLLVar::getEntryVal(size_t entry) {
    auto _data = data();
    if (!_data) return 0;
    if (_data->numEntries()<=entry) return 0;
    auto _pdf = pdf();
    *std::unique_ptr<RooAbsCollection>(_pdf->getObservables(_data)) = *_data->get(entry);
    //if (auto s = dynamic_cast<RooSimultaneous*>(_pdf.get());s) return -_data->weight()*s->getPdf(s->indexCat().getLabel())->getLogVal(_data->get());
    return -_data->weight()*_pdf->getLogVal(_data->get());
}

std::shared_ptr<RooArgSet> xRooNLLVar::pars(bool stripGlobalObs) {
    auto out = std::shared_ptr<RooArgSet>(get()->getVariables());
    if(stripGlobalObs && fGlobs) {
        out->remove(*fGlobs,true,true);
    }
    return out;
}

#include "TMultiGraph.h"
#include "TCanvas.h"

void xRooNLLVar::Draw(Option_t* opt) {
    TString sOpt(opt);

    auto _pars = pars();

    if (sOpt == "sensitivity") {

        // will make a plot of DeltaNLL

    }

    if (sOpt == "floating") {
        // start scanning floating pars
        auto floats = std::unique_ptr<RooAbsCollection>(_pars->selectByAttrib("Constant",false));
        TVirtualPad* pad = gPad;
        if (!pad) {
            TCanvas::MakeDefCanvas();
            pad = gPad;
        }
        TMultiGraph* gr = new TMultiGraph; gr->SetName("multigraph");
        gr->SetTitle(TString::Format("%s;Normalized Parameter Value;#Delta NLL",get()->GetTitle()));
        /*((TPad*)pad)->DivideSquare(floats->size());
        int i=0;
        for(auto a : *floats) {
            i++;
            pad->cd(i);
            Draw(a->GetName());
        }*/
        return;
    }



    RooArgList vars;
    TStringToken pattern(sOpt, ":");
    while (pattern.NextToken()) {
        TString s(pattern);
        if(auto a = _pars->find(s); a) vars.add(*a);
    }

    if (vars.size()==1) {
        TGraph *out = new TGraph;out->SetBit(kCanDelete);
        TGraph *bad = new TGraph; bad->SetBit(kCanDelete); bad->SetMarkerColor(kRed); bad->SetMarkerStyle(5);
        TMultiGraph* gr = (gPad) ? dynamic_cast<TMultiGraph*>(gPad->GetPrimitive("multigraph")) : nullptr;
        bool normRange = false;
        if (!gr) {
            gr = new TMultiGraph;
            gr->Add(out, "LP");
            gr->SetBit(kCanDelete);
        } else {
            normRange = true;
        }
        out->SetName(get()->GetName());
        gr->SetTitle(TString::Format("%s;%s;#Delta NLL",get()->GetTitle(),vars.at(0)->GetTitle()));
        // scan outwards from current value towards limits
        auto v = dynamic_cast<RooRealVar*>(vars.at(0));
        double low = v->getVal(); double high = low;
        double step = (v->getMax() - v->getMin())/100;
        double init = v->getVal(); double initVal = func()->getVal();
        double xscale = (normRange) ? (2.*(v->getMax() - v->getMin())) : 1.;
        auto currTime = std::chrono::steady_clock::now();
        while( out->GetN() < 100 && (low > v->getMin() || high < v->getMax()) ) {
            if(out->GetN()==0) {
                out->SetPoint(out->GetN(),low,0);
                low -= step; high += step;
                gr->Draw("A");
                gPad->SetGrid();
                continue;
            }
            if (low > v->getMin()) {
                v->setVal(low);
                auto _v = func()->getVal();
                if (std::isnan(_v) || std::isinf(_v)) {
                    if (bad->GetN()==0) gr->Add(bad,"P");
                    bad->SetPoint(bad->GetN(),low,out->GetPointY(0));
                } else {
                    out->SetPoint(out->GetN(), low, _v - initVal);
                }
                low -= step;
            }
            if (high < v->getMax()) {
                v->setVal(high);
                auto _v = func()->getVal();
                if (std::isnan(_v) || std::isinf(_v)) {
                    if (bad->GetN()==0) gr->Add(bad,"P");
                    bad->SetPoint(bad->GetN(),high,out->GetPointY(0));
                } else {
                    out->SetPoint(out->GetN(), high, _v - initVal);
                }
                high += step;
            }
            out->Sort();
            // should only do processEvents once every second in case using x11 (which is slow)
            gPad->Modified();
            if(std::chrono::steady_clock::now() - currTime > std::chrono::seconds(1)) {
                currTime = std::chrono::steady_clock::now();
                gPad->Update();gSystem->ProcessEvents();
            }
        }
        gPad->Update();gSystem->ProcessEvents();
        v->setVal(init);
    } else {
        Error("Draw","Name a parameter to scan over: Draw(<name>)");
    }




}

std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> xRooNLLVar::getData() const {
    return std::make_pair(fData,fGlobs);
}

Bool_t xRooNLLVar::setData(const xRooNode& data) {
    if (!data.get<RooAbsData>()) {
        return false;
    }
    return setData(std::dynamic_pointer_cast<RooAbsData>(data.fComp),std::shared_ptr<const RooAbsCollection>(data.globs().argList().snapshot()));
}

Bool_t xRooNLLVar::setData(const std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>>& _data) {

    if (fData == _data.first && fGlobs == _data.second) return true;

    if (fGlobs && !(fGlobs->empty() && !_data.second)) { // second condition allows for no globs being a nullptr
        if (!_data.second) throw std::runtime_error("Missing globs");
        // ignore 'extra' globs
        RooArgSet s;s.add(*fGlobs);
        std::unique_ptr<RooAbsCollection> _actualGlobs(fPdf->getObservables(s));
        RooArgSet s2; s2.add(*_data.second);
        std::unique_ptr<RooAbsCollection> _actualGlobs2(fPdf->getObservables(s2));
        if (!_actualGlobs->equals(*_actualGlobs2)) {
            RooArgSet rC; rC.add(*_actualGlobs2);
            rC.remove(*std::unique_ptr<RooAbsCollection>(rC.selectCommon(*_actualGlobs)));
            TString r = (!rC.empty()) ? rC.contentsString() : "";
            RooArgSet lC; lC.add(*_actualGlobs);
            lC.remove(*std::unique_ptr<RooAbsCollection>(lC.selectCommon(*_actualGlobs2)));
            TString l = (!lC.empty()) ? lC.contentsString() : "";
            throw std::runtime_error(TString::Format("globs mismatch: adding %s removing %s",r.Data(),l.Data()));
        }
        fGlobs = _data.second;
    }

    if (!std::shared_ptr<RooAbsReal>::get()) {
        fData = _data.first;
        return true; // not loaded yet so nothing to do
    }


    try {
        if (nllTerm()->operMode()==RooAbsTestStatistic::MPMaster) {
            throw std::runtime_error("not supported");
        }
        auto out = nllTerm()->setData(*_data.first, false /* clone data? */);
        fData = _data.first;
        return out;
    } catch(std::runtime_error&) {
        // happens when using MP need to rebuild the nll instead
        reset();
        AutoRestorer snap(*fFuncVars);
        // ensure the const state is back where it was at nll construction time;
        fFuncVars->setAttribAll("Constant",false); fConstVars->setAttribAll("Constant",true);
        fData = _data.first;
        reinitialize();
        return true;
    }
    throw std::runtime_error("Unable to setData");
}

std::shared_ptr<RooAbsReal> xRooNLLVar::func() const {
    if (!(*this)) {
        const_cast<xRooNLLVar*>(this)->reinitialize();
    } else if (auto f = std::unique_ptr<RooAbsCollection>(fConstVars->selectByAttrib("Constant",false)); !f->empty()) {
        // have to reinitialize if const par values have changed - const optimization forces this
        // TODO: currently changes to globs also triggers this since the vars includes globs (vars are the non-obs pars)
        //std::cout << "Reinitializing because of change of const parameters:" << f->contentsString() << std::endl;
        const_cast<xRooNLLVar*>(this)->reinitialize();
    }
    if (fGlobs && fFuncGlobs) {*fFuncGlobs = *fGlobs; fFuncGlobs->setAttribAll("Constant",true);}
    return *this;
}

void xRooNLLVar::AddOption(const RooCmdArg& opt) {
    fOpts->Add(opt.Clone(nullptr));
    reset(); // will trigger reinitialize
}

RooAbsData* xRooNLLVar::data() const {
    auto _nll = nllTerm();
    if (!_nll) return fData.get();
    RooAbsData* out = &_nll->data();
    if (!out) return fData.get();
    return out;
}

RooNLLVar* xRooNLLVar::nllTerm() const {
    auto _func = func();
    if (auto a = dynamic_cast<RooNLLVar*>(_func.get()); a) return a;
    for(auto s : _func->servers()) {
        if (auto a = dynamic_cast<RooNLLVar*>(s); a) return a;
    }
    return nullptr;
}

double xRooNLLVar::extendedTerm() const {
    // returns Nexp - Nobs*log(Nexp)
    return fPdf->extendedTerm(fData->sumEntries(), fData->get());
}

double xRooNLLVar::simTerm() const {
    if(auto s = dynamic_cast<RooSimultaneous*>(fPdf.get()); s) {
       return fData->sumEntries()*log(1.0*(s->servers().size()-1)); //one of the servers is the cat
    }
    return 0;
}

double xRooNLLVar::binnedDataTerm() const {
    // this is only relevant if BinnedLikelihood active
    double out=0;
    for(int i=0;i<fData->numEntries();i++){
        fData->get(i);
        out += TMath::LnGamma(fData->weight()+1);
    }
    return out;
}

RooConstraintSum* xRooNLLVar::constraintTerm() const {
    auto _func = func();
    if (auto a = dynamic_cast<RooConstraintSum*>(_func.get()); a) return a;
    for(auto s : _func->servers()) {
        if (auto a = dynamic_cast<RooConstraintSum*>(s); a) return a;
    }
    return nullptr;
}

/*xRooNLLVar::operator RooAbsReal &() const {
    // this works in c++ but not in python
    std::cout << "implicit conversion" << std::endl;
    return *fFunc;
}*/

void xRooNLLVar::xRooHypoPoint::Print() {
    std::cout << "mu: " << fPOIName << std::endl;
    std::cout << "null: " << fNullVal << " , alt: " << fAltVal << std::endl;
    std::cout << "ufit: ";
    if(fUfit) {
        std::cout << fUfit->minNll() << " (status=" << fUfit->status() << ") (mu_hat: " << mu_hat().getVal() << " +/- " << mu_hat().getError() << ")" << std::endl;
    } else {
        std::cout << " Not calculated" << std::endl;
    }
    std::cout << "null cfit: ";
    if(fNull_cfit) {
        std::cout << fNull_cfit->minNll() << " (status=" << fNull_cfit->status() << ")" << std::endl;
    } else {
        std::cout << " Not calculated" << std::endl;
    }
    if (!std::isnan(fAltVal)) {
        std::cout << "alt cfit: ";
        if (fAlt_cfit) {
            std::cout << fAlt_cfit->minNll() << " (status=" << fAlt_cfit->status() << ")" << std::endl;
        } else {
            std::cout << " Not calculated" << std::endl;
        }
        std::cout << "sigma_mu: ";
        if (!fAsimov || !fAsimov->fUfit || !fAsimov->fNull_cfit) {
            std::cout << " Not calculated" << std::endl;
        } else {
            std::cout << sigma_mu().first << " +/- " << sigma_mu().second << std::endl;
        }
    }
    if(fGenFit) std::cout << "genFit: " << fGenFit->GetName() << std::endl;
    std::cout << "nllVar: " << nllVar << std::endl;
}

RooRealVar& xRooNLLVar::xRooHypoPoint::mu_hat() {
    if (ufit()) {
        auto var = dynamic_cast<RooRealVar*>(ufit()->floatParsFinal().find(fPOIName.c_str()));
        if (var) return *var;
        else throw std::runtime_error("Cannot find POI");
    }
    throw std::runtime_error("Unconditional fit unavailable");
}

double xRooNLLVar::xRooHypoPoint::pNull_asymp(double nSigma) {
    return xRooFit::Asymptotics::PValue(fPllType,ts_asymp(nSigma),fNullVal,fNullVal,sigma_mu().first,mu_hat().getMin("physical"),mu_hat().getMax("physical"));
}

double xRooNLLVar::xRooHypoPoint::pAlt_asymp(double nSigma) {
    return xRooFit::Asymptotics::PValue(fPllType,ts_asymp(nSigma),fNullVal,fAltVal,sigma_mu().first,mu_hat().getMin("physical"),mu_hat().getMax("physical"));
}

double xRooNLLVar::xRooHypoPoint::ts_asymp(double nSigma) {
    return (std::isnan(nSigma)) ? pll().first : xRooFit::Asymptotics::k(fPllType,ROOT::Math::gaussian_cdf(nSigma),fNullVal,fAltVal,sigma_mu().first,mu_hat().getMin("physical"),mu_hat().getMax("physical"));
}


std::pair<double,double> xRooNLLVar::xRooHypoPoint::pll() {
    if (!ufit() || ufit()->status() != 0)  return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);
    auto cFactor = xRooFit::Asymptotics::CompatFactor(fPllType, fNullVal, mu_hat().getVal());
    if (cFactor == 0) return std::make_pair(0,0);
    if (!null_cfit() || null_cfit()->status() != 0) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);
    //std::cout << cfit->minNll() << ":" << cfit->edm() << " " << ufit->minNll() << ":" << ufit->edm() << std::endl;
    return std::make_pair(2.*cFactor*(null_cfit()->minNll()-ufit()->minNll()),2.*cFactor*sqrt(pow(null_cfit()->edm(),2)+pow(ufit()->edm(),2)));
    //return 2.*cFactor*(cfit->minNll()+cfit->edm() - ufit->minNll()+ufit->edm());
}

std::shared_ptr<const RooFitResult> xRooNLLVar::xRooHypoPoint::ufit() {
    if (fUfit) return fUfit;
    if (!nllVar) return nullptr;
    AutoRestorer snap(*nllVar->fFuncVars, nllVar);
    nllVar->setData(data);
    nllVar->fFuncVars->setAttribAll("Constant",false);
    *nllVar->fFuncVars = *coords; // will reconst the coords
    dynamic_cast<RooRealVar*>(nllVar->fFuncVars->find(fPOIName.c_str()))->setConstant(false);
    if (fGenFit) {
        // make initial guess same as pars we generated with
        nllVar->fFuncVars->assignValueOnly(fGenFit->constPars());
        nllVar->fFuncVars->assignValueOnly(fGenFit->floatParsFinal());
    }
    return (fUfit = nllVar->minimize());
}

std::shared_ptr<const RooFitResult> xRooNLLVar::xRooHypoPoint::null_cfit() {
    if (fNull_cfit) return fNull_cfit;
    if (!nllVar) return nullptr;
    AutoRestorer snap(*nllVar->fFuncVars, nllVar);
    nllVar->setData(data);
    if (fUfit) {
        // move to ufit coords before evaluating
        *nllVar->fFuncVars = fUfit->floatParsFinal();
    }
    nllVar->fFuncVars->setAttribAll("Constant",false);
    *nllVar->fFuncVars = *coords; // will reconst the coords
    return (fNull_cfit = nllVar->minimize());
}

std::shared_ptr<const RooFitResult> xRooNLLVar::xRooHypoPoint::alt_cfit() {
    if (std::isnan(fAltVal)) return nullptr;
    if (fAlt_cfit) return fAlt_cfit;
    if (!nllVar) return nullptr;
    AutoRestorer snap(*nllVar->fFuncVars, nllVar);
    nllVar->setData(data);
    if (fUfit) {
        // move to ufit coords before evaluating
        *nllVar->fFuncVars = fUfit->floatParsFinal();
    }
    nllVar->fFuncVars->setAttribAll("Constant",false);
    *nllVar->fFuncVars = *coords; // will reconst the coords
    dynamic_cast<RooRealVar*>(nllVar->fFuncVars->find(fPOIName.c_str()))->setVal(fAltVal);
    return (fAlt_cfit = nllVar->minimize());
}

std::pair<double,double> xRooNLLVar::xRooHypoPoint::sigma_mu() {

    if (!fAsimov) {
        if (!alt_cfit() || !nllVar) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);
        AutoRestorer snap(*nllVar->fFuncVars);
        *nllVar->fFuncVars = alt_cfit()->floatParsFinal();
        *nllVar->fFuncVars = alt_cfit()->constPars();
        auto asimov = nllVar->generate(true);
        fAsimov = std::make_shared<xRooHypoPoint>(*this);
        fAsimov->fPllType = xRooFit::Asymptotics::TwoSided;
        fAsimov->fUfit.reset();fAsimov->fNull_cfit.reset();fAsimov->fAlt_cfit.reset();
        fAsimov->data = asimov;
    }
    auto out = fAsimov->pll();
    return std::make_pair(std::abs(fNullVal - fAltVal)/sqrt(out.first), out.second*0.5*std::abs(fNullVal - fAltVal)/(out.first*sqrt(out.first)));
}

xRooNLLVar::xRooHypoPoint xRooNLLVar::xRooHypoPoint::generateNull(int seed) {
    xRooHypoPoint out;
    out.fPOIName = fPOIName; out.coords = coords; out.fPllType = fPllType; out.fNullVal=fNullVal; out.fAltVal = fAltVal;
    out.nllVar = nllVar;
    if (!nllVar) return out;
    *nllVar->fFuncVars = null_cfit()->floatParsFinal();
    *nllVar->fFuncVars = null_cfit()->constPars();
    out.data = nllVar->generate(false,seed);
    out.fGenFit = null_cfit();
    return out;
}

xRooNLLVar::xRooHypoPoint xRooNLLVar::xRooHypoPoint::generateAlt(int seed) {
    xRooHypoPoint out;
    out.fPOIName = fPOIName; out.coords = coords; out.fPllType = fPllType; out.fNullVal=fNullVal; out.fAltVal = fAltVal;
    out.nllVar = nllVar;
    if (!nllVar) return out;
    if (!alt_cfit()) return out;
    *nllVar->fFuncVars = alt_cfit()->floatParsFinal();
    *nllVar->fFuncVars = alt_cfit()->constPars();
    out.data = nllVar->generate(false,seed);
    out.fGenFit = alt_cfit();
    return out;
}

xRooNLLVar::xRooHypoPoint xRooNLLVar::hypoPoint(const char* parName, double value, double alt_value, const xRooFit::Asymptotics::PLLType& pllType) {
    xRooHypoPoint out;
    out.fPOIName = parName;
    out.fNullVal = value; out.fAltVal = alt_value;
    out.nllVar = this;
    out.data = getData();

    if (!fFuncVars) { reinitialize(); }

    auto poi = dynamic_cast<RooRealVar*>(fFuncVars->find(parName));
    if (!poi) return out;
    poi->setVal(value);
    poi->setConstant();
    auto _snap = std::unique_ptr<RooAbsCollection>(fFuncVars->selectByAttrib("Constant",true))->snapshot();
    if(fGlobs) _snap->remove(*fGlobs,true,true);
    out.coords.reset( _snap );


    auto _type = pllType;
    if (_type == xRooFit::Asymptotics::Unknown) {
        // decide based on values
        if (std::isnan(alt_value)) _type = xRooFit::Asymptotics::TwoSided;
        else if(value > alt_value) _type = xRooFit::Asymptotics::OneSidedPositive;
        else _type = xRooFit::Asymptotics::Uncapped;
    }

    out.fPllType = _type;

    return out;

}
