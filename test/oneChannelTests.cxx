
#include "xRooFit/xRooFit.h"
#include "RooConstVar.h"
#include "RooRealVar.h"
#include "gtest/gtest.h"

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

double oneChannel(double data, double bkg, double bkg_uncert, double sig, double sig_uncert_up, double sig_uncert_down, double lumi_uncert) {

    xRooNode w("RooWorkspace","w","w");

    w["simPdf/channel1"]->SetXaxis("obs",1,0,1); // 1-bin channel
    w["simPdf/channel1/bkg"]->SetBinContent(1,bkg);
    //w["simPdf/channel1/bkg"]->get<RooAbsArg>()->setStringAttribute("statPrefix","myPrefix"); // example of how to give a sample its own independent stat factors
    w["simPdf/channel1/bkg"]->SetBinError(1,bkg_uncert); // creates a special shape factor called 'statFactor' which may share pars with other samples

    w["simPdf/channel1/sig"]->SetBinContent(1,sig);
    // types of factor are: norm, overall, shape, histo
    w["simPdf/channel1/sig"]->Multiply("c1_sig_overall","overall"); // add an overall factor that we will vary
    w["simPdf/channel1/sig/c1_sig_overall"]->SetBinContent(1/*doesn't matter*/,sig_uncert_up,"ucs",1);
    w["simPdf/channel1/sig/c1_sig_overall"]->SetBinContent(1/*doesn't matter*/,sig_uncert_down,"ucs",-1);
    w["simPdf/channel1/sig"]->Multiply("lumi","norm"); // multiply by a lumi factor .. will constrain it below
    w["simPdf/channel1/sig"]->Multiply("mu_Sig","norm");
    // finally put constraints on the parameters of the model we wish to explicitly constrain
    w["simPdf"]->pars()["lumi"]->Constrain(TString::Format("gaussian(1,%f)",lumi_uncert).Data()); // lumi constraint
    w["simPdf"]->pars()["ucs"]->Constrain("normal");

    // set the data
    w["simPdf/channel1"]->datasets()["obsData"]->SetBinContent(1,data);


    // Compute CLs p-value with asymptotic formulae
    auto nll = w["simPdf"]->createNLL("obsData");

    auto _pll = nll.pll("mu_Sig",1,xRooFit::Asymptotics::OneSidedPositive);
    auto _sigma_mu = nll.sigma_mu("mu_Sig",1,0);

    auto clsb_obs = xRooFit::Asymptotics::PValue(xRooFit::Asymptotics::OneSidedPositive,_pll.first,1,1,_sigma_mu.first,0);
    auto clb_obs = xRooFit::Asymptotics::PValue(xRooFit::Asymptotics::OneSidedPositive,_pll.first,1,0,_sigma_mu.first,0);

    std::cout << "clsb_obs = " << clsb_obs << " clb_obs = " << clb_obs << std::endl;

    // can also generate toys to estimate
    auto obsData = nll.getData();

    w.pars()["mu_Sig"]->get<RooRealVar>()->setVal(1);
    w.pars()["mu_Sig"]->get<RooRealVar>()->setConstant();
    auto null_fit = nll.minimize();

    // speed up toys by disabling hesse
    nll.fitConfig()->SetParabErrors(false);

    double toy_clsb_obs = 0;
    int nToys=10000;
    for(int i=0;i<nToys;i++) {
        auto toy = nll.generate();
        nll.setData(toy);
        auto toy_pll = nll.pll("mu_Sig",1,xRooFit::Asymptotics::OneSidedPositive);
        if (toy_pll.first >= _pll.first) toy_clsb_obs++;
    }
    toy_clsb_obs /= nToys;
    std::cout << "toy clsb_obs = " << toy_clsb_obs << std::endl;
    nll.setData(obsData);

    w.vars()["mu_Sig"]->get<RooRealVar>()->setVal(0);
    auto alt_fit = nll.minimize();

    double toy_clb_obs = 0;
    for(int i=0;i<nToys/10;i++) {
        auto toy = nll.generate();
        nll.setData(toy);
        auto toy_pll = nll.pll("mu_Sig",1,xRooFit::Asymptotics::OneSidedPositive);
        if (toy_pll.first >= _pll.first) toy_clb_obs++;
    }
    toy_clb_obs /= (nToys/10);
    std::cout << "toy clb_obs = " << toy_clb_obs << std::endl;



    w.SaveAs("oneChannel.root");

    return clsb_obs/clb_obs;


}

TEST(test1,test1) {

    auto res = oneChannel(0,0.43,0.16,5.611,1.19266,0.807337,0.017);

    ASSERT_DOUBLE_EQ(res, 0.0019764892592501124);

}