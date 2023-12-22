#ifndef __CLING__
#include "xRooFit/xRooFit.h"
#include "RooConstVar.h"
#include "RooRealVar.h"
#include "gtest/gtest.h"
#include "Math/ProbFunc.h"
#include "TH1D.h"
#include "TFile.h"
#include "RooAbsData.h"
#include "RooFitResult.h"
#include "TSystem.h"
#include "RooFormulaVar.h"
#include "RooStats/HypoTestInverterResult.h"
#include <thread>
#endif

/**
 * Builds a model with a single channel
 * The bkg has only an "mc stat" uncertainty on it which is poisson constrained
 * The sig has a gaussian uncert of size sig_uncert along with a lumi_uncert
 * @param bkg
 * @param bkg_uncert
 * @param sig
 * @param sig_uncert
 * @param lumi_uncert
 * @return
 */

void altOneChannel() {

    // a workspace holds models and datasets
    xRooNode w("RooWorkspace","w","w");

    w.Add("myModel","model"); // creates a model
    w["myModel"]->variations().Add("myChannel"); // a channel is a 'variation' of a model
    // a channel is made up of factors. The factors are pdfs over observables
    w["myModel"]->variations()["myChannel"]->factors().Add("samples"); // creates a RooRealSumPdf that can be composed of samples

    // at this point you create histograms that you would want to add to the samples list
    TH1D h("s1","sample #1",10,0,5); h.SetFillColor(kGreen);
    h.SetBinContent(3,2);h.SetBinError(3,.2);

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
    w["myModel"]->variations()["myChannel"]->factors()["samples"]->components()["s1"]->factors()["c1_s1_overall"]->variations()["ucs=1"]->SetContent(1.1);
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

xRooNode buildModel(double data, double bkg, double bkg_uncert, double sig, double sig_uncert_up, double sig_uncert_down, double lumi_uncert) {
    xRooNode w("RooWorkspace","w","w");

    w["simPdf/channel1"]->SetXaxis("obs",1,0,1); // 1-bin channel
    w["simPdf/channel1/bkg"]->SetBinContent(1,bkg);
    //w["simPdf/channel1/bkg"]->get<RooAbsArg>()->setStringAttribute("statPrefix","myPrefix"); // example of how to give a sample its own independent stat factors
    if(bkg_uncert>0) w["simPdf/channel1/bkg"]->SetBinError(1,bkg_uncert); // creates a special shape factor called 'statFactor' which may share pars with other samples

    w["simPdf/channel1/sig"]->SetBinContent(1,sig);
    // types of factor are: norm, overall, shape, histo
    if(sig_uncert_up > 0 || sig_uncert_down > 0) {
        w["simPdf/channel1/sig"]->Multiply("c1_sig_overall", "overall"); // add an overall factor that we will vary
        w["simPdf/channel1/sig/c1_sig_overall"]->SetContent(sig_uncert_up, "ucs", 1);
        w["simPdf/channel1/sig/c1_sig_overall"]->SetContent(sig_uncert_down, "ucs", -1);
        w["simPdf"]->pars()["ucs"]->Constrain("normal"); // constrain it
    }
    if(lumi_uncert>0) {
        w["simPdf/channel1/sig"]->Multiply("lumi", "norm"); // multiply by a lumi factor .. will constrain it below
        w["simPdf"]->pars()["lumi"]->Constrain(TString::Format("gaussian(1,%f)", lumi_uncert).Data()); // lumi constraint
    }
    w["simPdf/channel1/sig"]->Multiply("mu_Sig","norm");

    // set the data
    w["simPdf/channel1"]->SetBinData(1,data);

    // adjust the ranges of the parameter of interest - relevant in hypothesis testing
    w.pars()["mu_Sig"]->get<RooRealVar>()->setRange(-0.01,100); // allow slightly less than 0 as possible fit, so 0 is not on the boundary of fit range
    w.pars()["mu_Sig"]->get<RooRealVar>()->setRange("physical",0,100); // but specify a physical range: used for asymptotic formulae etc
    w.pars()["mu_Sig"]->get<RooRealVar>()->setAttribute("poi");

    return w;
}


double testPoint(xRooNode w, double testValue = 1, double altValue = 0, int nToys=1500) {

    // create NLL function using simPdf model with obsData
    auto nll = w["simPdf"]->nll("obsData",{RooFit::Binned()});



    // Perform a hypothesis test of mu=testValue hypothesis using mu=altValue as alt hypothesis
    auto hypoTest = nll.hypoPoint(testValue,altValue);

    auto _pll = hypoTest.pll();
    auto _sigma_mu = hypoTest.sigma_mu();

    auto clsb_obs = hypoTest.pNull_asymp().first;
    auto clb_obs = hypoTest.pAlt_asymp().first;

    std::cout << "obs_pll = " << _pll.first << " sigma_mu = " << _sigma_mu.first << std::endl;
    std::cout << "cls_obs = " << (clsb_obs/clb_obs) << " [ clsb_obs = " << clsb_obs << " clb_obs = " << clb_obs << " ]" << std::endl;

    for(int i=-2;i<=2;i++) {
        std::cout << i << " sigma: " << hypoTest.pCLs_asymp(i).first << std::endl;
    }


    // can also generate toys to estimate
    if (nToys > 0) {
        auto obsData = nll.getData();

        TH1D hNull("null", "null", 100, 0, 2 * _pll.first);
        TH1D hAlt("alt", "alt", 100, 0, 2 * _pll.first);
        hAlt.SetLineColor(kRed);
        TGraph gPllVsN; // toy pll values vs N in toy


        w.pars()["mu_Sig"]->get<RooRealVar>()->setVal(testValue);
        w.pars()["mu_Sig"]->get<RooRealVar>()->setConstant();
        auto null_fit = nll.minimize();

        // speed up toys by disabling hesse
        nll.fitConfig()->SetParabErrors(false);

        double toy_clsb_obs = 0;
        int nToys = 1500;
        std::vector<double> toy_vals;
        toy_vals.reserve(nToys);
        for (int i = 0; i < nToys; i++) {
            auto toy = nll.generate(); //xRooFit::generateFrom(*nll.fPdf,null_fit); //nll.generate();
            nll.setData(toy);
            auto toy_pll = nll.hypoPoint("mu_Sig", testValue, altValue, xRooFit::Asymptotics::OneSidedPositive).pll();
            if (std::isnan(toy_pll.first)) std::cout << " nan null " << std::endl;
            if (toy_pll.first >= _pll.first) toy_clsb_obs++;
            toy_vals.push_back(toy_pll.first);
            hNull.Fill(toy_pll.first);
            gPllVsN.AddPoint(toy.first->sumEntries(), toy_pll.first);
        }
        toy_clsb_obs /= toy_vals.size();

        nll.setData(obsData);

        w.pars()["mu_Sig"]->get<RooRealVar>()->setVal(altValue);
        auto alt_fit = nll.minimize();


        double toy_clb_obs = 0;
        std::vector<double> toy_vals_b;
        toy_vals_b.reserve(nToys / 10);
        for (int i = 0; i < nToys / 10; i++) {
            auto toy = nll.generate(); //xRooFit::generateFrom(*nll.fPdf,alt_fit); //nll.generate();
            nll.setData(toy);
            auto toy_pll = nll.hypoPoint(testValue, altValue, xRooFit::Asymptotics::OneSidedPositive).pll();
            if (std::isnan(toy_pll.first)) std::cout << " nan alt " << std::endl;
            if (toy_pll.first >= _pll.first) toy_clb_obs++;
            toy_vals_b.push_back(toy_pll.first);
            hAlt.Fill(toy_pll.first);
            gPllVsN.AddPoint(toy.first->sumEntries(), toy_pll.first);
        }
        toy_clb_obs /= toy_vals_b.size();

        std::sort(std::begin(toy_vals_b), std::end(toy_vals_b));

        std::cout << "toy pvalues (s+b) (b) (s):" << std::endl;
        for (int i = -2; i <= 3; i++) {
            auto k = (i == 3) ? _pll.first : toy_vals_b.at(toy_vals_b.size() * ROOT::Math::gaussian_cdf_c(i));
            double pval = 0, pval_b = 0;
            for (auto &x: toy_vals) { if (x >= k) pval++; }
            for (auto &x: toy_vals_b) { if (x >= k) pval_b++; }
            pval /= toy_vals.size();
            pval_b /= toy_vals_b.size();
            if (i == 3) {
                std::cout << "Observed (pll=" << k << "): " << pval << " " << pval_b << " " << pval / pval_b
                          << std::endl;
            } else {
                std::cout << i << " sigma (pll=" << k << "): " << pval << " " << ROOT::Math::gaussian_cdf(i) << " "
                          << pval / ROOT::Math::gaussian_cdf(i) << std::endl;
            }
        }
        TFile f("tsDists.root", "recreate");
        hNull.SetDirectory(&f);
        hAlt.SetDirectory(&f);
        gPllVsN.Write("pll_vs_N");
        f.Write();
    }

    //w.SaveAs("oneChannel.root");
    return clsb_obs/clb_obs;


}

#ifndef __CLING__

TEST(iterateTest,iterateTest) {
    // purpose of this test is to test the custom iteration (xRooNodeIterator)
    // which is designed to browse the objects as they are obtained by dereferencing the iterator

    auto _w = buildModel(0,0.43,0.16,5.611,1.19266,0.807337,0.017);

    xRooNode w(*_w.ws());

    for(auto n : *w["simPdf"]) {
        ASSERT_NE(n->size(),0);
    }

}

TEST(test1,test1) {

    auto res = testPoint(buildModel(0,0.43,0.16,5.611,1.19266,0.807337,0.017),
                         1,0,0);

    //old res: 0.0019764892592501124 - got without change to ranges on POI

    // updte to 00188366 when realised "physical" range isnt propagated into floatParsFinal in FitResults
    // Sep2023: update to 0.00197646 once implemented lower-bound feature of pll test statistic (cfit_lbound)
    ASSERT_LT(res, 0.00197646 + 1e-7);
    ASSERT_GT(res, 0.00197646 - 1e-7);

}

TEST(test1,test2) {

    auto res = testPoint(buildModel(20,16,1,10,0.1,0.15,0.017),
                         1,0,0);

    //ASSERT_LT(abs(res - 0.0019764892592501124),1e-7);

}

TEST(test1,limiTest1) {

    auto model = buildModel(20,16,0,1,0,0,0);


}

TEST(test2,twoChannelTests) {
    xRooNode w("RooWorkspace","w","w");
    TH1D chan1_bkg("chan1_bkg","Background;dummyObs",1,0,1);chan1_bkg.SetBinContent(1,10);chan1_bkg.SetBinError(1,0.1);w["simPdf/chan1"]->Add(chan1_bkg);
    auto chan1_data = static_cast<TH1D*>(chan1_bkg.Clone("chan1_data")); chan1_data->SetTitle("Data");
    chan1_data->SetBinContent(1,chan1_data->GetBinContent(1)+1);
    w["simPdf/chan1"]->datasets()["obsData"]->Add(*chan1_data);
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->datasets()["obsData"]->GetBinContent(1),chan1_data->GetBinContent(1));

    TH1D chan2_bkg("chan2_bkg","Background;dummyObs",1,0,1);
    chan2_bkg.SetBinContent(1,5);chan2_bkg.SetBinError(1,0.5);
    w["simPdf/chan2"]->Add(chan2_bkg);
    auto chan2_data = static_cast<TH1D*>(chan2_bkg.Clone("chan2_data")); chan2_data->SetTitle("Data");
    chan2_data->SetBinContent(1,chan2_data->GetBinContent(1)+1);
    w["simPdf/chan2"]->datasets()["obsData"]->Add(*chan2_data);
    ASSERT_DOUBLE_EQ(w["simPdf/chan2"]->datasets()["obsData"]->GetBinContent(1),chan2_data->GetBinContent(1));


    xRooNode genDs(w["simPdf"]->reduced("chan1").nll().generate(true).first); // generate the asimov dataset for chan1
    genDs.Add(*w["simPdf"]->reduced("chan2").datasets()["obsData"]); // add the obsData of chan2

    // dataset should be asimov of chan1 with obs of chan2
    ASSERT_DOUBLE_EQ(w["simPdf/chan2"]->datasets()["obsData"]->GetBinContent(1)+w["simPdf/chan1"]->GetBinContent(1),genDs.get<RooAbsData>()->sumEntries());

    // should have the same global observables for the mc-stat errors
    ASSERT_TRUE( genDs.globs().get<RooArgList>()->equals(*w["obsData"]->globs().get<RooArgList>()));
    for(auto glob : genDs.globs()) {
        ASSERT_DOUBLE_EQ( glob->GetContent(), w["obsData"]->globs().at(glob->GetName())->GetContent() );
    }

}

