#include "xRooFit/xRooHypoSpace.h"

#include "RooArgSet.h"
#include "RooArgList.h"
#include "RooRealVar.h"
#include "RooFitResult.h"
#include "RooConstVar.h"
#include "RooAbsPdf.h"

#include "TCanvas.h"
#include "TStopwatch.h"
#include "TSystem.h"
#include "TPRegexp.h"

//bool xRooNLLVar::xRooHypoSpace::AddWorkspace(const char* wsFilename, const char* extraPars){
//
//    auto ws = std::make_shared<xRooNode>(wsFilename);
//
//    ws->browse();
//    std::set<std::shared_ptr<xRooNode>> models;
//    for(auto n : *ws) {
//        if (n->fFolder == "!models") models.insert(n);
//    }
//    if (models.size()!=1) {
//        throw std::runtime_error("More than one model in workspace, use AddModel instead");
//    }
//
//    auto out =  AddModel(*models.begin(),extraPars);
//    if (out) {
//        fWorkspaces.insert(ws); // keep ws open
//    }
//
//}

xRooNLLVar::xRooHypoSpace::xRooHypoSpace(const char* name, const char* title) : TNamed(name,title)
    , fPars(std::make_shared<RooArgSet>()){

}

std::shared_ptr<xRooNode> xRooNLLVar::xRooHypoSpace::pdf(const char* parValues) const {
    return pdf(toArgs(parValues));
}

std::shared_ptr<xRooNode> xRooNLLVar::xRooHypoSpace::pdf(const RooAbsCollection& parValues) const {
    RooArgList rhs(parValues);
    rhs.sort();

    std::shared_ptr<xRooNode> out = nullptr;

    for(auto& [_range,_pdf] : fPdfs) {
        // any pars not in rhs are assumed to have infinite range in rhs
        // and vice versa
        bool collision=true;
        for(auto& _lhs : *_range) {
            auto _rhs = rhs.find(*_lhs);
            if (!_rhs) continue;
            if(auto v = dynamic_cast<RooRealVar*>(_rhs); v) {
                if(auto v2 = dynamic_cast<RooRealVar*>(_lhs)) {
                    if (!(v->getMin() <= v2->getMax() && v2->getMin() <= v->getMax())) {
                        collision = false;
                        break;
                    }
                } else if(auto c2 = dynamic_cast<RooConstVar*>(_lhs)) {
                    if (!(v->getMin() <= c2->getVal() && c2->getVal() <= v->getMax())) {
                        collision = false;
                        break;
                    }
                }
            } else if(auto c = dynamic_cast<RooConstVar*>(_rhs); c) {
                if(auto v2 = dynamic_cast<RooRealVar*>(_lhs)) {
                    if (!(c->getVal() <= v2->getMax() && v2->getMin() <= c->getVal())) {
                        collision = false;
                        break;
                    }
                } else if(auto c2 = dynamic_cast<RooConstVar*>(_lhs)) {
                    if (!(c->getVal() == c2->getVal())) {
                        collision = false;
                        break;
                    }
                }
            }
        }
        if(collision) {
            if(out) {
                throw std::runtime_error("Multiple pdf possibilities");
            }
            out = _pdf;
        }
    }

    return out;

}



RooArgList xRooNLLVar::xRooHypoSpace::toArgs(const char* str) {

    RooArgList out;

    TStringToken pattern(str, ";");
    while (pattern.NextToken()) {
        TString s = pattern;
        // split by "=" sign
        auto _idx = s.Index('=');
        if (_idx==-1) continue;
        TString _name = s(0,_idx);
        TString _val = s(_idx+1,s.Length());
        double val = std::numeric_limits<double>::quiet_NaN();

        if (_val.IsFloat()) {
            out.addClone(RooConstVar(_name,_name,_val.Atof()));
        } else if (_val.BeginsWith('[')) {
            _idx = _val.Index(',');
            if (_idx==-1) continue;
            TString _min = _val(0,_idx);
            TString _max = _val(_idx+1,_val.Length()-_idx-2);
            out.addClone(RooRealVar(_name,_name,_min.Atof(),_max.Atof()));
        }
    }

    return out;

}

