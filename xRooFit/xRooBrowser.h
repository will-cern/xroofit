#pragma once

#include "xRooFit/xRooNode.h"

#include "TBrowser.h"
class xRooBrowser: public TBrowser {
public:
    xRooBrowser();

    xRooNode* GetSelected() { return dynamic_cast<xRooNode*>(TBrowser::GetSelected()); }

    void ls(const char* path = nullptr) {
        if (!fNode) return;
        if (!path) fNode->Print();
        else {
            // will throw exception if not found
            fNode->at(path)->Print();
        }
    }

    void cd(const char* path) {
        auto _node = fNode->at(path); // throws exception if not found
        fNode = _node;
    }

private:
    std::shared_ptr<xRooNode> fNode; //!


ClassDefOverride(TBrowser,0)

};