TEST(test1,toyHypoTest) {

    auto model = buildModel(20,16,0,1,0,0,0);

    auto hp = model["simPdf"]->nll("obsData").hypoPoint(1,0);

    hp.addNullToys(30);

}

TEST(test1,coutCaptureTest) {
   auto model = buildModel(20,16,0,1,0,0,0);
   auto nll = model["simPdf"]->nll();
   nll.fitConfig()->MinimizerOptions().SetPrintLevel(2); // increase print level so will generate some log content
   nll.fitConfigOptions()->SetValue("LogSize", 1024); // and trigger capturing of log, otherwise don't get .log
   auto fr = nll.minimize();
   std::cout << " Captured Log: " << std::endl;
   ASSERT_NE( fr->constPars().find(".log"), nullptr );
   fr->constPars().find(".log")->Print();
}

#include "RooWorkspace.h"

TEST(test1, binnedFormulaVarTest) {

    // tests use of a RooFormulaVar inside a ParamHistFunc
    // so that the model remains 'binned'

    xRooNode w("RooWorkspace","w","w");
    w["simPdf/chan1"]->SetXaxis(3,0,3);
    for(int i=1;i<=w["simPdf/chan1"]->GetXaxis()->GetNbins();i++) w["simPdf/chan1/samp1"]->SetBinContent(i,1);

    // goal is to scale by factor: v*xaxis;
    w.Add(RooRealVar("v","v",-5,10));
    //TH1D bc("chan1_binCenters","",3,0,3);for(int i=1;i<=bc.GetNbinsX();i++) bc.SetBinContent(i,bc.GetXaxis()->GetBinCenter(i));
    //w.Add(bc); // creates a histoFactor

    w.get<RooWorkspace>()->factory("expr::myFactor('@0*@1',v,xaxis)");

    for(auto b : w["simPdf/chan1/samp1"]->bins()) b->Multiply("myFactor");
    w.SaveAs("binnedFormularVarTest.root");

}

