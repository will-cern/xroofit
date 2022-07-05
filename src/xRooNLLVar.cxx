
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
#include "TEfficiency.h"

#include "RooRealVar.h"
#include "Math/ProbFunc.h"
#include "RooRandom.h"

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

#define private public
#include "RooWorkspace.h"
#undef private

void xRooNLLVar::reinitialize() {
    TString oldName = ""; if (std::shared_ptr<RooAbsReal>::get()) oldName = std::shared_ptr<RooAbsReal>::get()->GetName();
    if (fPdf) {
        cout_redirect c(fFuncCreationLog);
        // need to find all RooRealSumPdf nodes and mark them binned or unbinned as required
        RooArgSet s; fPdf->treeNodeServerList(&s,nullptr,true,false);
        bool isBinned=false;
        bool hasBinned=false; // if no binned option then 'auto bin' ...
        if (auto a = dynamic_cast<RooCmdArg*>(fOpts->find("Binned"));a) {
            hasBinned = true; isBinned = a->getInt(0);
        }
        std::map<RooAbsArg*,bool> origValues;
        if (hasBinned) {
            for (auto a: s) {
                if (a->InheritsFrom("RooRealSumPdf")) {
                    // since RooNLLVar will assume binBoundaries available (not null), we should check bin boundaries available
                    bool setBinned = false;
                    if (isBinned) {
                        std::unique_ptr<RooArgSet> obs(a->getObservables(fData->get()));
                        if (obs->size() == 1) { // RooNLLVar requires exactly 1 obs
                            auto *var = static_cast<RooRealVar *>(obs->first());
                            std::unique_ptr<std::list<Double_t>> boundaries{
                                    dynamic_cast<RooAbsReal *>(a)->binBoundaries(*var, var->getMin(), var->getMax())};
                            if (boundaries) {
                                if (!std::shared_ptr<RooAbsReal>::get())
                                    Info("xRooNLLVar", "%s will be evaluated as a Binned PDF (%d bins)", a->GetName(),
                                         int(boundaries->size()-1));
                                setBinned = true;
                            }
                        }
                    }
                    origValues[a] = a->getAttribute("BinnedLikelihood");
                    a->setAttribute("BinnedLikelihood", setBinned);
                }
            }
        }
        // before creating, clear away caches if any if pdf is in ws
        if (fPdf->_myws) {
            std::set<std::string> setNames;
            for(auto& a : fPdf->_myws->_namedSets) {
                if (TString(a.first.c_str()).BeginsWith("CACHE_")) { setNames.insert(a.first); }
            }
            for(auto& a : setNames) fPdf->_myws->removeSet(a.c_str());
        }
        this->reset( fPdf->createNLL(*fData,*fOpts) );
        if(oldName!="") std::shared_ptr<RooAbsReal>::get()->SetName(oldName);
        if(!origValues.empty()) {
            // need to evaluate NOW so that slaves are created while the BinnedLikelihood settings are in place
            std::shared_ptr<RooAbsReal>::get()->getVal();
            for(auto& [o,v] : origValues) o->setAttribute("BinnedLikelihood",v);
        }

    }

    fFuncVars.reset( std::shared_ptr<RooAbsReal>::get()->getVariables() );
    if(fGlobs) {fFuncGlobs.reset( fFuncVars->selectCommon(*fGlobs) );fFuncGlobs->setAttribAll("Constant",true);}
    fConstVars.reset( fFuncVars->selectByAttrib("Constant",true) ); // will check if any of these have floated
}

std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> xRooNLLVar::generate(bool expected,int seed) {
    if(!fPdf) return std::pair(nullptr,nullptr);
    auto fr = std::make_shared<RooFitResult>();
    fr->setFinalParList(RooArgList());
    RooArgList l; l.add((fFuncVars) ? *fFuncVars : *std::unique_ptr<RooAbsCollection>(fPdf->getParameters(*fData)));
    fr->setConstParList(l);
    fr->_constPars->setAttribAll("global",false);
    if(fGlobs) std::unique_ptr<RooAbsCollection>(fr->_constPars->selectCommon(*fGlobs))->setAttribAll("global",true);
    return xRooFit::generateFrom(*fPdf, fr,expected,seed);
}

xRooNLLVar::xRooFitResult::xRooFitResult(const std::shared_ptr<xRooNode>& in): std::shared_ptr<const RooFitResult>(std::dynamic_pointer_cast<const RooFitResult>(in->fComp)), fNode(in) { }
const RooFitResult* xRooNLLVar::xRooFitResult::operator->() const { return fNode->get<RooFitResult>(); }
//xRooNLLVar::xRooFitResult::operator std::shared_ptr<const RooFitResult>() const { return std::dynamic_pointer_cast<const RooFitResult>(fNode->fComp); }
xRooNLLVar::xRooFitResult::operator const RooFitResult*() const { return fNode->get<const RooFitResult>(); }
void xRooNLLVar::xRooFitResult::Draw(Option_t* opt) { fNode->Draw(opt); }

xRooNLLVar::xRooFitResult xRooNLLVar::minimize(const std::shared_ptr<ROOT::Fit::FitConfig>& _config) {
    auto out = xRooFit::minimize(*get(),(_config) ? _config : fitConfig());
    // add any pars that are const here that aren't in constPars list because they may have been
    // const-optimized and their values cached with the dataset, so if subsequently floated the
    // nll wont evaluate correctly
    //fConstVars.reset( fFuncVars->selectByAttrib("Constant",true) );
    if(out) {
        out->_constPars->setAttribAll("global",false);
        if(fGlobs) std::unique_ptr<RooAbsCollection>(out->_constPars->selectCommon(*fGlobs))->setAttribAll("global",true);
    }
    return xRooFitResult(std::make_shared<xRooNode>(out,fPdf));
}

class AutoRestorer {
public:
    AutoRestorer(const RooAbsCollection& s, xRooNLLVar* nll=nullptr) : fSnap(s.snapshot()), fNll(nll) {
        fPars.add(s);
        if(fNll) {fOldData = fNll->getData(); fOldName = fNll->get()->GetName(); fOldTitle = fNll->get()->getStringAttribute("fitresultTitle"); }
    }
    ~AutoRestorer() { ((RooAbsCollection&)fPars) = *fSnap; if(fNll) {fNll->setData(fOldData); fNll->get()->SetName(fOldName); fNll->get()->setStringAttribute("fitresultTitle",(fOldTitle=="") ? nullptr : fOldTitle); } }
    RooArgSet fPars;
    std::unique_ptr<RooAbsCollection> fSnap;
    xRooNLLVar* fNll = nullptr;
    std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>> fOldData;
    TString fOldName,fOldTitle;
};

