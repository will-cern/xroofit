
# this script shows how to compute limits on a single-channel single-bin model

import ROOT

n_bkg = 100
n_bkg_uncert = 20
n_sig = 10
n_sig_uncert = 0
n_data = 110


w = ROOT.xRooNode("RooWorkspace","combined","my workspace")
m = w.Add("simPdf","model") # add a model
c = m.Add("sr","channel") # add "signal region" (sr) channel
c.SetXaxis("dummy obs",1,0,1) # single bin channel
bkg_sr = c.Add("bkg","sample") # add a sample (component) to the channel called "bkg"
bkg_sr.SetBinContent(1,n_bkg)
if n_bkg_uncert>0:
    bkg_sr.SetBinContent(1,n_bkg+n_bkg_uncert,"alpha_bkg",1) # add a variation to bkg_sr
    m.pars()["alpha_bkg"].Constrain("normal") # ensure alpha is constrained (use a normal gaussian constraint)
sig_sr = c.Add("sig","sample") # add another sample, "sig", to represent the signal
sig_sr.SetBinContent(1,n_sig)
if n_sig_uncert>0:
    sig_sr.SetBinContent(1,n_sig+n_sig_uncert,"alpha_sig",1) # add a variation
    m.pars()["alpha_sig"].Constrain("normal") # ensure alpha is constrained (use a normal gaussian constraint)
mu = sig_sr.Multiply("mu","norm") # multiply sig by a floating norm factor

d = c.datasets()["obsData"].SetBinContent(1,n_data) # add the data

# set the physically allowed range of mu and expand slightly the fittable range
mu.setRange(-0.1,100)
mu.setRange("physical",0,10)
mu.setBinning(ROOT.RooUniformBinning(0.1,10,20),"hypoPoints") # defines points to test: 21 points between 0 and 10 (uses bin boundaries) - use 0 bins for autoscan
ht = ROOT.xRooFit.hypoTest(w.get())
# can extract results from the ht canvas like this:
print("Observed CLs Limit:",ht.GetPrimitive("obs_CLs").GetPointX(0))
print("Expected CLs Limits [-2,-1,0,1,2]:",
      ht.GetPrimitive("exp-2_CLs").GetPointX(0),
      ht.GetPrimitive("exp-1_CLs").GetPointX(0),
      ht.GetPrimitive("exp0_CLs").GetPointX(0),
      ht.GetPrimitive("exp1_CLs").GetPointX(0),
      ht.GetPrimitive("exp2_CLs").GetPointX(0))



## The rest of this code shows how to manually produce the results of the hypoTest function
#
# mu.setRange("scan",0.1,10)
# # build NLL function from model and dataset
# nll = m.createNLL("obsData")
#
# # find what value of mu has clsPValue=0.05 ... that's the upper limit
# gr = ROOT.TGraph();
# from collections import defaultdict
# expected_gr = defaultdict(ROOT.TGraph)
# step = (mu.getMax("scan")-mu.getMin("scan"))/19
# mu_test = mu.getMin("scan")
# mu_alt = 0.
# pllType = ROOT.xRooFit.Asymptotics.OneSidedPositive # for upperLimits
# while mu_test <= mu.getMax("scan"):
#     gr.SetPoint(gr.GetN(),mu_test,nll.hypoPoint("mu",mu_test,mu_alt,pllType).pCLs_asymp())
#     for i in range(-2,3): expected_gr[i].SetPoint(expected_gr[i].GetN(),mu_test,nll.hypoPoint("mu",mu_test,mu_alt,pllType).pCLs_asymp(i))
#     mu_test += step
#
# expected_2sigma = ROOT.TGraph(expected_gr[-2]);expected_2sigma.SetTitle(f";{mu.GetTitle()};p-value")
# expected_2sigma.Sort(ROOT.TGraph.CompareX,False)
# cc = ROOT.TList(); cc.Add(expected_gr[2]);expected_2sigma.Merge(cc)
# expected_2sigma.SetFillColor(ROOT.kYellow)
#
# expected_1sigma = ROOT.TGraph(expected_gr[-1])
# expected_1sigma.Sort(ROOT.TGraph.CompareX,False)
# cc = ROOT.TList(); cc.Add(expected_gr[1]);expected_1sigma.Merge(cc)
# expected_1sigma.SetFillColor(ROOT.kGreen)
#
#
# xx = ROOT.TCanvas()
# expected_2sigma.DrawClone("AF");
# expected_1sigma.DrawClone("F");
# expected_gr[0].SetLineStyle(2);expected_gr[0].DrawClone("L")
# gr.DrawClone("LP")
# ROOT.TLine().DrawLine(mu.getMin("scan"),0.05,mu.getMax("scan"),0.05)
# ROOT.gPad.RedrawAxis();