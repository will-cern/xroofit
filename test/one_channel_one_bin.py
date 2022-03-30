
# this script shows how to compute limits on a single-channel single-bin model

import ROOT

n_bkg = 100
n_bkg_uncert = 20
n_sig = 10
n_sig_uncert = 0


w = ROOT.xRooNode("RooWorkspace","combined","my workspace")
m = w.Add("simPdf","model") # add a model
c = m.Add("sr","channel") # add "signal region" (sr) channel
c.SetXaxis("obs","dummy obs",1,0,1) # single bin channel
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
mu = sig_sr.Multiply("mu","norm") # multiply by a norm factor

# set the physically allowed range of mu and expand slightly the fittable range
mu.setRange(-0.1,100)
mu.setRange("physical",0,10)
mu.setRange("scan",0.1,10)

d = c.datasets()["obsData"].SetBinContent(1,n_bkg)

#d = w.Add("obsData","dataset"); d["channelCat=sr"].SetBinContent(1,5) -- TODO should make this sort of thing work

# cosmetics
bkg_sr.SetFillColor(ROOT.kGreen)
sig_sr.SetFillColor(ROOT.kRed)


# build NLL function from model and dataset
nll = m.createNLL("obsData")

def getPValues(mu_test):

    pllType = ROOT.xRooFit.Asymptotics.OneSidedPositive # for upperLimits
    pll_obs = nll.pll("mu",mu_test,pllType)
    sigma_mu = nll.sigma_mu("mu",mu_test,0)
    pval_sb = ROOT.xRooFit.Asymptotics.PValue(pllType,pll_obs,mu_test,mu_test,sigma_mu,mu.getMin("physical"),mu.getMax("physical"))
    pval_b = ROOT.xRooFit.Asymptotics.PValue(pllType,pll_obs,mu_test,0.,sigma_mu,mu.getMin("physical"),mu.getMax("physical"))

    return pval_sb,pval_b

def clsPValue(mu_test):
    pval = getPValues(mu_test)
    print(mu_test,pval[0],pval[1])
    if pval[0]==pval[1]: return 1.
    return pval[0]/pval[1]

# find what value of mu has clsPValue=0.05 ... that's the upper limit
gr = ROOT.TGraph()
step = (mu.getMax("scan")-mu.getMin("scan"))/19
mu_test = mu.getMin("scan")
while mu_test <= mu.getMax("scan"):
    gr.SetPoint(gr.GetN(),mu_test,clsPValue(mu_test))
    mu_test += step

gr.DrawClone("ALP")
ROOT.TLine().DrawLine(mu.getMin("scan"),0.05,mu.getMax("scan"),0.05)