std::shared_ptr<ROOT::Fit::FitConfig> xRooNLLVar::fitConfig() {
    if (!fFitConfig) fFitConfig = xRooFit::createFitConfig();
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
    get()->SetName(TString::Format("%s/%s_toys",get()->GetName(),cfit_prime->GetName()));
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
#include "TArrow.h"

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
                if(!normRange) {
                    gr->Draw("A");
                    gPad->SetGrid();
                }
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
        // add arrow to show current par value
        TArrow a; a.DrawArrow(init,0,init,-0.1);
        gPad->Update();gSystem->ProcessEvents();
        v->setVal(init);
    } else {
        Error("Draw","Name a parameter to scan over: Draw(<name>) , choose from: %s",_pars->empty() ? "" : _pars->contentsString().c_str());
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

RooArgList xRooNLLVar::xRooHypoPoint::poi() {
    RooArgList out; out.setName("poi");
    out.add( *std::unique_ptr<RooAbsCollection>(coords->selectByAttrib("poi",true)) );
    return out;
}

RooArgList xRooNLLVar::xRooHypoPoint::alt_poi() {
    RooArgList out; out.setName("alt_poi");
    out.addClone( *std::unique_ptr<RooAbsCollection>(coords->selectByAttrib("poi",true)) );
    for(auto a : out) {
        auto v = dynamic_cast<RooAbsRealLValue*>(a);
        if(!v) continue;
        if(auto s = a->getStringAttribute("altHypo"); s && strlen(s)) {
            v->setVal(TString(s).Atof());
        } else {
            v->setVal(std::numeric_limits<double>::quiet_NaN());
        }
    }
    return out;
}

void xRooNLLVar::xRooHypoPoint::Print() {
    std::cout << "POI: " << poi().contentsString() << " , null: " << dynamic_cast<RooAbsReal*>(poi().first())->getVal() << " , alt: " << dynamic_cast<RooAbsReal*>(alt_poi().first())->getVal() << std::endl;
    std::cout << "pllType: " << fPllType << " , ufit: ";
    if(fUfit) {
        std::cout << fUfit->minNll() << " (status=" << fUfit->status() << ") (mu_hat: " << mu_hat().getVal() << " +/- " << mu_hat().getError() << ")" << std::endl;
    } else {
        std::cout << " Not calculated" << std::endl;
    }
    std::cout << "null cfit: ";
    if(fNull_cfit) {
        std::cout << fNull_cfit->minNll() << " (status=" << fNull_cfit->status() << ")";
    } else {
        std::cout << " Not calculated";
    }
    if (!std::isnan(dynamic_cast<RooAbsReal*>(poi().first())->getVal())) {
        std::cout << " , alt cfit: ";
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
    } else {
        std::cout << std::endl;
    }
    if(fGenFit) std::cout << "genFit: " << fGenFit->GetName() << std::endl;
    if (!nullToys.empty() || !altToys.empty()) {
        std::cout << "null toys: " << nullToys.size();
        size_t firstToy = 0; while(firstToy < nullToys.size() && std::isnan(std::get<1>(nullToys[firstToy]))) firstToy++;
        if (firstToy>0) std::cout << " [ of which " << firstToy << " are bad]";
        std::cout << " , alt toys: " << altToys.size();
        firstToy = 0; while(firstToy < altToys.size() && std::isnan(std::get<1>(altToys[firstToy]))) firstToy++;
        if (firstToy>0) std::cout << " [ of which " << firstToy << " are bad]";
        std::cout << std::endl;
    }
    std::cout << "nllVar: " << nllVar << std::endl;
}

RooRealVar& xRooNLLVar::xRooHypoPoint::mu_hat() {
    if (ufit()) {
        auto var = dynamic_cast<RooRealVar*>(ufit()->floatParsFinal().find(fPOIName()));
        if (var) return *var;
        else throw std::runtime_error("Cannot find POI");
    }
    throw std::runtime_error("Unconditional fit unavailable");
}

double xRooNLLVar::xRooHypoPoint::pNull_asymp(double nSigma) {
    if(fPllType != xRooFit::Asymptotics::Uncapped && ts_asymp(nSigma)==0) return 1;
    auto first_poi = dynamic_cast<RooRealVar*>(poi().first());
    if (!first_poi) return std::numeric_limits<double>::quiet_NaN();
    return xRooFit::Asymptotics::PValue(fPllType,ts_asymp(nSigma),fNullVal(),fNullVal(),sigma_mu().first,first_poi->getMin("physical"),first_poi->getMax("physical"));
}

double xRooNLLVar::xRooHypoPoint::pAlt_asymp(double nSigma) {
    if(fPllType != xRooFit::Asymptotics::Uncapped && ts_asymp(nSigma)==0) return 1;
    auto first_poi = dynamic_cast<RooRealVar*>(poi().first());
    if (!first_poi) return std::numeric_limits<double>::quiet_NaN();
    return xRooFit::Asymptotics::PValue(fPllType,ts_asymp(nSigma),fNullVal(),fAltVal(),sigma_mu().first,first_poi->getMin("physical"),first_poi->getMax("physical"));
}

double xRooNLLVar::xRooHypoPoint::ts_asymp(double nSigma) {
    auto first_poi = dynamic_cast<RooRealVar*>(poi().first());
    if (!first_poi) return std::numeric_limits<double>::quiet_NaN();
    return (std::isnan(nSigma)) ? pll().first : xRooFit::Asymptotics::k(fPllType,ROOT::Math::gaussian_cdf(nSigma),fNullVal(),fAltVal(),sigma_mu().first,first_poi->getMin("physical"),first_poi->getMax("physical"));
}

std::pair<double,double> xRooNLLVar::xRooHypoPoint::ts_toys(double nSigma) {
    if(std::isnan(nSigma)) return pll();
    //nans should appear in the alt toys first ... so loop until past nans
    size_t firstToy = 0; while(firstToy < altToys.size() && std::isnan(std::get<1>(altToys[firstToy]))) firstToy++;
    if (firstToy>=altToys.size()) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::quiet_NaN());
    int targetIdx = (altToys.size()-firstToy)*ROOT::Math::gaussian_cdf(nSigma) + firstToy; // TODO: Account for weights
    return std::make_pair(std::get<1>(altToys[targetIdx]),(std::get<1>(altToys[std::min(int(altToys.size()),targetIdx)]) - std::get<1>(altToys[std::max(0,targetIdx)]))/2.);
}


std::pair<double,double> xRooNLLVar::xRooHypoPoint::pll() {
    if (!ufit() || ufit()->status() != 0)  return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);
    auto cFactor = xRooFit::Asymptotics::CompatFactor(fPllType, fNullVal(), mu_hat().getVal());
    if (cFactor == 0) return std::make_pair(0,0);
    if (!null_cfit() || null_cfit()->status() != 0) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);
    //std::cout << cfit->minNll() << ":" << cfit->edm() << " " << ufit->minNll() << ":" << ufit->edm() << std::endl;
    return std::make_pair(2.*cFactor*(null_cfit()->minNll()-ufit()->minNll()),2.*cFactor*sqrt(pow(null_cfit()->edm(),2)+pow(ufit()->edm(),2)));
    //return 2.*cFactor*(cfit->minNll()+cfit->edm() - ufit->minNll()+ufit->edm());
}

std::shared_ptr<const RooFitResult> xRooNLLVar::xRooHypoPoint::ufit() {
    if (fUfit) return fUfit;
    if (!nllVar) return nullptr;
    if(!nllVar->fFuncVars) nllVar->reinitialize();
    AutoRestorer snap(*nllVar->fFuncVars, nllVar.get());
    nllVar->setData(data);
    nllVar->fFuncVars->setAttribAll("Constant",false);
    *nllVar->fFuncVars = *coords; // will reconst the coords
    if(nllVar->fFuncGlobs) nllVar->fFuncGlobs->setAttribAll("Constant",true);
    dynamic_cast<RooRealVar*>(nllVar->fFuncVars->find(fPOIName()))->setConstant(false);
    if (fGenFit) {
        // make initial guess same as pars we generated with
        nllVar->fFuncVars->assignValueOnly(fGenFit->constPars());
        nllVar->fFuncVars->assignValueOnly(fGenFit->floatParsFinal());
        // rename nll so if caching fit results will cache into subdir
        nllVar->get()->SetName(TString::Format("%s/%s_%s",nllVar->get()->GetName(),fGenFit->GetName(),(isExpected) ? "asimov" : "toys"));
    }
    return (fUfit = nllVar->minimize());
}