TEST(test1, testSimpleModel) {

    RooWorkspace ws("w","w"); // create workspace
    xRooNode w(ws); // wrap it with an xRooNode to interact with

    w["simPdf/chan1"]->SetXaxis("myObs","dummy obs",5,0,5); // creates a channel with 5 uniform bins between 0 and 5
    w["simPdf/chan1/samp1"]->SetBinContent(1,3);
    w["simPdf/chan1/samp1"]->SetBinError(1,0.5);

    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->GetBinError(1),0.5);
    // verify the histograms have the right error too
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->BuildHistogram(nullptr,false,true)->GetBinError(1),0.5);
    ASSERT_DOUBLE_EQ(w["simPdf/chan1/samples"]->BuildHistogram(nullptr,false,true)->GetBinError(1),0.5);


    w["simPdf/chan1/samp2"]->SetBinContent(1,1);
    w["simPdf/chan1/samp2"]->SetBinContent(1,1.5,"alpha",1); // creates variation called alpha, assigning value 1.5 to +1sigma
    w["simPdf/chan1/samp2"]->SetBinContent(1,0.5,"alpha",-1); // -1 sigma variation (optional because is symmetric)

    w["simPdf"]->pars()["alpha"]->Constrain("normal"); // adds normal (gaussian(1,0)) constraint on alpha parameter
    w["simPdf/chan1/samp2"]->Multiply("mu","norm"); // creates a normalization factor that multiplies samp2
    w.pars()["mu"]->get<RooRealVar>()->setRange(0,10); // can access the parameter to change the range like this

    w["simPdf/chan1"]->SetBinData(1,6);

    w["simPdf/chan1"]->datasets()["obsData"]->get()->Print();

    // check the total error on the bin is what we expect (MC statistical + systematic in quadrature)
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->GetBinError(1),sqrt(0.5*0.5 + 0.5*0.5));
    // should also still be able to get the individual component errors using comma separated wildcards to select pars
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->GetBinError(1,"stat*"),0.5);
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->GetBinError(1,"alpha*"),0.5);

    // check integral and error functionality working as well
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->IntegralAndError().first,4);
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->IntegralAndError().second,sqrt(0.5*0.5 + 0.5*0.5));
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->datasets()["obsData"]->IntegralAndError().first,6);

    auto fr = w["simPdf"]->nll("obsData").minimize();
    fr->Print();

    ASSERT_NEAR(dynamic_cast<RooRealVar*>(fr->floatParsFinal().find("alpha"))->getError(),1,1e-2);

    // test creation of asimov datasets
    w.pars()["mu"]->get<RooRealVar>()->setVal(0);
    w["simPdf"]->datasets().Add("expData","asimov");
    ASSERT_DOUBLE_EQ(w["simPdf/chan1"]->GetBinData(1,"expData"),w["simPdf/chan1"]->GetBinContent(1));

    w.pars()["mu"]->get<RooRealVar>()->setVal(1);
    // check can get at fit result representing snapshot of model state
    ASSERT_DOUBLE_EQ( w["simPdf"]->datasets()["expData"]->fitResult().get<RooFitResult>()->floatParsFinal().getRealValue("mu"),0.);
    std::cout << w["simPdf"]->datasets()["expData"]->fitResult()->GetName() << " " << w["simPdf"]->datasets()["expData"]->fitResult()->GetTitle() << std::endl;

}

