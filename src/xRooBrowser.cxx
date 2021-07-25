
#include "xRooFit/xRooBrowser.h"

#include "TSystem.h"
#include "TKey.h"
#include "TROOT.h"
#include "TEnv.h"
#include "TFile.h"
#include "RooWorkspace.h"

xRooBrowser::xRooBrowser() : TBrowser("RooBrowser", []() {
    gEnv->SetValue("X11.UseXft","no"); // for faster x11
    return new xRooNode("Workspaces"); }(), "RooFit Browser") {

    fNode = std::shared_ptr<xRooNode>(dynamic_cast<xRooNode*>(GetSelected()),[](xRooNode*){});

    if (fNode) {
        for (auto file : *gROOT->GetListOfFiles()) {
            auto _file = dynamic_cast<TFile *>(file);
            auto keys = _file->GetListOfKeys();
            if (keys) {
                for (auto &&k : *keys) {
                    auto cl = TClass::GetClass(((TKey *) k)->GetClassName());
                    if (cl == RooWorkspace::Class() || cl->InheritsFrom("RooWorkspace")) {
                        if (auto w = _file->Get<RooWorkspace>(k->GetName()); w) {
                            if (!fNode->contains(_file->GetName())) {
                                fNode->emplace_back(std::make_shared<xRooNode>(*_file));
                            }
                            if (!fNode->at(_file->GetName())->contains(w->GetName())) {
                                fNode->at(_file->GetName())->emplace_back(
                                        std::make_shared<xRooNode>(*w, fNode->at(_file->GetName())));
                            }
                        }
                    }
                }
            }
        }
    }
}