#include "RooStringVar.h"
std::string collectionContents(const RooAbsCollection& coll) {
    std::string out;
    for(auto & c : coll) {
        if(!out.empty()) out += ",";
        out += c->GetName();
        if(auto v = dynamic_cast<RooAbsReal*>(c); v) out += TString::Format("=%g",v->getVal());
        else if(auto cc = dynamic_cast<RooAbsCategory*>(c); cc) out += TString::Format("=%s",cc->getLabel());
        else if(auto s = dynamic_cast<RooStringVar*>(c); v) out += TString::Format("=%s",s->getVal());
    }
    return out;
}

std::shared_ptr<const RooFitResult> xRooNLLVar::xRooHypoPoint::null_cfit() {
    if (fNull_cfit) return fNull_cfit;
    if (!nllVar) return nullptr;
    if(!nllVar->fFuncVars) nllVar->reinitialize();
    AutoRestorer snap(*nllVar->fFuncVars, nllVar.get());
    nllVar->setData(data);
    if (fUfit) {
        // move to ufit coords before evaluating
        *nllVar->fFuncVars = fUfit->floatParsFinal();
    }
    nllVar->fFuncVars->setAttribAll("Constant",false);
    *nllVar->fFuncVars = *coords; // will reconst the coords
    if(nllVar->fFuncGlobs) nllVar->fFuncGlobs->setAttribAll("Constant",true);
    nllVar->fFuncVars->find(fPOIName())->setStringAttribute("altHypo",(!std::isnan(fAltVal())) ? TString::Format("%g",fAltVal()) : nullptr);
    if(fGenFit) nllVar->get()->SetName(TString::Format("%s/%s_%s",nllVar->get()->GetName(),fGenFit->GetName(),(isExpected) ? "asimov" : "toys"));
    nllVar->get()->setStringAttribute("fitresultTitle",collectionContents(poi()).c_str());
    return (fNull_cfit = nllVar->minimize());
}

std::shared_ptr<const RooFitResult> xRooNLLVar::xRooHypoPoint::alt_cfit() {
    if (std::isnan(fAltVal())) return nullptr;
    if (fAlt_cfit) return fAlt_cfit;
    if (!nllVar) return nullptr;
    if(!nllVar->fFuncVars) nllVar->reinitialize();
    AutoRestorer snap(*nllVar->fFuncVars, nllVar.get());
    nllVar->setData(data);
    if (fUfit) {
        // move to ufit coords before evaluating
        *nllVar->fFuncVars = fUfit->floatParsFinal();
    }
    nllVar->fFuncVars->setAttribAll("Constant",false);
    *nllVar->fFuncVars = *coords; // will reconst the coords
    if(nllVar->fFuncGlobs) nllVar->fFuncGlobs->setAttribAll("Constant",true);
    *nllVar->fFuncVars = alt_poi();
    if(fGenFit) nllVar->get()->SetName(TString::Format("%s/%s_%s",nllVar->get()->GetName(),fGenFit->GetName(),(isExpected) ? "asimov" : "toys"));
    nllVar->get()->setStringAttribute("fitresultTitle",collectionContents(alt_poi()).c_str());
    return (fAlt_cfit = nllVar->minimize());
}

std::pair<double,double> xRooNLLVar::xRooHypoPoint::sigma_mu() {

    if (!fAsimov) {
        if (!alt_cfit() || !nllVar) return std::make_pair(std::numeric_limits<double>::quiet_NaN(),0);
        if(!nllVar->fFuncVars) nllVar->reinitialize();
        AutoRestorer snap(*nllVar->fFuncVars);
        *nllVar->fFuncVars = alt_cfit()->floatParsFinal();
        *nllVar->fFuncVars = alt_cfit()->constPars();
        auto asimov = nllVar->generate(true);
        fAsimov = std::make_shared<xRooHypoPoint>(*this);
        fAsimov->fPllType = xRooFit::Asymptotics::TwoSided;
        fAsimov->fUfit.reset();fAsimov->fNull_cfit.reset();fAsimov->fAlt_cfit.reset();
        fAsimov->data = asimov;
        fAsimov->fGenFit = fAlt_cfit;
        fAsimov->isExpected = true;
    }
    auto out = fAsimov->pll();
    return std::make_pair(std::abs(fNullVal() - fAltVal())/sqrt(out.first), out.second*0.5*std::abs(fNullVal() - fAltVal())/(out.first*sqrt(out.first)));
}

std::pair<double,double> xRooNLLVar::xRooHypoPoint::pX_toys(bool alt, double nSigma) {
    auto _ts = ts_toys(nSigma);
    if (std::isnan(_ts.first)) return _ts;

    TEfficiency eff("","",1,0,1);

    auto& _theToys = (alt) ? altToys : nullToys;

    // loop over toys, count how many are > ts value
    // nans (mean bad ts evaluations) will count towards uncertainty
    int nans = 0;
    double result = 0; double result_err_up = 0;  double result_err_down = 0;
    for(auto& toy : _theToys) {
        if (std::isnan(std::get<1>(toy))) nans++;
        else {
            bool res = std::get<1>(toy) >= _ts.first;
            if(std::get<2>(toy)!=1) eff.FillWeighted(res,0.5,std::get<2>(toy)); else eff.Fill(res,0.5);
            if(res) result+=std::get<2>(toy);
            if(std::get<1>(toy) >= _ts.first-_ts.second) result_err_up+=std::get<2>(toy);
            if(std::get<1>(toy) >= _ts.first-_ts.second) result_err_down+=std::get<2>(toy);
        }
    }
    // symmetrize the error
    result_err_up -= result; result_err_down -= result;
    double result_err = std::max(std::abs(result_err_up),std::abs(result_err_down));
    // assume the nans would "add" to the p-value, conservative scenario
    result_err += nans;
    result_err /= _theToys.size();

    // don't include the nans for the central value though
    result /= (_theToys.size() - nans);

    // add to the result_err (in quadrature) the uncert due to limited stats
    result_err = sqrt(result_err*result_err + eff.GetEfficiencyErrorUp(1)*eff.GetEfficiencyErrorUp(1));
    return std::make_pair(result,result_err);
}

std::pair<double,double> xRooNLLVar::xRooHypoPoint::pNull_toys(double nSigma) {
    return pX_toys(false,nSigma);
}

std::pair<double,double> xRooNLLVar::xRooHypoPoint::pAlt_toys(double nSigma) {
    return pX_toys(true,nSigma);
}

xRooNLLVar::xRooHypoPoint xRooNLLVar::xRooHypoPoint::generateNull(int seed) {
    xRooHypoPoint out;
    out.coords = coords; out.fPllType = fPllType; //out.fPOIName = fPOIName; out.fNullVal=fNullVal; out.fAltVal = fAltVal;
    out.nllVar = nllVar;
    if (!nllVar) return out;
    if(!nllVar->fFuncVars) nllVar->reinitialize();
    *nllVar->fFuncVars = null_cfit()->floatParsFinal();
    *nllVar->fFuncVars = null_cfit()->constPars();
    out.data = nllVar->generate(false,seed);
    out.fGenFit = null_cfit();
    return out;
}