TEST(test1,discreteMinimizationTest) {
    // test if can minimize a model containing a categorical (discrete) parameter

    // for this dummy test will create a channel but use category as a scaling factor
    RooWorkspace _ws;
    xRooNode w(_ws);
    w["simPdf/chan1"]->SetXaxis(1,0,1);
    w["simPdf/chan1/bkg1"]->SetBinContent(1,5);
    w["simPdf/chan1/bkg1"]->SetBinError(1,0.1);
    w["simPdf/chan1/bkg2"]->SetBinContent(1,10);
    w["simPdf/chan1/bkg2"]->SetBinError(1,1);
    w["simPdf/chan1/bkg1"]->Multiply("fac1('myCat==0?1:0',myCat[a,b])","func");
    w["simPdf/chan1/bkg2"]->Multiply("fac2('myCat==0?0:1',myCat)","func");
    w["simPdf/chan1"]->SetBinData(1,12);

    auto fr = w["simPdf"]->nll("obsData").minimize();

    // expect cat index = 1 (second case) to be a better fit ...
    EXPECT_EQ( fr->floatParsFinal().getCatIndex("myCat"), 1 );

    // now change data to below 5
    w["simPdf/chan1"]->SetBinData(1,4);

    // expect cat index = 0 (first case) to be a better fit ...
    EXPECT_EQ( w["simPdf"]->nll("obsData").minimize()->floatParsFinal().getCatIndex("myCat"), 0 );


}

