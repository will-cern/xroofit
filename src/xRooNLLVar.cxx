
#define protected public
#include "RooFitResult.h"
#undef protected

#include "xRooFit/xRooFit.h"

#include "RooCmdArg.h"
#include "RooAbsPdf.h"
#include "RooAbsData.h"
#include "RooNLLVar.h"
#include "RooConstraintSum.h"
#include "RooSimultaneous.h"
#include "RooAbsCategoryLValue.h"
#include "TPRegexp.h"

#include "RooRealVar.h"



xRooNLLVar::~xRooNLLVar() {

}

xRooNLLVar::xRooNLLVar(const std::shared_ptr<RooAbsPdf>& pdf, const std::shared_ptr<RooAbsData>& data, const RooLinkedList& opts) :
    fPdf(pdf), fData(data) {

    fOpts = std::shared_ptr<RooLinkedList>(new RooLinkedList,[](RooLinkedList* l) { if(l) l->Delete(); delete l; } );

    for(int i=0; i< opts.GetSize(); i++) {
        fOpts->Add( opts.At(i)->Clone(nullptr) ); //nullptr needed because accessing Clone via TObject base class puts "" instead, so doesnt copy names
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

    if (auto globs = dynamic_cast<RooCmdArg*>(fOpts->find("GlobalObservables"))) {
        // first remove any obs the pdf doesnt depend on
        auto _vars = std::unique_ptr<RooAbsCollection>( fPdf->getVariables() );
        auto _funcGlobs = std::unique_ptr<RooAbsCollection>(_vars->selectCommon(*globs->getSet(0)));
        fGlobs.reset( std::unique_ptr<RooAbsCollection>(globs->getSet(0)->selectCommon(*_funcGlobs))->snapshot() );
        globs->setSet(0,dynamic_cast<RooArgSet&>(*fGlobs)); // use fGlobs because will stay alive as long as the linked list
        /*RooArgSet toRemove;
        for(auto a : *globs->getSet(0)) {
            if (!_vars->find(*a)) toRemove.add(*a);
        }
        const_cast<RooArgSet*>(globs->getSet(0))->remove(toRemove);
        fGlobs.reset( globs->getSet(0)->snapshot() );
        fGlobs->setAttribAll("Constant",true);
        const_cast<RooArgSet*>(globs->getSet(0))->replace(*fGlobs);*/
    }

}

struct cout_redirect {
    cout_redirect(std::string& _out) : out(_out) { old = std::cout.rdbuf(buffer.rdbuf()); old2 = std::cerr.rdbuf(buffer.rdbuf());
        old3 = stdout;
        fp = fmemopen(buffer2,1024*1024,"w");
        stdout = fp;
    }
    ~cout_redirect( ) { std::cout.rdbuf( old ); std::cerr.rdbuf(old2); std::fclose(fp);stdout = old3;
        out = buffer.str(); out += buffer2;
    }
private:
    std::streambuf * old, *old2;
    std::stringstream buffer;
    char buffer2[1024*1024];
    FILE* fp;
    FILE* old3;
    std::string& out;
};

void xRooNLLVar::reinitialize() {
    {
        cout_redirect c(fFuncCreationLog);
        this->reset( fPdf->createNLL(*fData,*fOpts) );
    }

    fFuncVars.reset( std::shared_ptr<RooAbsReal>::get()->getVariables() );
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
    return xRooFit::minimize(*get(),(_config) ? _config : fitConfig());
}

class AutoRestorer {
public:
    AutoRestorer(const RooAbsCollection& s) : fSnap(s.snapshot()) { fPars.add(s); }
    ~AutoRestorer() { ((RooAbsCollection&)fPars) = *fSnap; }
    RooArgSet fPars;
    std::unique_ptr<RooAbsCollection> fSnap;
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

Bool_t xRooNLLVar::setData(const std::pair<std::shared_ptr<RooAbsData>,std::shared_ptr<const RooAbsCollection>>& _data) {

    if (fGlobs) {
        if (!_data.second) throw std::runtime_error("Missing globs");
        if (!fGlobs->equals(*_data.second)) throw std::runtime_error("globs mismatch");
        *fGlobs = *_data.second;
    }

    auto out = nllTerm()->setData(*_data.first, false /* clone data? */);
    fData = _data.first;
    return out;
}

std::shared_ptr<RooAbsReal> xRooNLLVar::func() const {
    if (!(*this)) {
        const_cast<xRooNLLVar*>(this)->reinitialize();
    } else if (!std::unique_ptr<RooAbsCollection>(fConstVars->selectByAttrib("Constant",false))->empty()) {
        std::cout << "Reinitializing because of change of const parameters" << std::endl;
        const_cast<xRooNLLVar*>(this)->reinitialize();
    }
    if (fGlobs) *fFuncVars = *fGlobs;
    return *this;
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


