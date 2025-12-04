import ROOT

def compute_postfit_mu(bkg_yield, sig_yield_nominal, observed_events):
    """
    implementation for test_2bin:test_postfit_mu

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
    #return mu_hat.getVal(),mu_hat.getError()
    return {"mu_hat":mu_hat.getVal(),"mu_hat_err":mu_hat.getError()}


def compute_simple_histogram_model_limit(hist_file_path):
    """
    This is the implementation for test_hist:test_simple_histogram_model_limit
    :param hist_file_path:
    :return:
    """
    import ROOT
    f = ROOT.TFile(hist_file_path)

    # read all the histograms into a dictionary, for ease
    from collections import defaultdict
    hists = defaultdict(lambda: defaultdict(lambda: defaultdict(None)))

    import ROOT
    f = ROOT.TFile(hist_file_path)
    for k in f.GetListOfKeys():
        cName,sName,vName = k.GetName().split("/")
        hists[cName][sName][vName] = k.ReadObj()

    # each dir at top-level is a different region
    # each dir in region folder is either "Data" or is a sample
    # each sample folder contains a "Nominal" histogram and variation histograms
    global w
    w = ROOT.xRooNode("RooWorkspace","combined","My workspace")

    for chan in hists.keys():
        chanNode = w["pdfs/simPdf"].Add(chan,"channel")
        sampsDir = hists[chan]
        systNPs = set()
        for samp in hists[chan].keys():
            if samp=="Data": continue
            nomHist = sampsDir[samp]["Nominal"]
            if samp=="Signal":
                # ensure signal gets it own mc-stat error since will receive a scale factor
                nomHist.SetOption("statPrefix=sig_mcstat_"+chanNode.GetName().split("=")[-1])
                # or just remove all the errors:
                # for i in range(nomHist.GetNbinsX()): nomHist.SetBinError(i+1,0)
            if not nomHist: raise RuntimeError("Could not get nominal hist for sample " + samp + " in channel " + chan)
            nomHist.SetName(samp)
            chanNode["samples"].Add(nomHist)
            # get the systematics hists
            for syst in sampsDir[samp].keys():
                if syst=="Nominal": continue # not a syst
                systHist = sampsDir[samp][syst]
                # remove any bin errors on the systematics hist ... dont know how to treat those
                for i in range(systHist.GetNbinsX()): systHist.SetBinError(i+1,0)
                isDown = syst.endswith("_Down")
                systName = "alpha_"+syst.rsplit("_",1)[0]
                print(isDown,systName)
                systHist.SetName(systName+"="+ ("-1" if isDown else "1"))
                chanNode["samples"][nomHist.GetName()].Vary(systHist)
                systNPs.add(systName)
        # constrain the systematics (use normal gaussians)
        for systName in systNPs: chanNode.pars()[systName].Constrain("normal")
        # add normFactor to Signal sample
        if chanNode["samples"].contains("Signal"):
            chanNode["samples"]["Signal"].Multiply("mu","norm")

        # now add the data
        hData = hists[chan]["Data"]["Nominal"]
        if not hData: raise RuntimeError("Could not get data histogram for channel " + chan)
        hData.SetName("obsData")
        print(hData)
        chanNode.datasets().Add(hData)

    #w.Print("depth=5")
    answers = {}

    # obtain SR yields and total errors:
    answers["yield_s"] = w["pdfs/simPdf/SR/samples"].reduced("Signal").GetContent()
    answers["yield_s_err"] = w["pdfs/simPdf/SR/samples"].reduced("Signal").GetError()
    answers["yield_b"] = w["pdfs/simPdf/SR/samples"].reduced("Signal",invert=True).GetContent() # invert selects all non-signal
    answers["yield_b_err"] = w["pdfs/simPdf/SR/samples"].reduced("Signal",invert=True).GetError()
    answers["yield_b_err_up"] = w["pdfs/simPdf/SR/samples"].reduced("Signal",invert=True).GetErrorHi()
    answers["yield_b_err_down"] = w["pdfs/simPdf/SR/samples"].reduced("Signal",invert=True).GetErrorLo()

    nllFunc = w["pdfs/simPdf"].nll("obsData")
    nomNll = nllFunc.getVal() # current value of NLL

    # obtain background yield for alpha_WeightBasedModeling_0p5=0.5
    w.pars()["alpha_WeightBasedModeling"].setVal(0.5)
    answers["yield_b_alpha_WeightBasedModeling_0p5"] = w["pdfs/simPdf/SR/samples"].reduced("Signal",invert=True).GetContent()
    for b in range(w["pdfs/simPdf/SR"].GetXaxis().GetNbins()):
        print("var Bin content = ",w["pdfs/simPdf/SR"].GetBinContent(b+1))

    answers["dnll_alpha_WeightBasedModeling_0p5"] = nllFunc.getVal() - nomNll
    w.pars()["alpha_WeightBasedModeling"].setVal(0)

    # compute the 95% CLs asymptotic limits
    ROOT.gROOT.SetBatch(True) # turns off visualization of limit scan progression
    limits = nllFunc.hypoSpace("mu").limits()

    answers["cls_obs_lim"] = limits["obs"].value()
    answers["cls_obs_lim_err"] = limits["obs"].error()
    answers["cls_exp_lim"] = limits["0"].value()
    answers["cls_exp_lim_err"] = limits["0"].error()
    answers["cls_1sig_lim"] = limits["1"].value()
    answers["cls_1sig_lim_err"] = limits["1"].error()

    return answers