xRooNLLVar::xRooHypoPoint& xRooNLLVar::xRooHypoSpace::AddPoint(const char* coords) {
    // move to given coords, if any
    fPars->assignValueOnly(toArgs(coords));

    auto _pdf = pdf();

    if (!_pdf) throw std::runtime_error("no model at coordinates");

    if( std::unique_ptr<RooAbsCollection>(fPars->selectByAttrib("poi",true))->size() == 0 ) {
        throw std::runtime_error("No pars designated as POI - set with pars()->find(<parName>)->setAttribute(\"poi\",true)");
    }

    if (fNlls.find(_pdf) == fNlls.end()) {
        fNlls[_pdf] = std::make_shared<xRooNLLVar>(_pdf->nll(""/*TODO:allow change dataset name and nll opts*/,{}));
    }

    xRooHypoPoint out;

    out.nllVar = fNlls[_pdf];
    out.data = fNlls[_pdf]->getData();

    out.coords.reset( fPars->snapshot() ); // should already have altHypo prop on poi, and poi labelled
    // ensure all poi are marked const ... required by xRooHypoPoint behaviour
    out.poi().setAttribAll("Constant");
    double value = out.fNullVal();
    double alt_value = out.fAltVal();

    auto _type = fTestStatType;
    if (_type == xRooFit::Asymptotics::Unknown) {
        // decide based on values
        if (std::isnan(alt_value)) _type = xRooFit::Asymptotics::TwoSided;
        else if(value >= alt_value) _type = xRooFit::Asymptotics::OneSidedPositive;
        else _type = xRooFit::Asymptotics::Uncapped;
    }

    out.fPllType = _type;

    return emplace_back(out);

}

bool xRooNLLVar::xRooHypoSpace::AddModel(const xRooNode& _pdf, const char* validity) {

    if (!_pdf.get<RooAbsPdf>()) {
        throw std::runtime_error("Not a pdf");
    }

    auto pars = _pdf.pars().argList();

    // replace any pars with validity pars and add new pars
    auto vpars = toArgs(validity);
    pars.replace(vpars);
    vpars.remove(pars,true,true);
    pars.add(vpars);


    if(auto existing = pdf(pars)) {
        throw std::runtime_error(std::string("Clashing model: ") + existing->GetName());
    }

    auto myPars = std::shared_ptr<RooArgList>(dynamic_cast<RooArgList*>(pars.snapshot()));
    myPars->sort();

    pars.remove(*fPars,true,true);

    fPars->addClone(pars);

    fPdfs.insert(std::make_pair(myPars,std::make_shared<xRooNode>(_pdf)));

    return true;

}

RooArgList xRooNLLVar::xRooHypoSpace::poi() {
    RooArgList out;
    if(!empty()) {
        out.add(*std::unique_ptr<RooAbsCollection>(front().nllVar->pars()->selectCommon(front().poi())));
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

                emplace_back(hp);
            }
        }
    } else {
        std::cout << "possible POI: ";
        for(auto p : allpois)  std::cout << p << ",";
        std::cout << std::endl;
    }
}

#include "TGraphErrors.h"

std::shared_ptr<TGraphErrors> xRooNLLVar::xRooHypoSpace::pValues(double nSigma,bool doCLs) {
    auto out = std::make_shared<TGraphErrors>();
    out->SetName(GetName());
    const char* sCL = (doCLs) ? "CLs" : "null";

    TString title = TString::Format("%s p_{%s};%s", (std::isnan(nSigma)) ? "Observed" : TString::Format("Expected (%d#sigma)",int(nSigma)).Data() , sCL, poi().first()->GetTitle());

    auto pllType = xRooFit::Asymptotics::TwoSided;
    if (!empty() && poi().size()==1) {
        auto v = dynamic_cast<RooRealVar*>(poi().first());
        for(auto& p : *this) {
            if (p.fPllType != xRooFit::Asymptotics::TwoSided) {
                pllType = p.fPllType;
            }
        }
    }

    if(std::isnan(nSigma)) {
        out->SetNameTitle(TString::Format("obs_p%s",sCL),title);
    } else {
        out->SetNameTitle(TString::Format("exp%d_p%s",int(nSigma),sCL),title);
        out->SetLineStyle(2 + int(nSigma)); out->SetMarkerStyle(0);
    }

    for(auto& p : *this) {
        if (p.fPllType != pllType) continue; // must all have same pll type
        auto pval = (doCLs) ? p.pCLs_asymp(nSigma) : p.pNull_asymp(nSigma);
        out->SetPoint(out->GetN(),p.fNullVal(),pval.first);
        out->SetPointError(out->GetN()-1,0,pval.second);
    }

    return out;

}