xRooNLLVar::xRooHypoPoint xRooNLLVar::xRooHypoPoint::generateAlt(int seed) {
    xRooHypoPoint out;
    out.coords = coords; out.fPllType = fPllType; //out.fPOIName = fPOIName; out.fNullVal=fNullVal; out.fAltVal = fAltVal;
    out.nllVar = nllVar;
    if (!nllVar) return out;
    if (!alt_cfit()) return out;
    if(!nllVar->fFuncVars) nllVar->reinitialize();
    *nllVar->fFuncVars = alt_cfit()->floatParsFinal();
    *nllVar->fFuncVars = alt_cfit()->constPars();
    out.data = nllVar->generate(false,seed);
    out.fGenFit = alt_cfit();
    return out;
}

#include "TDirectory.h"

void xRooNLLVar::xRooHypoPoint::addToys(bool alt,int nToys) {
    auto& toys = (alt) ? altToys : nullToys;
    int nans=0;
    std::vector<float> times(nToys);
    float lastTime = 0;int lasti = -1;
    TStopwatch s2; s2.Start(); TStopwatch s; s.Start();
    for(auto i = 0;i<nToys;i++) {
        int seed = RooRandom::randomGenerator()->Integer(std::numeric_limits<uint32_t>::max());
        toys.push_back( std::make_tuple(seed, ((alt) ? generateAlt(seed) : generateNull(seed)).pll().first , 1.) );
        if(std::isnan(std::get<1>(toys.back()))) nans++;
        times[i] = s.RealTime() - lastTime; // stops the clock
        lastTime = s.RealTime();
        if (s.RealTime() > 3) {
            std::cout << "\r" << TString::Format("Generated %d/%d %s hypothesis toys [%.2f toys/s]...",i+1,nToys,alt ? "alt" : "null",double(i-lasti)/s.RealTime()) << std::flush;
            lasti = i;
            s.Reset();s.Start();
            //std::cout << "Generated " << i << "/" << nToys << (alt ? " alt " : " null ") << " hypothesis toys " ..." << std::endl;
        }
        s.Continue();
    }
    if(lasti) std::cout << "\r" << TString::Format("Generated %d/%d %s hypothesis toys [%.2f toys/s overall]...Done!",nToys,nToys,alt ? "alt" : "null",double(nToys)/s2.RealTime()) << std::endl;
    auto g = gDirectory->Get<TGraph>("toyTime"); if(!g) {g = new TGraph;g->SetNameTitle("toyTime","Time per toy;Toy;time [s]");gDirectory->Add(g); }
    g->Set(times.size()); for(size_t i=0;i<times.size();i++) g->SetPoint(i,i,times[i]);
    // sort the toys ... put nans first - do by setting all as negative inf
    for(auto& t : toys) { if(std::isnan(std::get<1>(t))) std::get<1>(t)=-std::numeric_limits<double>::infinity(); }
    std::sort(toys.begin(),toys.end(),[](const decltype (nullToys)::value_type & a, const decltype (nullToys)::value_type & b) -> bool
    {
        if(std::isnan(std::get<1>(a))) return true;
        if(std::isnan(std::get<1>(b))) return false;
        return std::get<1>(a) < std::get<1>(b);
    });
    for(auto& t : toys) { if(std::isinf(std::get<1>(t))) std::get<1>(t)=std::numeric_limits<double>::quiet_NaN(); }
    if (nans>0) std::cout << "Warning: " << nans << " toys were bad" << std::endl;
}

void xRooNLLVar::xRooHypoPoint::addNullToys(int nToys) {
   addToys(false,nToys);
}
void xRooNLLVar::xRooHypoPoint::addAltToys(int nToys) {
    addToys(true,nToys);
}

xRooNLLVar::xRooHypoPoint xRooNLLVar::hypoPoint(const char* parName, double value, double alt_value, const xRooFit::Asymptotics::PLLType& pllType) {
    xRooHypoPoint out;
    //out.fPOIName = parName; out.fNullVal = value; out.fAltVal = alt_value;

    if (!fFuncVars) { reinitialize(); }

    out.nllVar = std::make_shared<xRooNLLVar>(*this);
    out.data = getData();



    auto poi = dynamic_cast<RooRealVar*>(fFuncVars->find(parName));
    if (!poi) return out;
    AutoRestorer snap((RooArgSet(*poi)));
    poi->setVal(value);
    poi->setConstant();
    auto _snap = std::unique_ptr<RooAbsCollection>(fFuncVars->selectByAttrib("Constant",true))->snapshot();
    _snap->find(poi->GetName())->setAttribute("poi",true);
    if(std::isnan(alt_value)) _snap->find(poi->GetName())->setStringAttribute("altHypo",nullptr);
    else _snap->find(poi->GetName())->setStringAttribute("altHypo",TString::Format("%g",alt_value));
    if(fGlobs) _snap->remove(*fGlobs,true,true);
    out.coords.reset( _snap );


    auto _type = pllType;
    if (_type == xRooFit::Asymptotics::Unknown) {
        // decide based on values
        if (std::isnan(alt_value)) _type = xRooFit::Asymptotics::TwoSided;
        else if(value >= alt_value) _type = xRooFit::Asymptotics::OneSidedPositive;
        else _type = xRooFit::Asymptotics::Uncapped;
    }

    out.fPllType = _type;

    return out;

}

#include "TStyle.h"
#include "TH1D.h"
#include "TLegend.h"

