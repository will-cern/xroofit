
#include "xRooFit/xRooFit.h"
#include "RooConstVar.h"
#include "RooRealVar.h"
#include "gtest/gtest.h"
#include "Math/ProbFunc.h"
#include "TH1D.h"
#include "TFile.h"

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


double oneChannel(double data, double bkg, double bkg_uncert, double sig, double sig_uncert_up, double sig_uncert_down, double lumi_uncert) {

    xRooNode w("RooWorkspace","w","w");

    w["simPdf/channel1"]->SetXaxis("obs",1,0,1); // 1-bin channel
    w["simPdf/channel1/bkg"]->SetBinContent(1,bkg);
    //w["simPdf/channel1/bkg"]->get<RooAbsArg>()->setStringAttribute("statPrefix","myPrefix"); // example of how to give a sample its own independent stat factors
    w["simPdf/channel1/bkg"]->SetBinError(1,bkg_uncert); // creates a special shape factor called 'statFactor' which may share pars with other samples

    w["simPdf/channel1/sig"]->SetBinContent(1,sig);
    // types of factor are: norm, overall, shape, histo
    w["simPdf/channel1/sig"]->Multiply("c1_sig_overall","overall"); // add an overall factor that we will vary
    w["simPdf/channel1/sig/c1_sig_overall"]->SetContents(sig_uncert_up,"ucs",1);
    w["simPdf/channel1/sig/c1_sig_overall"]->SetContents(sig_uncert_down,"ucs",-1);
    w["simPdf/channel1/sig"]->Multiply("lumi","norm"); // multiply by a lumi factor .. will constrain it below
    w["simPdf/channel1/sig"]->Multiply("mu_Sig","norm");
    // finally put constraints on the parameters of the model we wish to explicitly constrain
    w["simPdf"]->pars()["lumi"]->Constrain(TString::Format("gaussian(1,%f)",lumi_uncert).Data()); // lumi constraint
    w["simPdf"]->pars()["ucs"]->Constrain("normal");

    // set the data
    w["simPdf/channel1"]->datasets()["obsData"]->SetBinContent(1,data);


    // create NLL function using simPdf model with obsData
    auto nll = w["simPdf"]->createNLL("obsData");

    // Perform a hypothesis test of mu=1 hypothesis using mu=0 as alt hypothesis
    auto hypoTest = nll.hypoPoint("mu_Sig",1,0);

    auto _pll = hypoTest.pll();
    auto _sigma_mu = hypoTest.sigma_mu();

    auto clsb_obs = hypoTest.pNull_asymp();
    auto clb_obs = hypoTest.pAlt_asymp();

    std::cout << "obs_pll = " << _pll.first << " sigma_mu = " << _sigma_mu.first << std::endl;
    std::cout << "cls_obs = " << (clsb_obs/clb_obs) << " [ clsb_obs = " << clsb_obs << " clb_obs = " << clb_obs << " ]" << std::endl;

    for(int i=-2;i<=2;i++) {
        std::cout << i << " sigma: " << hypoTest.pCLs_asymp(i) << std::endl;
    }


    // can also generate toys to estimate
    auto obsData = nll.getData();

    TH1D hNull("null","null",100,0,2*_pll.first);
    TH1D hAlt("alt","alt",100,0,2*_pll.first); hAlt.SetLineColor(kRed);

    w.pars()["mu_Sig"]->get<RooRealVar>()->setVal(1);
    w.pars()["mu_Sig"]->get<RooRealVar>()->setConstant();
    auto null_fit = nll.minimize();

    // speed up toys by disabling hesse
    nll.fitConfig()->SetParabErrors(false);

    double toy_clsb_obs = 0;
    int nToys=15;
    std::vector<double> toy_vals; toy_vals.reserve(nToys);
    for(int i=0;i<nToys;i++) {
        auto toy = nll.generate(); //xRooFit::generateFrom(*nll.fPdf,null_fit); //nll.generate();
        nll.setData(toy);
        auto toy_pll = nll.hypoPoint("mu_Sig",1,std::numeric_limits<double>::quiet_NaN(), xRooFit::Asymptotics::OneSidedPositive).pll();
        if (std::isnan(toy_pll.first)) std::cout << " nan null " << std::endl;
        if (toy_pll.first >= _pll.first) toy_clsb_obs++;
        toy_vals.push_back(toy_pll.first);
        hNull.Fill(toy_pll.first);
    }
    toy_clsb_obs /= nToys;
    std::cout << "toy clsb_obs = " << toy_clsb_obs << std::endl;
    nll.setData(obsData);

    w.vars()["mu_Sig"]->get<RooRealVar>()->setVal(0);
    auto alt_fit = nll.minimize();



    double toy_clb_obs = 0;
    std::vector<double> toy_vals_b; toy_vals_b.reserve(nToys/10);
    for(int i=0;i<nToys/10;i++) {
        auto toy = nll.generate(); //xRooFit::generateFrom(*nll.fPdf,alt_fit); //nll.generate();
        nll.setData(toy);
        auto toy_pll = nll.hypoPoint("mu_Sig",1,std::numeric_limits<double>::quiet_NaN(), xRooFit::Asymptotics::OneSidedPositive).pll();
        if (std::isnan(toy_pll.first)) std::cout << " nan alt " << std::endl;
        if (toy_pll.first >= _pll.first) toy_clb_obs++;
        toy_vals_b.push_back(toy_pll.first);
        hAlt.Fill(toy_pll.first);
    }
    toy_clb_obs /= (nToys/10);
    std::cout << "toy clb_obs = " << toy_clb_obs << std::endl;

    std::cout << "toy cls_obs = " << toy_clsb_obs/toy_clb_obs << std::endl;

    std::sort(std::begin(toy_vals_b),std::end(toy_vals_b));

    for(int i=-2;i<=3;i++) {
        auto k = (i==3) ? _pll.first : toy_vals_b.at( toy_vals_b.size()*ROOT::Math::gaussian_cdf_c(i));
        double pval = 0, pval_b=0;
        for(auto& x : toy_vals) { if (x >= k) pval++; }
        for(auto& x : toy_vals_b) { if (x >=k) pval_b++; }
        if (i==3) {
            std::cout << i << " sigma (pll=" << k << "): " << pval / toy_vals.size() << " " << pval_b/toy_vals_b.size() << " "
                      << std::endl;
        } else {
            std::cout << i << " sigma (pll=" << k << "): " << pval * ROOT::Math::gaussian_cdf(i) / toy_vals.size()
                      << std::endl;
        }
    }


    //w.SaveAs("oneChannel.root");

    TFile f("tsDists.root","recreate");
    hNull.SetDirectory(&f);
    hAlt.SetDirectory(&f);
    f.Write();


    return clsb_obs/clb_obs;


}

TEST(test1,test1) {

    auto res = oneChannel(0,0.43,0.16,5.611,1.19266,0.807337,0.017);

    ASSERT_LT(abs(res - 0.0019764892592501124),1e-7);

}