std::pair<double,double> xRooNLLVar::xRooHypoSpace::GetLimit(double nSigma,bool cls, double relUncert) {
    auto gr = pValues(nSigma,cls);

    // remove any nan points
    int i=0;
    while(i < gr->GetN()) {
        if (std::isnan(gr->GetPointY(i))) gr->RemovePoint(i);
        else {
            // convert to log ....
            gr->SetPointY(i,log(gr->GetPointY(i)));
            i++;
        }
    }

    gr->Sort();

    // simple linear extrapolation to critical value ... return nan if problem
    if(gr->GetN()<2) return std::pair(std::numeric_limits<double>::quiet_NaN(),0);

    double alpha = log(0.05);

    bool above = gr->GetPointY(0) > alpha;
    for(int i=1;i<gr->GetN();i++) {
        if( (above && (gr->GetPointY(i) <= alpha)) || (!above && (gr->GetPointY(i)>=alpha)) ) {
            // found the limit ... return linearly extrapolated point
            double lim = gr->GetPointX(i-1) + (gr->GetPointX(i)-gr->GetPointX(i-1))*(alpha - gr->GetPointY(i-1))/(gr->GetPointY(i) - gr->GetPointY(i-1));
            // use points either side as error
            double err = std::max( lim - gr->GetPointX(i-1), gr->GetPointX(i) - lim );
            if (err/lim <= relUncert) return std::pair(lim,err);

            // need to evaluate another point .... choose to squeeze the error ...
            double newPoint = lim + 0.99*relUncert*lim*(((lim - gr->GetPointX(i-1)) > (gr->GetPointX(i) - lim)) ? -1. : 1.);// gr->GetPointX((lim - gr->GetPointX(i-1)) > (gr->GetPointX(i) - lim) ? i : (i-1));

            Info("GetLimit","Testing new point @ %s=%g",poi().first()->GetName(),newPoint);
            push_back(back()); // creates a copy
            back().fAsimov.reset();
            back().coords.reset( back().coords->snapshot() );
            dynamic_cast<RooRealVar*>(back().coords->find(poi().first()->GetName()))->setVal(newPoint);
            back().fNull_cfit = nullptr;
            back().altToys.clear(); back().nullToys.clear();
            gr.reset(); // to clear memory
            return GetLimit(nSigma,cls,relUncert);
        }
    }
    Error("GetLimit","Limit out of HypoSpace bounds: %g - %g",gr->GetPointX(0),gr->GetPointX(gr->GetN()-1));
    return std::pair(std::numeric_limits<double>::quiet_NaN(),0);

}

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
    if (!empty() && poi().size()==1) {
        auto v = dynamic_cast<RooRealVar*>(poi().first());
        for(auto& p : *this) {
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
    for(auto& p : *this) {
        if(p.fPllType != pllType) continue; // must all have same pll type
        auto val = p.pll().first;
        if(!ufr) ufr = p.ufit();
        if(auto fr = p.fNull_cfit; fr) { // access member to avoid unnecessarily creating fit result if wasnt needed
            // create a new subpad and draw fitResult on it
            auto _pad = gPad;
            auto pad = new TPad(fr->GetName(),TString::Format("%s = %g",poi().first()->GetTitle(),p.fNullVal()),0,0,1.,1);
            pad->SetNumber(out->GetN()+1); // can't use "0" for a subpad
            pad->cd();
            xRooNode(fr).Draw("goff");
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
            out->Sort();
            basePad->GetPad(1)->Modified();
        }

        if(!std::isnan(p.fAltVal())) {
            // calculate p-values ...

            for(auto nSig : expSig) {
                auto pval = (doCLs) ? p.pCLs_asymp(nSig==999 ? std::numeric_limits<double>::quiet_NaN() : double(nSig)) : p.pNull_asymp(nSig==999 ? std::numeric_limits<double>::quiet_NaN() : double(nSig));
                if (!std::isnan(pval.first)) {
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

                    exp_pcls[nSig]->SetPoint(exp_pcls[nSig]->GetN(), p.fNullVal(), pval.first);
                    exp_pcls[nSig]->SetPointError(exp_pcls[nSig]->GetN()-1, 0, pval.second);
                    exp_pcls[nSig]->Sort();
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
        xRooNode(ufr).Draw("goff");
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

