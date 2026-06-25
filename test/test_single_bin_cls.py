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
        ROOT.gROOT.SetBatch(True)
        limits = nllFunc.hypoSpace("mu").limits()

        # Retrieve limits
        obs_lim = limits["obs"].value()
        exp_lim = limits["0"].value()

        # Assert the limits are within expected reasonable values
        self.assertTrue(obs_lim > 0, f"Observed limit should be positive, got {obs_lim}")
        self.assertTrue(exp_lim > 0, f"Expected limit should be positive, got {exp_lim}")
        
        # In this specific configuration, expected and observed limit should be approximately 2.09
        self.assertAlmostEqual(obs_lim, 2.09, places=2)
        self.assertAlmostEqual(exp_lim, 2.09, places=2)

if __name__ == "__main__":
    unittest.main()