TEST(test1,speedTest) {

    xRooNode w("~/Downloads/hatt_SI_1L_combined_hatt_SI_1L_exp_A4001_0_model.root");
    // no longer need to specify physical range as will default to 0->inf on POI - update:April2023:brought back in need to do this, as trying to avoid tinkering with ranges on load of ws
    w.pars()["sqrt_mu"]->get<RooRealVar>()->setRange("physical",0,std::numeric_limits<double>::infinity());
    w.pars()["sqrt_mu"]->get<RooRealVar>()->setRange(-1,10);

    w.pars()["sqrt_mu"]->get<RooRealVar>()->setVal(0);

    //auto nll = w["simPdf"]->nll("asimovData",{xRooFit::ReuseNLL(false)});
    //nll.setData(nll.generate(true));


    auto nll = w["simPdf"]->nll(); // now if no dataset given will generate asimov dataset
    nll->SetName("nll_hatt_SI_1L_combined_hatt_SI_1L_exp_A4001_0_model.root");

    w["simPdf"]->pars().reduced("alpha_*,gamma_*").argList().setAttribAll("Constant",true);

    nll.fitConfig()->MinimizerOptions().SetStrategy(1);
    nll.fitConfig()->MinimizerOptions().SetTolerance(1);

    TFile f("hypoSpace400.root","RECREATE");

    //nll.pars()->find("sqrt_mu")->setStringAttribute("altVal","0");
    auto hs = nll.hypoSpace("sqrt_mu",xRooFit::Asymptotics::OneSidedPositive,0);

    auto lim = hs.findlimit("pcls exp0", 0.05);
    std::cout << lim.first << " +/- " << lim.second << std::endl;
    hs.Print();
    for(auto& hp : hs) hp.Print();

    f.Close();

    auto result = hs.result();

    // verify can reproduce limit .... 1/6/23: planning to phase out LoadFits functionality in favour of HypoTestInverterResult
    //xRooHypoSpace hs2;
    //hs2.LoadFits("hypoSpace400.root:nll_hatt_SI_1L_combined_hatt_SI_1L_exp_A4001_0_model.root");
    //hs2.Print();for(auto& hp : hs2) hp.Print();

    xRooHypoSpace hs2( result );

    auto lim2 = hs2.findlimit("pcls exp0", 0.05);
    std::cout << lim2.first << " +/- " << lim2.second << std::endl;

    ASSERT_LT(abs(lim.first-lim2.first),1e-3);
    ASSERT_LT(abs(lim.second-lim2.second),1e-3);

}

