import ROOT
import unittest

class TestSingleBinCLs(unittest.TestCase):
    def test_single_bin_cls_limit(self):
        """
        Creates a simple single-bin model and runs a CLs limit on it.
        """
        # Create workspace and define a single-bin channel called "SR"
        w = ROOT.xRooNode("RooWorkspace", "combined", "my workspace")
        w["pdfs/simPdf/SR"].SetXaxis(1, 0, 1)

        # Set background, signal, and observed data yields
        w["pdfs/simPdf/SR/bkg"].SetBinContent(1, 100.0)
        w["pdfs/simPdf/SR/sig"].SetBinContent(1, 10.0)
        w["pdfs/simPdf/SR"].SetBinData(1, 100.0)

        # Multiply signal by normFactor 'mu'
        w["pdfs/simPdf/SR/sig"].Multiply("mu", "norm")

        # Create NLL function
        nllFunc = w["pdfs/simPdf"].nll("obsData")

        # Run asymptotic CLs limit
        ROOT.gROOT.SetBatch(True) # disables ROOT canvas showing progress of limit search
        limits = nllFunc.hypoSpace("mu").limits()

        # To compute limits from a "fixed point" scan between two values:
        hs = nllFunc.hypoSpace("mu")
        hs.scan("cls", nPoints=20, low=0, high=10)
        limits_fixed = hs.limits()
        obs_lim_fixed = limits_fixed["obs"].value()
        exp_lim_fixed = limits_fixed["0"].value()

        # Retrieve limits
        obs_lim = limits["obs"].value()
        exp_lim = limits["0"].value()

        # Assert the limits are within expected reasonable values
        self.assertTrue(obs_lim > 0, f"Observed limit should be positive, got {obs_lim}")
        self.assertTrue(exp_lim > 0, f"Expected limit should be positive, got {exp_lim}")
        
        # In this specific configuration, expected and observed limit should be approximately 2.09
        self.assertAlmostEqual(obs_lim, 2.09, places=2)
        self.assertAlmostEqual(exp_lim, 2.09, places=2)

        # Verify consistency between the auto-scan and fixed-point scan limits within errors
        self.assertAlmostEqual(obs_lim, obs_lim_fixed, delta=limits_fixed["obs"].error())
        self.assertAlmostEqual(exp_lim, exp_lim_fixed, delta=limits_fixed["0"].error())

    def test_single_bin_cls_limit_with_systematic(self):
        """
        Creates a simple single-bin model with a systematic uncertainty on background 
        and runs a CLs limit on it.
        """
        # Create workspace and define a single-bin channel called "SR"
        w = ROOT.xRooNode("RooWorkspace", "combined", "my workspace")
        w["pdfs/simPdf/SR"].SetXaxis(1, 0, 1)

        # Set background nominal content, and add a +/- 10% systematic variation 'alpha'
        w["pdfs/simPdf/SR/bkg"].SetBinContent(1, 100.0)
        w["pdfs/simPdf/SR/bkg"].SetBinContent(1, 110.0, "alpha", 1.0)
        w["pdfs/simPdf/SR/bkg"].SetBinContent(1, 90.0, "alpha", -1.0)

        # Constrain the systematic parameter 'alpha' with a normal gaussian constraint
        w["pdfs/simPdf"].pars()["alpha"].Constrain("normal")

        # Set signal and observed data yields
        w["pdfs/simPdf/SR/sig"].SetBinContent(1, 10.0)
        w["pdfs/simPdf/SR"].SetBinData(1, 100.0)

        # Multiply signal by normFactor 'mu'
        w["pdfs/simPdf/SR/sig"].Multiply("mu", "norm")

        # Create NLL function
        nllFunc = w["pdfs/simPdf"].nll("obsData")

        # Run asymptotic CLs limit
        ROOT.gROOT.SetBatch(True) # disables ROOT canvas showing progress of limit search
        limits = nllFunc.hypoSpace("mu").limits()

        # Retrieve limits
        obs_lim = limits["obs"].value()
        exp_lim = limits["0"].value()

        # Assert the limits are within expected reasonable values
        self.assertTrue(obs_lim > 0, f"Observed limit should be positive, got {obs_lim}")
        self.assertTrue(exp_lim > 0, f"Expected limit should be positive, got {exp_lim}")
        
        # Expected and observed limits are weaker (approximately 2.84) due to the systematic uncertainty
        self.assertAlmostEqual(obs_lim, 2.84, places=2)
        self.assertAlmostEqual(exp_lim, 2.84, places=2)

if __name__ == "__main__":
    unittest.main()
