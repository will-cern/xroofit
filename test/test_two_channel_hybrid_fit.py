import os
import unittest
import ROOT


WORKSPACE_FILE = os.path.join(os.path.dirname(__file__), "two_channel_workspace.root")


def setUpModule():
    """
    Module-level fixture: build a two-channel (SR + CR) workspace with
    pretend obsData and save it to a ROOT file so every test class can
    load it independently.

    Model
    -----
    SR (Signal Region) – 1 bin:
        bkg = 20 events, sig = 5 events (scaled by POI mu), obsData = 20
    CR (Control Region) – 1 bin:
        bkg = 100 events, obsData = 100

    The signal-strength POI is 'mu'.  mu is not shared with the CR
    background (no bkg-constraint link), so the CR constrains the bkg
    normalisation only implicitly through the model structure.
    """
    w = ROOT.xRooNode("RooWorkspace", "combined", "two-channel workspace")

    # --- SR ---
    w["pdfs/simPdf/SR"].SetXaxis(1, 0, 1)          # single-bin observable
    w["pdfs/simPdf/SR/bkg"].SetBinContent(1, 20.0)
    w["pdfs/simPdf/SR/sig"].SetBinContent(1, 5.0)
    w["pdfs/simPdf/SR"].SetBinData(1, 20.0)

    # --- CR ---
    w["pdfs/simPdf/CR"].SetXaxis(1, 0, 1)
    w["pdfs/simPdf/CR/bkg"].SetBinContent(1, 100.0)
    w["pdfs/simPdf/CR/sig"].SetBinContent(1, 0.1) # small signal contamination
    w["pdfs/simPdf/CR"].SetBinData(1, 100.0)


    # --- POI ---
    w["pdfs/simPdf/SR/sig"].Multiply("mu", "norm")
    w["pdfs/simPdf/CR/sig"].Multiply("mu") # will reuse norm factor created above

    # optional: declare mu as a poi
    w.poi().Add("mu")


    # Persist to disk
    w.SaveAs(WORKSPACE_FILE)


def tearDownModule():
    """Remove the workspace file produced by the module fixture."""
    if os.path.exists(WORKSPACE_FILE):
        os.remove(WORKSPACE_FILE)