TEST(expensiveTest,fullLimitTest) {

   auto printMem = []() {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      static ProcInfo_t info;
      const float toMB = 1.f / 1024.f;
      gSystem->GetProcInfo(&info);
      printf(" res  memory = %g Mbytes\n", info.fMemResident * toMB);
      printf(" vir  memory = %g Mbytes\n", info.fMemVirtual * toMB);
   };

   xRooNode w("~/Downloads/leptoquarks_combined_leptoquarks_model.root");




   for(int i=0;i<1;i++) {
      TFile f("/tmp/fits_saved2.root","RECREATE");
      w.nll("obsData").hypoSpace().limits("cls visualize");
      f.Close();
      printMem();
   }


}

TEST(test1,tomasYields) {

   xRooNode w("~/Downloads/tomas.root");
   std::stringstream s;
   for(auto chan : *(w["simPdf"])) {
      for(auto samp : *(chan->at("samples"))) {
         s << samp->GetName() << " : " << samp->GetContent() << " +/- " << samp->GetError() << " [";
         for(auto b : samp->bins()) {
            s << b->GetContent() << ":" << b->GetError() << ",";
         }
         s << "]" << std::endl;
      }
   }

   std::string ref = "ttlight_ljets_5j3b_HT_shapes : 461.591 +/- 53.2555 [57.2567:12.0941,170.595:31.7475,124.328:14.4948,57.4211:3.09746,23.7444:2.59176,28.2458:3.42683,]\n"
                     "ttc_ljets_5j3b_HT_shapes : 151.569 +/- 15.8918 [16.4938:1.65735,54.4312:5.49183,38.2451:4.30843,23.8594:2.98895,7.8196:1.09518,10.7199:1.47873,]\n"
                     "ttb_ljets_5j3b_HT_shapes : 245.005 +/- 35.7825 [26.6461:4.10973,81.8434:12.1511,62.8618:9.38281,34.1697:5.21509,19.3249:3.13204,20.1593:3.2222,]\n"
                     "ttH_ljets_5j3b_HT_shapes : 22.3254 +/- 0.490244 [0.87798:0.0241488,6.11444:0.150628,6.14776:0.134624,4.33418:0.0895696,1.72547:0.0352729,3.12557:0.063892,]\n"
                     "ttlight_ljets_6j4b_BDT_shapes : 287.744 +/- 51.5531 [84.2107:14.3093,75.5761:13.5134,50.0541:9.47436,33.8633:6.64575,30.8182:6.1262,13.2211:2.63727,]\n"
                     "ttc_ljets_6j4b_BDT_shapes : 173.678 +/- 20.3483 [46.8037:11.1746,32.3972:7.20078,28.0243:4.12997,28.481:1.56541,20.0937:1.6901,17.8777:1.77447,]\n"
                     "ttb_ljets_6j4b_BDT_shapes : 378.944 +/- 12.7098 [70.8606:4.60833,56.7989:2.91461,59.8209:3.03892,66.2953:3.50972,58.4558:3.28983,66.713:3.93514,]\n"
                     "ttH_ljets_6j4b_BDT_shapes : 121.679 +/- 2.91801 [11.2774:0.270446,16.7343:0.401308,20.5886:0.493739,21.9574:0.526564,24.7161:0.592722,26.4055:0.633235,]\n";

   ASSERT_STREQ(s.str().c_str(),ref.c_str());

//   // test new histo method too
   s.str("");
   for(auto chan : *(w["simPdf"])) {
      for(auto samp : *(chan->at("samples"))) {
         auto hSamp = samp->histo("");
         s << samp->GetName() << " : " << hSamp.get<TH1>()->GetBinContent(1) << " +/- " << hSamp.get<TH1>()->GetBinError(1) << " [";
         hSamp = samp->histo("x");
         for(int i=1;i<=hSamp.get<TH1>()->GetNbinsX(); i++) {
            s << hSamp.get<TH1>()->GetBinContent(i) << ":" << hSamp.get<TH1>()->GetBinError(i) << ",";
         }
         s << "]" << std::endl;
      }
   }

   ASSERT_STREQ(s.str().c_str(),ref.c_str());


}

