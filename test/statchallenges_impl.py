import ROOT

def compute_postfit_mu(bkg_yield, sig_yield_nominal, observed_events):
    """
    Builds a one-channel model with n-bins, with the given bkg, signal, and observed yields.
    Signal sample gets a mu normFactor.
    :param bkg_yield: the background per-bin yields
    :param sig_yield_nominal: the expected signal per-bin yields corresponding to mu=1
    :param observed_events: the observed per-bin yields
    :return: the post-fit value and error of the normFactor.
    """

    w = ROOT.xRooNode("RooWorkspace","combined","my workspace")
    w["pdfs/simPdf/SR"].SetXaxis(len(bkg_yield),0,len(bkg_yield)) # create n-bin channel called "SR"

    for i in range(len(bkg_yield)):
        w["pdfs/simPdf/SR/bkg"].SetBinContent(i+1,bkg_yield[i])
        w["pdfs/simPdf/SR/sig"].SetBinContent(i+1,sig_yield_nominal[i])
        w["pdfs/simPdf/SR"].SetBinData(i+1,observed_events[i])

    print(w["pdfs/simPdf/SR"].GetContent())

    w["pdfs/simPdf/SR/sig"].Multiply("mu","norm") # scale signal by a normFactor called mu

    fr = w["pdfs/simPdf"].nll("obsData").minimize() # run the minimization

    # confirm fit succeeds by checking status and covariance quality
    assert fr.status()==0,"Fit failed"
    assert fr.covQual()==3,"CovQual bad"

    mu_hat = fr.floatParsFinal().find("mu")
    return mu_hat.getVal(),mu_hat.getError()