void xRooNLLVar::xRooHypoPoint::Draw(Option_t* opt) {

    if (!nllVar) return;

    TString sOpt(opt);
    sOpt.ToLower();
    bool hasSame = sOpt.Contains("same"); sOpt.ReplaceAll("same","");

    TVirtualPad *pad = gPad;

    TH1* hAxis = nullptr;

    auto clearPad = []() {
        gPad->Clear();
        if (gPad->GetNumber()==0) {
            gPad->SetBottomMargin(gStyle->GetPadBottomMargin());
            gPad->SetTopMargin(gStyle->GetPadTopMargin());
            gPad->SetLeftMargin(gStyle->GetPadLeftMargin());
            gPad->SetRightMargin(gStyle->GetPadRightMargin());
        }
    };

    if (!hasSame || !pad) {
        if (!pad) {
            TCanvas::MakeDefCanvas();
            pad = gPad;
        }
        clearPad();
    } else {
        // get the histogram representing the axes
        hAxis = dynamic_cast<TH1*>(pad->GetPrimitive("axis"));
        if (!hAxis) {
            for(auto o : *pad->GetListOfPrimitives()) {
                if (hAxis = dynamic_cast<TH1*>(o); hAxis) break;
            }
        }
    }


    // get min and max values
    double _min = std::numeric_limits<double>::quiet_NaN();
    double _max = -std::numeric_limits<double>::quiet_NaN();

    for(auto& p : nullToys) {
        if(std::get<2>(p)==0) continue;
        if (std::isnan(std::get<1>(p))) continue;
        _min = std::min(std::get<1>(p),_min);
        _max = std::max(std::get<1>(p),_max);
    }
    for(auto& p : altToys) {
        if(std::get<2>(p)==0) continue;
        if (std::isnan(std::get<1>(p))) continue;
        _min = std::min(std::get<1>(p),_min);
        _max = std::max(std::get<1>(p),_max);
    }

    auto obs = pll();
    if (!std::isnan(obs.first)) {
        _min = std::min(obs.first-abs(obs.first)*0.1,_min);
        _max = std::max(obs.first+abs(obs.first)*0.1,_max);
    }

    auto asi = (fAsimov && fAsimov->fUfit && fAsimov->fNull_cfit) ? fAsimov->pll().first : std::numeric_limits<double>::quiet_NaN();
    if (!std::isnan(asi) && asi>0 && fPllType != xRooFit::Asymptotics::Unknown) {
        // can calculate asymptotic distributions,
        _min = std::min(asi-abs(asi),_min);
        _max = std::max(asi+abs(asi),_max);
    }
    if(_min>0) _min=0;

    if(!nllVar->fFuncVars) nllVar->reinitialize();
    auto poi = dynamic_cast<RooRealVar*>(nllVar->fFuncVars->find(fPOIName()));

    auto makeHist = [&](bool isAlt) {
        TString title;
        auto h = new TH1D((isAlt) ? "alt_toys" : "null_toys", "", 100, _min, _max + (_max - _min) * 0.01);
        h->SetDirectory(0);
        size_t nBadOrZero = 0;
        for (auto &p: (isAlt) ? altToys : nullToys) {
            double w = std::isnan(std::get<1>(p)) ? 0 : std::get<2>(p);
            if (w == 0) nBadOrZero++;
            if(!std::isnan(std::get<1>(p))) h->Fill(std::get<1>(p), w);
        }
        if (h->GetEntries() > 0) h->Scale(1. / h->Integral(0, h->GetNbinsX() + 1));

        // add POI values to identify hypos
//        for(auto p : *fPOI) {
//            if (auto v = dynamic_cast<RooRealVar*>(p)) {
//                if (auto v2 = dynamic_cast<RooRealVar*>(fAltPoint->fCoords->find(*v)); v2 && v2->getVal()!=v->getVal()) {
//                    // found point that differs in poi and altpoint value, so print my coords value for this
//                    title += TString::Format("%s' = %g, ",v->GetTitle(),dynamic_cast<RooRealVar*>(fCoords->find(*v))->getVal());
//                }
//            }
//        }
        title += TString::Format("%s' = %g",fPOIName(), (isAlt) ? fAltVal() : fNullVal());
        title += TString::Format(" , N_{toys}=%lu",(isAlt) ? altToys.size() : nullToys.size());
        if (nBadOrZero > 0) title += TString::Format(" (N_{bad/0}=%lu)",nBadOrZero);
        auto v = poi;
        if(fPllType == xRooFit::Asymptotics::OneSidedPositive) {
            if (v && v->getMin()==0) title += TString::Format(";#tilde{q}_{%s=%g}",v->GetTitle(),v->getVal());
            else if(v) title += TString::Format(";q_{%s=%g}",v->GetTitle(),v->getVal());
            else title += ";q";
        } else if(fPllType == xRooFit::Asymptotics::TwoSided) {
            if (v && v->getMin()==0) title += TString::Format(";#tilde{t}_{%s=%g}",v->GetTitle(),v->getVal());
            else if(v) title += TString::Format(";t_{%s=%g}",v->GetTitle(),v->getVal());
            else title += ";t";
        } else if(fPllType == xRooFit::Asymptotics::OneSidedNegative) {
            if (v && v->getMin()==0) title += TString::Format(";#tilde{r}_{%s=%g}",v->GetTitle(),v->getVal());
            else if(v) title += TString::Format(";r_{%s=%g}",v->GetTitle(),v->getVal());
            else title += ";r";
        } else if(fPllType == xRooFit::Asymptotics::Uncapped) {
            if (v && v->getMin()==0) title += TString::Format(";#tilde{s}_{%s=%g}",v->GetTitle(),v->getVal());
            else if(v) title += TString::Format(";s_{%s=%g}",v->GetTitle(),v->getVal());
            else title += ";s";
        } else {
            title += ";Test Statistic";
        }
        title += TString::Format(";Probability Mass");
        h->SetTitle(title);
        h->SetLineColor(isAlt ? kRed : kBlue); h->SetLineWidth(2);
        h->SetMarkerSize(0);
        h->SetBit(kCanDelete);
        return h;
    };

    auto nullHist = makeHist(false);
    auto altHist = makeHist(true);


    TLegend* l = nullptr;
    auto h = nullHist;
    if (!hasSame) {
        gPad->SetLogy();
        auto axis = (TH1*)h->Clone(".axis");
        axis->Reset("ICES");
        axis->SetMinimum(1e-7); axis->SetMaximum(h->GetMaximum());
        axis->SetTitle(TString::Format("HypoPoint"));
        axis->SetLineWidth(0);
        axis->Draw("");//h->Draw("axis"); cant use axis option if want title drawn
        hAxis=axis;
        l = new TLegend(0.4,0.7,1.-gPad->GetRightMargin(),1.-gPad->GetTopMargin());
        l->SetName("legend");
        l->SetFillStyle(0);l->SetBorderSize(0);
        l->SetBit(kCanDelete);
        l->Draw();
    } else {
        for(auto o : *gPad->GetListOfPrimitives()) {
            l = dynamic_cast<TLegend*>(o);
            if (l) break;
        }
    }

    if(h->GetEntries()>0) h->Draw( "histesame" );
    else h->Draw("axissame");// for unknown reason if second histogram empty it still draws with two weird bars???
    h = altHist;
    if(h->GetEntries()>0) h->Draw( "histesame" );
    else h->Draw("axissame");// for unknown reason if second histogram empty it still draws with two weird bars???

    if(l) { l->AddEntry(nullHist); l->AddEntry(altHist); }


    if (!std::isnan(sigma_mu().first) && !std::isnan(fAltVal()) && fAsimov && fAsimov->fUfit && fAsimov->fNull_cfit) {
        auto hh = (TH1 *) nullHist->Clone("null_asymp");
        hh->SetLineStyle(2);
        hh->Reset();
        for (int i = 1; i <= hh->GetNbinsX(); i++) {
            hh->SetBinContent(i,
                              xRooFit::Asymptotics::PValue(fPllType, hh->GetBinLowEdge(i), fNullVal(), fNullVal(),
                                                           sigma_mu().first, poi->getMin("physical"), poi->getMax("physical")) -
                              xRooFit::Asymptotics::PValue(fPllType, hh->GetBinLowEdge(i + 1), fNullVal(),
                                                           fNullVal(), sigma_mu().first, poi->getMin("physical"), poi->getMax("physical")));
        }
        hh->Draw("lsame");
        hh = (TH1 *) altHist->Clone("alt_asymp");
        hh->SetLineStyle(2);
        hh->Reset();
        for (int i = 1; i <= hh->GetNbinsX(); i++) {
            hh->SetBinContent(i,
                              xRooFit::Asymptotics::PValue(fPllType, hh->GetBinLowEdge(i), fNullVal(), fAltVal(),
                                                           sigma_mu().first, poi->getMin("physical"), poi->getMax("physical")) -
                              xRooFit::Asymptotics::PValue(fPllType, hh->GetBinLowEdge(i + 1), fNullVal(),
                                                           fAltVal(), sigma_mu().first, poi->getMin("physical"), poi->getMax("physical")));
        }
        hh->Draw("lsame");
    }

    // draw observed points
    TLine ll; ll.SetLineStyle(2);
    //for(auto p : fObs) {
        auto tl = ll.DrawLine(pll().first,hAxis->GetMinimum(),pll().first,0.1);
        auto label = TString::Format("obs ts = %.4f",pll().first);
        if (pll().second) label += TString::Format(" +/- %.4f",pll().second);
        auto pNull = pNull_toys();
        auto pAlt = pAlt_toys();

        auto pNullA = pNull_asymp();
        auto pAltA = pAlt_asymp();

        l->AddEntry(tl,label,"l");
        label="";
        if (!std::isnan(pNull.first) || !std::isnan(pAlt.first)) {
            auto pCLs = pCLs_toys();
            label += " p_{toy}=(";
            label += (std::isnan(pNull.first)) ? "-" : TString::Format("%.4f +/- %.4f",pNull.first,pNull.second);
            label += (std::isnan(pAlt.first)) ? ",-" : TString::Format("%.4f +/- %.4f",pAlt.first,pAlt.second);
            label += (std::isnan(pCLs.first)) ? ",-)" : TString::Format("%.4f +/- %.4f",pCLs.first,pCLs.second);
        }
        if (label.Length()>0) l->AddEntry("",label,"");
        label="";
        if (!std::isnan(pNullA) || !std::isnan(pAltA)) {
            auto pCLs = pCLs_asymp();
            label += " p_{asymp}=(";
            label += (std::isnan(pNullA)) ? "-" : TString::Format("%g",pNullA);
            label += (std::isnan(pAltA)) ? ",-" : TString::Format(",%g",pAltA);
            label += (std::isnan(pCLs)) ? ",-)" : TString::Format(",%g)",pCLs);
        }
        if (label.Length()>0) l->AddEntry("",label,"");

    //}
}