TEST(test1,unextendedPdfError) {

   // checks error calculation on an unextended pdf
   // two-bin case, one floating parameter
   RooWorkspace _ws;
   _ws.factory("obs[0,2]"); _ws.var("obs")->setBins(2);
   _ws.factory("RooWrapperPdf::model(ParamHistFunc::model_func(obs,{p1[0.5,0,1],p2[0.5,0,1]}))");
   _ws.function("model_func")->forceNumInt(); // workaround for bogus assert in analytical integral method
   _ws.var("p1")->setConstant();
   _ws.var("p2")->setError(0.1);
   xRooNode w(_ws);

   // calculated error should be:
   // |(up - down)/2|

   ASSERT_NEAR(w["model"]->GetBinError(1), (0.5/0.9 - 0.5/1.1)/2,1e-5);
   ASSERT_NEAR(w["model"]->GetBinError(2), (0.5/0.9 - 0.5/1.1)/2,1e-5);

}

TEST(test1,replaceTest) {
    xRooNode w("/tmp/dado.root");
    w.vars()["gamma_templateSF_SR_emu_Signal_1p5_172p5_bin_0"]->Replace(RooFormulaVar("myVar","gamma_templateSF_SR_emu_Signal_1p5_172p5_bin_0*2",*w.vars()["gamma_templateSF_SR_emu_Signal_1p5_172p5_bin_0"]->get<RooAbsArg>()));
    w.SaveAs("/tmp/dado_replaced.root");
}

//TEST(test1,memLeakTest) {
//
//   //xRooNode w("/Users/cym53897/CLionProjects/xroofit/cmake-build-debug-root_628/tomas-complex-notAllRegions.root");
//   //w["simPdf/SR_4L_SF"]->GetBinContents();
//
//   //xRooNode w("/Users/cym53897/CLionProjects/xroofit/cmake-build-debug-root_628/tomas.root");
//   xRooNode w("/tmp/ws626.root");
//   //w["simPdf/ljets_5j3b_HT/samples"]->GetBinContents();
//   auto tmp = w["simPdf/ljets_5j3b_HT"];
//   std::cout << tmp->GetBinContent(1) << std::endl;
//
//}

