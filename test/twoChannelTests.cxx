

#include "xRooFit/xRooFit.h"
#include "gtest/gtest.h"

#include "TH1D.h"
#include "TFile.h"


TEST(twoChannelTests,test1) {

    // create a two-channel model


    xRooNode w("RooWorkspace","w","w");
    auto model = w.Add("simPdf","model"); // creates a model

    TFile f("mjj.root");


    model.Add("SR","channel"); //add a channel
    model.Add("CRA","channel");
    std::vector<double> bins = {1000, 1450, 1800, 2300, 5300};

    // use same binning in all channels, and add 3 samples to each channel
    for(auto& c : model) {
        c->SetXaxis("m_jj",bins.size()-1,&bins[0]);
        // add samples
        c->Add("qcd","sample").SetTitle("QCD_Mgraph");
        c->Add("signal","sample").SetTitle("Signal");
        c->at("signal")->SetContents(*f.Get<TH1>(TString::Format("Signal_%s",c->GetName())));
        c->Add("nonwy","sample").SetTitle("NonW#gamma");
        c->at("nonwy")->SetContents(*f.Get<TH1>(TString::Format("nonwy_%s",c->GetName())));
        auto hData = f.Get<TH1>(TString::Format("asimov_%s",c->GetName()));
        for(int i=1;i<=hData->GetNbinsX();i++) c->SetBinData(i,hData->GetBinContent(i));
    }

    // create a shapeFactor (has a floating variable for each bin)
    // shape factors can be created from ROOT histograms (can set min and max values for parameters etc)
    TH1D sf("r_LWyC", "r_LWyC", bins.size(), &bins[0]);
    sf.SetMinimum(0); sf.SetMaximum(200); for(int i=1;i<=sf.GetNbinsX();i++) sf.SetBinContent(i,1);
    sf.SetOption("shape");
    w.factors().Add(sf);

    // multiply the qcd component in all channels by this factor
    for(auto& c : model) {
        c->at("qcd")->Multiply(*w.factors()["r_LWyC"]);
    }






    // create a histogram, will be used to define a 'sample' in the channel
    TH1D h("SR_strong","Strong;m_jj",10,0,5); h.SetFillColor(kGreen);
    h.SetBinContent(3,2);h.SetBinError(3,.2);

    // a channel is made up of factors. The factors are pdfs over observables
    w["myModel"]->variations()["myChannel"]->factors().Add("samples"); // creates a RooRealSumPdf that can be composed of samples

    // at this point you create histograms that you would want to add to the samples list


    // can add your samples like this
    w["myModel"]->variations()["myChannel"]->factors()["samples"]->components().Add(h);

    // samples can be modified by the application of additional factors
    // the types of factor are: norm, shape, overall, histo
    // these factor names are global in that they can be used across channels
    w["myModel"]->variations()["myChannel"]->factors()["samples"]->components()["s1"]->factors().Add("mu","norm");

    // note that when a sample is created using a histogram like above, it is itself a histo factor
    // when a factor is added to a sample, the histofactor of the sample histo factor is moved into the sample
    // and the sample becomes a 'product' of factors

    // overall and histo factors can be varied
    w["myModel"]->variations()["myChannel"]->factors()["samples"]->components()["s1"]->factors().Add("c1_s1_overall","overall");
    w["myModel"]->variations()["myChannel"]->factors()["samples"]->components()["s1"]->factors()["c1_s1_overall"]->variations()["ucs=1"]->SetContents(1.1);
    // the main sample histo factor, so can also be varied:
    w["myModel"]->variations()["myChannel"]->factors()["samples"]->components()["s1"]->factors()["s1"]->variations()["d=1"]->SetBinContent(3,3);

    // note how the syntax quickly gets verbose.
    // we can shorten things like this:
    //  components().Add( ... )   ->   .Add( ... )
    //  factors().Add( ... )      ->   .Multiply( ... )
    //  variations().Add( ... )   ->   .Vary( ... )

    // we can also shorten paths to nodes:
    //  ["x"]->a()["y"] where a() is components() or factors() or variations() can be shortened to ["x/y"]

    // Also, in many cases intermediate objects are able to be automatically created based on their context.
    // so it's possible to e.g. do: variations()["x=1"]->SetContents(1.1) in place of Vary("x=1").SetContents(1.1)


    // data can be added using the datasets() method of a channel ... create a histogram just as before
    TH1D hData("obsData","Observed Data",10,0,5); h.SetBinContent(3,2);
    w["myModel/myChannel"]->datasets().Add(hData);



}