const char* xRooNLLVar::xRooHypoPoint::fPOIName() { return (poi().first())->GetName(); }
double xRooNLLVar::xRooHypoPoint::fNullVal() { return dynamic_cast<RooAbsReal*>(poi().first())->getVal(); }
double xRooNLLVar::xRooHypoPoint::fAltVal()  { return dynamic_cast<RooAbsReal*>(alt_poi().first())->getVal(); }

xRooNLLVar::xRooHypoSpace xRooNLLVar::hypoSpace(const char* parName, int nPoints, double low, double high, double alt_value, const xRooFit::Asymptotics::PLLType& pllType) {
    xRooNLLVar::xRooHypoSpace s(parName,parName);
    if (nPoints==1) {
        return s;
    }
    for(double i = low; i<=high; i+= (high-low)/(nPoints-1)) {
        s.fPoints.emplace_back(hypoPoint(parName,i,alt_value,pllType));
    }
    // make all hypoPoints use the same NLLVar instance
    for(auto& p : s.fPoints) p.nllVar = s.fPoints.front().nllVar;
    return s;
}

RooArgList xRooNLLVar::xRooHypoSpace::poi() {
    RooArgList out;
    if(!fPoints.empty()) {
        out.add(*std::unique_ptr<RooAbsCollection>(fPoints.front().nllVar->pars()->selectCommon(fPoints.front().poi())));
    }
    return out;
}

#include "TKey.h"
#include "TFile.h"

void xRooNLLVar::xRooHypoSpace::LoadFits(const char* apath) {

    if (!gDirectory) return;
    auto dir = gDirectory->GetDirectory(apath);
    if (!dir) {
        // try open file first
        TString s(apath);
        if (s.Contains(":")) {
            auto f = TFile::Open(TString(s(0,s.Index(":"))));
            if(f) dir = gDirectory->GetDirectory(apath);
        }
        if(!dir) {
            Error("LoadFits","Path not found %s",apath);
            return;
        }
    }

    // assume for now all fits in given dir will have the same pars
    // so can just look at the float and const pars of first fit result to get all of them
    // tuple is: parName, parValue, parAltValue (blank if nan)
    std::map<std::set<std::tuple<std::string,double,std::string>>,std::set<std::set<std::string>>> cfits;
    std::set<std::string> allpois;

    int nFits = 0;
    std::function<void(TDirectory*)> processDir;
    processDir = [&](TDirectory* dir) {
        std::cout << "Processing " << dir->GetName() << std::endl;
        if (auto keys = dir->GetListOfKeys(); keys) {
            for (auto &&k: *keys) {
                if(auto subdir = dir->GetDirectory(k->GetName()); subdir) {
                    processDir(subdir); continue;
                }
                auto cl = TClass::GetClass(((TKey *) k)->GetClassName());
                if (cl->InheritsFrom("RooFitResult")) {
                    if (auto cachedFit = dir->Get<RooFitResult>(k->GetName());cachedFit) {
                        nFits++;
                        if (!fPars) {
                            fPars = std::make_shared<RooArgSet>();
                            fPars->addClone(cachedFit->floatParsFinal()); // constPars added below to limit to real pars
                        }
                        // build a set of the const par values
                        std::set<std::tuple<std::string, double,std::string>> constPars;
                        for (auto &p: cachedFit->constPars()) {
                            if(p->getAttribute("global")) continue; // don't consider globals when looking for cfits
                            auto v = dynamic_cast<RooAbsReal *>(p);
                            if (!v) { continue; };
                            constPars.insert(std::make_tuple(v->GetName(), v->getVal(), v->getStringAttribute("altHypo") ? v->getStringAttribute("altHypo") : ""));
                            if (!fPars->contains(*v)) fPars->addClone(*v);
                        }
                        // now see if this is a subset of any existing cfit ... if not we will add it as a cfit
                        bool isufit = false;
                        for (auto&&[key, value]: cfits) {
                            if (std::includes(key.begin(), key.end(), constPars.begin(), constPars.end())) {
                                // this fr is a ufit of the cfit
                                // add all par names of key that aren't in constPars
                                std::set<std::string> pois;
                                for (auto &&par: key) {
                                    if (constPars.find(par) == constPars.end()) {
                                        pois.insert(std::get<0>(par));
                                        allpois.insert(std::get<0>(par));
                                    }
                                }
                                if (!pois.empty()) {
                                    value.insert(pois);
                                    isufit = true;
                                }
                            } else if (std::includes(constPars.begin(), constPars.end(), key.begin(), key.end())) {
                                // cfit is actually a ufit of this fr ...
                                std::set<std::string> pois;
                                for (auto &&par: constPars) {
                                    if (key.find(par) == key.end()) {
                                        pois.insert(std::get<0>(par));
                                        allpois.insert(std::get<0>(par));
                                    }
                                }
                                if (!pois.empty()) {
                                    cfits[constPars].insert(pois);
                                    isufit = true;
//                                    std::cout << cachedFit->GetName() << " ";
//                                    for(auto ff: constPars) std::cout << ff.first << "=" << ff.second << " ";
//                                    std::cout << std::endl;
                                }
                            }
                        }
                        if (!isufit) {
                            cfits[constPars];
                        }
                        delete cachedFit;
                    }
                }
            }
        }
    };
    processDir(dir);
    Info("xRooHypoSpace","Loaded %d fits",nFits);


    if(allpois.size()==1) {
        Info("xRooHypoSpace","Detected POI: %s",allpois.begin()->c_str());

        auto nll = std::make_shared<xRooNLLVar>(nullptr, nullptr);
        auto dummyNll = std::make_shared<RooRealVar>(apath, "Dummy NLL", 1);
        nll->std::shared_ptr<RooAbsReal>::operator=(dummyNll);
        dummyNll->setAttribute("readOnly");
        // add pars as 'servers' on the dummy NLL
        if (fPars) {
            for (auto &&p: *fPars) {
                dummyNll->addServer(*p); // this is ok provided fPars (i.e. hypoSpace) stays alive as long as the hypoPoint ...
            }
        }
        nll->reinitialize(); // triggers filling of par lists etc

        for(auto&& [key,value] : cfits) {
            if(value.find(allpois) != value.end()) {
                // get the value of the poi in the key set
                auto _coords = std::make_shared<RooArgSet>();
                for(auto& k : key) {
                    auto v = _coords->addClone(RooRealVar(std::get<0>(k).c_str(), std::get<0>(k).c_str(), std::get<1>(k)));
                    v->setAttribute("poi",allpois.find(std::get<0>(k)) != allpois.end());
                    if(!std::get<2>(k).empty())  {
                        v->setStringAttribute("altHypo",std::get<2>(k).c_str());
                    }
                }
                xRooNLLVar::xRooHypoPoint hp;
                //hp.fPOIName = allpois.begin()->c_str();
                //hp.fNullVal = _coords->getRealValue(hp.fPOIName.c_str());
                hp.coords = _coords;
                hp.nllVar = nll;

//                auto altVal = hp.null_cfit()->constPars().find(hp.fPOIName.c_str())->getStringAttribute("altHypo");
//                if(altVal) hp.fAltVal = TString(altVal).Atof();
//                else hp.fAltVal = std::numeric_limits<double>::quiet_NaN();

                // decide based on values
                if (std::isnan(hp.fAltVal())) hp.fPllType = xRooFit::Asymptotics::TwoSided;
                else if(hp.fNullVal() >= hp.fAltVal()) hp.fPllType = xRooFit::Asymptotics::OneSidedPositive;
                else hp.fPllType = xRooFit::Asymptotics::Uncapped;

                fPoints.emplace_back(hp);
            }
        }
    } else {
        for(auto p : allpois) std::cout << "possible POI: " << p << std::endl;
    }
}