TEST(test1,simpleToyTest) {
    xRooNode w("/Users/cym53897/CLionProjects/xroofit/cmake-build-debug-root_628/tomas.root");
    auto hp1 = w.nll().hypoPoint(1,0);
    hp1.addNullToys(1000);
}

TEST(test1,simpleLimitTest) {
    xRooNode w("/Users/cym53897/CLionProjects/xroofit/cmake-build-debug-root_628/tomas.root");
    auto hs = w["simPdf"]->nll().hypoSpace();
    std::unique_ptr<RooAbsCollection> coords(hs.pars()->snapshot());
    std::unique_ptr<RooAbsCollection> toRemove(std::unique_ptr<RooAbsCollection>(coords->selectByAttrib("Constant", false))->snapshot());
    const_cast<RooAbsCollection *>(coords.get())->remove(*toRemove, true, true);
}

TEST(test1,anotherTest) {
    TFile* f = TFile::Open("~/Downloads/FitExampleNtuple_combined_FitExampleNtuple_model.root", "READ");
    {
        std::unique_ptr<RooWorkspace> ws(f->Get<RooWorkspace>("combined"));

        xRooNode model(*ws);
        auto hs = model.nll("obsData").hypoSpace("mu_XS_ttH");
        hs.scan("cls");
        auto result = hs.result();
        result->SetName("clsLimits");
        model.ws()->import(*result);
    }
    f->Close();

}

TEST(test1,toysTest) {
//   xRooNode w("/Users/cym53897/CLionProjects/xroofit/cmake-build-debug-root_628/tomas.root");
//   w.poi()[0]->get<RooAbsArg>()->setStringAttribute("altVal","0");
//   w.np().get<RooArgList>()->setAttribAll("Constant");

    xRooNode w("/Users/cym53897/CLionProjects/xroofit/cmake-build-debug-root_628/tomas.root");
    auto nll = w.nll();

    for(int i=1;i<10001;i++) {
        nll.generate();
    }

   //----

//   auto hs = w["simPdf"]->nll("asimovData").hypoSpace();
//   hs.AddPoints("mu_XS_ttH",10,0,4);
//   hs.graphs("pcls toys");
//   hs.Print();
//   for(auto& hp : hs) hp.Print();

// -----

//   auto hp1 = w.nll().hypoPoint(1,0);
//   hp1.addNullToys(100,0,0.05);
//   hp1.addAltToys(100);

//   xRooNode w2("~/Downloads/jack_failedLimit.root");
//   w2.pars().reduced("mu",true).get<RooArgList>()->setAttribAll("Constant",true);
//   xRooFit::defaultFitConfig()->SetParabErrors(false);
//   auto hs = w2["CombinedPdf"]->nll("combData").hypoSpace();
//   hs.AddPoint("mu=4000").addCLsToys(100);
}

TEST(test1,plotTest) {
    {
        xRooNode w("~/Downloads/largeTrexWS.root");
        w["simPdf"]->Draw("eratio");
    }
}


xRooNode GetNode(const std::string& path) {

    std::unique_ptr<TFile> wsFile(TFile::Open(path.c_str(), "READ"));
    std::unique_ptr<RooWorkspace> ws(wsFile->Get<RooWorkspace>("combined"));

    wsFile->Close();
    xRooNode node(path.c_str());

    return node;
}

TEST(test1,NodeLoadTest) {

    std::cout << "start\n";
    {
        xRooNode node = GetNode("~/Downloads/Fit_1l_allRegions_combined_Fit_1l_model.root");
        auto pdf = node["simPdf"];
        std::cout << "end\n";
    }
    xRooNode node = GetNode("~/Downloads/Fit_1l_allRegions_combined_Fit_1l_model.root");
    auto pdf = node["simPdf"];
    std::cout << "end1\n";
}

#endif