class TestTwoChannelHybridFit(unittest.TestCase):
    """
    Tests a realistic analysis workflow using a two-channel xRooFit workspace:
      1. Load the workspace from file.
      2. Fix mu=0 (background-only hypothesis) and fit the CR only.
      3. Generate a post-fit Asimov dataset in the SR using the CR fit result.
      4. Combine the SR Asimov with the CR observed data into a hybrid dataset.
      5. Release mu and perform an unconditional fit to all channels.
      6. Assert that the unconditional fit returns mu ≈ 0 (consistent with
         the bkg-only Asimov generation).
    """

    def setUp(self):
        """Load the workspace fresh for each test."""
        self.w = ROOT.xRooNode(WORKSPACE_FILE)

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _assert_valid_fit(self, fr, label=""):
        prefix = f"[{label}] " if label else ""
        self.assertEqual(fr.status(), 0, f"{prefix}Fit status should be 0 (converged)")
        self.assertEqual(fr.covQual(), 3, f"{prefix}covQual should be 3 (positive-definite covariance)")

    # ------------------------------------------------------------------
    # Tests
    # ------------------------------------------------------------------

    def test_cr_only_bkg_fit(self):
        """
        A bkg-only (mu=0) fit restricted to the CR should converge and
        leave mu constant at 0.
        """
        w = self.w
        w.pars()["mu"].setVal(0)
        w.pars()["mu"].setConstant()

        nll_cr = w["pdfs/simPdf"].reduced("CR").nll("obsData")
        fr_cr = nll_cr.minimize()

        self._assert_valid_fit(fr_cr, "CR bkg-only fit")

        # mu must still be 0 after the fit (it was held constant)
        # Note: will only have mu in the constPars list if had sig sample
        # in the CR (multiplied by mu). Otherwise the CR fit is independent
        # of mu and there is no need for it in the constPars list.
        mu_val = fr_cr.constPars().find("mu").getVal()
        self.assertAlmostEqual(mu_val, 0.0, places=10)

    def test_sr_asimov_generation_from_cr_fit(self):
        """
        After the CR bkg-only fit, the SR Asimov dataset (generated at the
        post-CR-fit parameter values) should have a total yield equal to
        the SR bkg prediction (mu=0 → no signal contribution).
        """
        w = self.w
        w.pars()["mu"].setVal(0)
        w.pars()["mu"].setConstant()

        nll_cr = w["pdfs/simPdf"].reduced("CR").nll("obsData")
        fr_cr = nll_cr.minimize()
        self._assert_valid_fit(fr_cr, "CR bkg-only fit")

        # Generate post-fit Asimov in SR using the CR fit result
        asiData = w["pdfs/simPdf"].reduced("SR").generate(fr_cr, expected=True)
        self.assertIsNotNone(asiData, "SR Asimov dataset should not be None")

        # Asimov SR total yield should be the bkg-only prediction (20.0 events)
        total_yield = asiData.sumEntries()
        self.assertAlmostEqual(total_yield, 20.0, places=5,
                               msg=f"SR Asimov yield should be 20 (bkg-only), got {total_yield}")

    def test_hybrid_dataset_construction(self):
        """
        The hybrid dataset (SR Asimov + CR observed) should contain
        entries from both channels.
        """
        w = self.w
        w.pars()["mu"].setVal(0)
        w.pars()["mu"].setConstant()

        nll_cr = w["pdfs/simPdf"].reduced("CR").nll("obsData")
        fr_cr = nll_cr.minimize()
        self._assert_valid_fit(fr_cr, "CR bkg-only fit")

        asiData = w["pdfs/simPdf"].reduced("SR").generate(fr_cr, expected=True)
        asiData.Add(w["pdfs/simPdf"].reduced("CR").datasets()["obsData"])

        # Hybrid should have SR bkg (20) + CR obs (100) = 120 events
        total = asiData.sumEntries()
        self.assertAlmostEqual(total, 120.0, places=5,
                               msg=f"Hybrid dataset should have 120 total events, got {total}")

    def test_unconditional_fit_on_hybrid_dataset(self):
        """
        An unconditional fit to the hybrid dataset should:
          - Converge (status=0, covQual=3)
          - Return mu ≈ 0, because the SR data was generated under the
            bkg-only hypothesis (mu=0).
        """
        w = self.w

        # Step 1: bkg-only conditional CR fit
        w.pars()["mu"].setVal(0)
        w.pars()["mu"].setConstant()
        nll_cr = w["pdfs/simPdf"].reduced("CR").nll("obsData")
        fr_cr = nll_cr.minimize()
        self._assert_valid_fit(fr_cr, "CR bkg-only fit")

        # Step 2: build hybrid dataset
        asiData = w["pdfs/simPdf"].reduced("SR").generate(fr_cr, expected=True)
        asiData.Add(w["pdfs/simPdf"].reduced("CR").datasets()["obsData"])

        # Step 3: unconditional fit to all channels with mu floating
        w.pars()["mu"].setConstant(False)
        nll_all = w["pdfs/simPdf"].nll(asiData)
        fr_all = nll_all.minimize()
        self._assert_valid_fit(fr_all, "Unconditional full fit")

        # mu_hat should be ≈ 0 (generated under bkg-only)
        mu_hat = fr_all.floatParsFinal().find("mu").getVal()
        self.assertAlmostEqual(mu_hat, 0.0, places=5,
                               msg=f"mu_hat should be ~0 on bkg-only Asimov, got {mu_hat}")


        # do a signal significance test 
        # will repeat above process but with a mu=1 conditional fit
        # and check mu_hat ~ 1

        w.pars()["mu"].setVal(1)
        w.pars()["mu"].setConstant()
        nll_cr = w["pdfs/simPdf"].reduced("CR").nll("obsData")
        fr_cr = nll_cr.minimize()
        self._assert_valid_fit(fr_cr, "CR mu=1 fit")
        asiData = w["pdfs/simPdf"].reduced("SR").generate(fr_cr, expected=True)
        asiData.Add(w["pdfs/simPdf"].reduced("CR").datasets()["obsData"])
        w.pars()["mu"].setConstant(False)
        nll_all = w["pdfs/simPdf"].nll(asiData)
        fr_all = nll_all.minimize()
        self._assert_valid_fit(fr_all, "Unconditional full fit")

        # mu_hat should be ≈ 1 (generated under mu=1)
        mu_hat = fr_all.floatParsFinal().find("mu").getVal()
        self.assertAlmostEqual(mu_hat, 1.0, places=3,
                               msg=f"mu_hat should be ~1 on mu=1 Asimov, got {mu_hat}")

        # to evaluate discovery significance, we do a hypothesis test
        # of mu=0 (bkg only) hypothesis, and get the p-value
        # this uses the hybrid mu=1 dataset we created
        # use the u0 (uncapped) discovery test statistic:

        hp = nll_all.hypoPoint(value=0,alt_value=1)
        print("Discovery P-Value = ",hp.pNull_asymp())
        print("Significance = ",ROOT.RooStats.PValueToSignificance(hp.pNull_asymp().value()))

        # can compare to the 'expected' significance, which is based on the alt_value
        print("Expected Discovery P-Value = ",hp.pNull_asymp(nSigma=0))

        self.assertAlmostEqual(hp.pNull_asymp().value(), hp.pNull_asymp(nSigma=0).value(), delta=0.01,
                               msg=f"hybrid asimov (mu=1) dataset significance should be similar to expected (mu=1) significance")



if __name__ == "__main__":
    unittest.main()