#include "TGraphErrors.h"

void xRooNLLVar::xRooHypoSpace::Draw(Option_t* opt) {

    TString sOpt(opt);

    if (poi().empty()) return;

    TGraphErrors* out = new TGraphErrors;
    out->SetName(GetName());
    bool doCLs = true;
    const char* sCL = (doCLs) ? "CLs" : "null";

    std::vector<int> expSig = {-2,-1,0,1,2,999};
    std::map<int,TGraphErrors*> exp_pcls;//,exp_cls;
    for(auto& s : expSig) {
        exp_pcls[s] = new TGraphErrors;
        if(s==999)  exp_pcls[s]->SetNameTitle(TString::Format("obs_p%s",sCL),TString::Format("Observed p_{%s};%s;p-value",sCL,poi().first()->GetTitle()));
        else exp_pcls[s]->SetNameTitle(TString::Format("exp%d_p%s",s,sCL),TString::Format("Expected (%d#sigma) p_{%s};%s",s,sCL,poi().first()->GetTitle()));
        //exp_cls[s].SetNameTitle(TString::Format("exp%d_%s",s,sCL),TString::Format("Expected (%d#sigma) %s;%s",s,sCL,mu->GetTitle()));
    }
    exp_pcls[0]->SetLineStyle(2);exp_pcls[0]->SetMarkerStyle(0);

    std::map<int,std::tuple<TGraph*,TGraph*,TGraph*>> exp_pbands;
    for(auto& s : {1,2}) {
        std::get<0>(exp_pbands[s]) = new TGraph; std::get<0>(exp_pbands[s])->SetNameTitle(TString::Format("exp_band%d_p%s",s,sCL),TString::Format(";%s;%s p-value",poi().first()->GetTitle(),sCL));
        std::get<1>(exp_pbands[s]) = new TGraph; std::get<1>(exp_pbands[s])->SetNameTitle(".pCLs_2sigma_upUncert","");
        std::get<2>(exp_pbands[s]) = new TGraph; std::get<2>(exp_pbands[s])->SetNameTitle(".pCLs_2sigma_downUncert","");
        std::get<0>(exp_pbands[s])->SetFillColor((s==2) ? kYellow : kGreen);
        std::get<1>(exp_pbands[s])->SetFillColor((s==2) ? kYellow : kGreen);
        std::get<2>(exp_pbands[s])->SetFillColor((s==2) ? kYellow : kGreen);
        std::get<1>(exp_pbands[s])->SetFillStyle(3005);std::get<2>(exp_pbands[s])->SetFillStyle(3005);
    }
    auto updateBands = [&]() {
        for(auto& s : {1,2}) {
            std::get<0>(exp_pbands[s])->Set(0);std::get<1>(exp_pbands[s])->Set(0);std::get<2>(exp_pbands[s])->Set(0);
            for(int i=0;i<exp_pcls[s]->GetN();i++) {
                std::get<0>(exp_pbands[s])->SetPoint(std::get<0>(exp_pbands[s])->GetN(),exp_pcls[s]->GetPointX(i),exp_pcls[s]->GetPointY(i) - exp_pcls[s]->GetErrorYlow(i));
                std::get<1>(exp_pbands[s])->SetPoint(std::get<1>(exp_pbands[s])->GetN(),exp_pcls[s]->GetPointX(i),exp_pcls[s]->GetPointY(i) + exp_pcls[s]->GetErrorYhigh(i));
            }
            for(int i=exp_pcls[s]->GetN()-1;i>=0;i--) {
                std::get<1>(exp_pbands[s])->SetPoint(std::get<1>(exp_pbands[s])->GetN(),exp_pcls[s]->GetPointX(i),exp_pcls[s]->GetPointY(i) - exp_pcls[s]->GetErrorYlow(i));
            }
            for(int i=0;i<exp_pcls[-s]->GetN();i++) {
                std::get<2>(exp_pbands[s])->SetPoint(std::get<2>(exp_pbands[s])->GetN(),exp_pcls[-s]->GetPointX(i),exp_pcls[-s]->GetPointY(i) + exp_pcls[-s]->GetErrorYhigh(i));
            }
            for(int i=exp_pcls[-s]->GetN()-1;i>=0;i--) {
                std::get<0>(exp_pbands[s])->SetPoint(std::get<0>(exp_pbands[s])->GetN(),exp_pcls[-s]->GetPointX(i),exp_pcls[-s]->GetPointY(i) + exp_pcls[-s]->GetErrorYhigh(i));
                std::get<2>(exp_pbands[s])->SetPoint(std::get<2>(exp_pbands[s])->GetN(),exp_pcls[-s]->GetPointX(i),exp_pcls[-s]->GetPointY(i) - exp_pcls[-s]->GetErrorYlow(i));
            }
        }
    };

    TString title = TString::Format(";%s", poi().first()->GetTitle());

    auto pllType = xRooFit::Asymptotics::TwoSided;
    if (!fPoints.empty() && poi().size()==1) {
        auto v = dynamic_cast<RooRealVar*>(poi().first());
        for(auto& p : fPoints) {
            if (p.fPllType != xRooFit::Asymptotics::TwoSided) {
                pllType = p.fPllType;
            }
        }
        if(pllType == xRooFit::Asymptotics::OneSidedPositive) {
            if (v && v->hasRange("physical") && v->getMin("physical") != -std::numeric_limits<double>::infinity()) title += TString::Format(";Lower-Bound One-Sided Limit PLR");
            else if(v) title += TString::Format(";One-Sided Limit PLR");
            else title += ";q";
        } else if(pllType == xRooFit::Asymptotics::TwoSided) {
            if (v && v->hasRange("physical") && v->getMin("physical") != -std::numeric_limits<double>::infinity()) title += TString::Format(";Lower-Bound PLR");
            else if(v) title += TString::Format(";PLR");
            else title += ";t";
        } else if(pllType == xRooFit::Asymptotics::OneSidedNegative) {
            if (v && v->hasRange("physical") && v->getMin("physical") != -std::numeric_limits<double>::infinity()) title += TString::Format(";Lower-Bound One-Sided Discovery PLR");
            else if(v) title += TString::Format(";One-Sided Discovery PLR");
            else title += ";r";
        } else if(pllType == xRooFit::Asymptotics::Uncapped) {
            if (v && v->hasRange("physical") && v->getMin("physical") != -std::numeric_limits<double>::infinity()) title += TString::Format(";Lower-Bound Uncapped PLR");
            else if(v) title += TString::Format(";Uncapped PLR");
            else title += ";s";
        } else {
            title += ";Test Statistic";
        }
    }

    out->SetTitle(title);
    *dynamic_cast<TAttFill*>(out) = *this;
    *dynamic_cast<TAttLine*>(out) = *this;
    *dynamic_cast<TAttMarker*>(out) = *this;
    out->SetBit(kCanDelete);

    if(!gPad) TCanvas::MakeDefCanvas();
    auto basePad = gPad;
    if (!sOpt.Contains("same")) basePad->Clear();
    gPad->Divide(1,2);
    gPad->cd(1);gPad->SetBottomMargin(gPad->GetBottomMargin()*2.); // increase margin to be same as before
    out->SetEditable(false);
    out->Draw(sOpt);
    basePad->cd(2);

    TGraph* badPoints = nullptr;

    TStopwatch s; s.Start();
    std::shared_ptr<const RooFitResult> ufr;
    for(auto& p : fPoints) {
        if(p.fPllType != pllType) continue; // must all have same pll type
        auto val = p.pll().first;
        if(!ufr) ufr = p.ufit();
        if(auto fr = p.fNull_cfit; fr) { // access member to avoid unnecessarily creating fit result if wasnt needed
            // create a new subpad and draw fitResult on it
            auto _pad = gPad;
            auto pad = new TPad(fr->GetName(),TString::Format("%s = %g",poi().first()->GetTitle(),p.fNullVal()),0,0,1.,1);
            pad->SetNumber(out->GetN()+1); // can't use "0" for a subpad
            pad->cd();
            xRooNode(fr).Draw();
            _pad->cd();
            //_pad->GetListOfPrimitives()->AddFirst(pad);
            pad->AppendPad();
        }
        if (std::isnan(val)) {
            if (!badPoints) {
                badPoints = new TGraph;
                badPoints->SetBit(kCanDelete); badPoints->SetName("badPoints");
                auto _pad = gPad;
                basePad->GetPad(1)->cd();
                badPoints->Draw("P");
                _pad->cd();
                badPoints->SetMarkerStyle(5); badPoints->SetMarkerColor(kRed); badPoints->SetMarkerSize(1);
            }
            badPoints->SetPoint(badPoints->GetN(),p.fNullVal(),0);
            basePad->GetPad(1)->Modified();
        } else{
            if (badPoints && out->GetN()) {
                // can now position the marker on the line ...
                badPoints->SetPointY(badPoints->GetN()-1,(out->GetPointY(out->GetN()-1)+val)/2.);
            }
            out->SetPoint(out->GetN(), p.fNullVal(), p.pll().first );
            out->SetPointError(out->GetN()-1,0,p.pll().second);
            basePad->GetPad(1)->Modified();
        }

        if(!std::isnan(p.fAltVal())) {
            // calculate p-values ...

            for(auto nSig : expSig) {
                auto pval = (doCLs) ? p.pCLs_asymp(nSig==999 ? std::numeric_limits<double>::quiet_NaN() : double(nSig)) : p.pNull_asymp(nSig==999 ? std::numeric_limits<double>::quiet_NaN() : double(nSig));
                if (!std::isnan(pval)) {
                    auto _pad = gPad;
                    bool isFirst = !(basePad->GetPad(3));
                    if (isFirst) {
                        basePad->cd();
                        basePad->GetPad(1)->SetBottomMargin(basePad->GetPad(1)->GetBottomMargin() * 2);
                        auto newPad = new TPad("pvalues", "pvalues", basePad->GetPad(1)->GetXlowNDC(),
                                               basePad->GetPad(1)->GetYlowNDC(),
                                               basePad->GetPad(1)->GetXlowNDC() + basePad->GetPad(1)->GetWNDC(),
                                               basePad->GetPad(1)->GetYlowNDC() +
                                               basePad->GetPad(1)->GetHNDC() / 2.);
                        newPad->SetBottomMargin(basePad->GetPad(1)->GetBottomMargin());
                        newPad->SetNumber(3);
                        newPad->Draw();
                        basePad->GetPad(1)->SetPad(basePad->GetPad(1)->GetXlowNDC(),
                                                   basePad->GetPad(1)->GetYlowNDC() +
                                                   basePad->GetPad(1)->GetHNDC() / 2.,
                                                   basePad->GetPad(1)->GetXlowNDC() + basePad->GetPad(1)->GetWNDC(),
                                                   basePad->GetPad(1)->GetYlowNDC() +
                                                   basePad->GetPad(1)->GetHNDC());
                        newPad->cd();

                        for(auto nSig2 : expSig) {
                            TString drawType = (isFirst) ? "A" : "";
                            if (nSig2 == 0 || nSig2 == 999) drawType += "LP";
                            else drawType += "F"; // bands

                            if (nSig2 == 0 || nSig2 == 999) {
                                exp_pcls[nSig2]->SetBit(kCanDelete);
                                exp_pcls[nSig2]->Draw(drawType);
                            } else if (nSig2 < 0) { // only draw band once
                                std::get<0>(exp_pbands[-nSig2])->SetBit(kCanDelete);
                                std::get<0>(exp_pbands[-nSig2])->Draw(drawType);
                            }
                            isFirst=false;
                        }
                        _pad->cd();
                    }

                    exp_pcls[nSig]->SetPoint(exp_pcls[nSig]->GetN(), p.fNullVal(), pval);
                    if(nSig!=0 && nSig!=999) updateBands();

                    basePad->GetPad(3)->Modified();
                }

            }
        }
        if (s.RealTime() > 3) { // stops the clock
            basePad->Update();gSystem->ProcessEvents();
            s.Reset();s.Start();
        }
        s.Continue();
    }

    for(auto& [_,g] : exp_pcls) if(!g->TestBit(kCanDelete)) delete g;


    // finish by overlaying ufit
    if(ufr) {
        auto _pad = gPad;
        auto pad = new TPad(ufr->GetName(), "unconditional fit", 0, 0, 1., 1.);
        pad->SetNumber(-1);
        pad->cd();
        xRooNode(ufr).Draw();
        _pad->cd();
        pad->AppendPad();
    }

    // draw one more pad to represent the selected, and draw the ufit pad onto that pad
    auto pad = new TPad("selected","selected",0,0,1,1);
    pad->Draw();
    if(ufr) {
        pad->cd();
        basePad->GetPad(2)->GetPad(-1)->AppendPad();
        pad->Modified();pad->Update();gSystem->ProcessEvents();
    }
    basePad->cd();


    if (!xRooNode::gIntObj) { xRooNode::gIntObj = new xRooNode::InteractiveObject; }
    gPad->GetCanvas()->Connect("Highlighted(TVirtualPad*,TObject*,Int_t,Int_t)","xRooNode::InteractiveObject",xRooNode::gIntObj,"Interactive_PLLPlot(TVirtualPad*,TObject*,Int_t,Int_t)");

    return;

}

