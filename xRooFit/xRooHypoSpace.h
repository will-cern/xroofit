//
// Created by Will Buttinger on 12/08/2021.
//

#pragma once

#include "xRooFit.h"

class xRooHypoSpace;

class xRooHypoPoint : public TObject {

public:
    xRooHypoPoint() {}
    xRooHypoPoint(xRooHypoSpace* space, const RooAbsCollection& coords, const RooAbsCollection& alt_coords);
    xRooHypoSpace* fSpace = nullptr;


    void Draw(Option_t* opt = "") override;

    double GetObs(const char* name=nullptr); // if null will use space obs name
    double GetObsError(const char* name=nullptr);
    double GetExp(double nSigma=0,bool asymptotics=false); // return expected test stat value based on alt dist

    void FillNull(double value, double w=1.);
    void FillAlt(double value, double w=1.);
    void SetObs(double value, const char* name = nullptr);

    bool HasAsymptotics() const;

    size_t GetNullEntries() const { return fNull.size(); }
    size_t GetAltEntries() const { return (fAltPoint && fAltPoint!=this) ? fAltPoint->GetNullEntries() : 0; }

    double GetPCLs(double value, bool asymptotic=false) { double out= GetPNull(value,asymptotic); if(out==0) return out; return out / GetPAlt(value,asymptotic); }
    double GetPAlt(double value, bool asymptotic=false);
    double GetPNull(double value, bool asymptotic=false);

    std::pair<double,double> GetSigmaMu();

    xRooFit::Asymptotics::PLLType fPllType = xRooFit::Asymptotics::Unknown;

    const RooAbsCollection* fCoords = nullptr;
    const RooAbsCollection* fPOI = nullptr; // values of the parameters of interest used for defining test stat

    xRooHypoPoint* fAltPoint = nullptr; // same pllType and fPOI (so same test statistic) but different coords

    std::vector<std::pair<double,double>> fNull;
    std::map<std::string, std::pair<double,double>> fObs; // named observed values - may include asimovData

    ClassDefOverride(xRooHypoPoint,1)
};

class xRooHypoSpace : public TNamed {

public:

    

    xRooHypoSpace();
    xRooHypoSpace(const char* name, const char* title, const RooAbsCollection& poi);

    void Draw(Option_t* opt="") override;

    xRooHypoPoint* GetPoint(const std::string& coords, const std::string& alt_coords="");


    std::vector<xRooHypoPoint*> fPoints;
    std::string fObsName = "obs"; // name of the observed value in points

    const RooAbsCollection* fPOI = nullptr;

    ClassDefOverride(xRooHypoSpace,1)

};
