# Advanced Hypothesis Testing

## Hypothesis Spaces

A **hypothesis space** (hypoSpace) is defined by selecting which parameters
define the hypotheses to test. Assign parameters to axes of the hypoSpace (one
parameter per axis) or fix them to specific values.

Examples:

- Higgs search: mass parameter on x-axis, signal strength on y-axis
- SUSY search: two mass parameters on axes, signal strength fixed to 1

## Test Statistics

All test statistics are variants of the profile likelihood ratio:

t_μ = −2 ln(L(μ,ν̂̂,θ) / L(μ̂,ν̂,θ))

### Available Test Statistics

| Name       | Symbol | Use case     | Description                                                                |
| ---------- | ------ | ------------ | -------------------------------------------------------------------------- |
| `tmu`      | t_μ    | Two-sided    | Full PLR, no capping                                                       |
| `qmu`      | q_μ    | Upper limits | One-sided capped-above: 0 if μ̂ ≥ μ                                         |
| `qmutilde` | q̃_μ    | Upper limits | One-sided capped-above + lower-bounded: accounts for physical boundary μ_L |
| `q0`       | q₀     | Discovery    | One-sided capped-below: 0 if μ̂ ≤ 0                                         |
| `u0`       | u₀     | Discovery    | Uncapped: −t₀ if μ̂ ≤ 0, else t₀                                            |

`qmutilde` is the default for upper limits. `u0` has become popular for
discovery in recent years.

## HypoPoint Quantities

Each scanned point in the hypoSpace is a **hypoPoint** with computable
quantities:

| Method (results have `.value()` and `.error()`) | Description                                     |
| ----------------------------------------------- | ----------------------------------------------- |
| `pNull_asymp()`                                 | Observed p_null from asymptotic formulae        |
| `pNull_asymp(n)`                                | n-sigma expected p_null                         |
| `pAlt_asymp()`                                  | Observed p_alt from asymptotic formulae         |
| `pAlt_asymp(n)`                                 | n-sigma expected p_alt (= Φ(n) by construction) |
| `pCLs_asymp()`                                  | pNull/pAlt (observed)                           |
| `pCLs_asymp(n)`                                 | pNull(n)/pAlt(n) (n-sigma expected)             |
| `ts_asymp()`                                    | Observed test statistic value                   |
| `ts_asymp(n)`                                   | n-sigma Asimov test statistic value             |

Replace `_asymp` with `_toys` for toy-based values.

## HypoPoint Fits

| Method                 | Description                                                                      |
| ---------------------- | -------------------------------------------------------------------------------- |
| `ufit()`               | Unconditional fit to observed data (PLR denominator)                             |
| `cfit_null()`          | Conditional fit with POI at null hypothesis values (PLR numerator)               |
| `cfit_alt()`           | Conditional fit with POI at alt hypothesis values (needed for Asimov generation) |
| `cfit_lbound()`        | Conditional fit with POI at lower bound μ*L (needed for q̃*μ if μ̂ < μ_L)          |
| `asimov().ufit()`      | Unconditional fit to Asimov dataset (for asymptotic formulae)                    |
| `asimov().cfit_null()` | Null conditional fit to Asimov dataset                                           |

## Full Verbose Limit-Setting Example

```python
import ROOT
XRF = ROOT  # or: XRF = ROOT.Experimental.XRooFit

fileName  = "path/to/workspace.root"
pdfName   = "simPdf"
channels  = "*"         # comma-separated; exclude VRs
dsName    = "obsData"   # "" for Asimov
poiName   = ""          # "" to auto-infer
asimovVal = 0           # POI value for Asimov dataset
scanMin   = 0
scanMax   = 10
scanN     = 0           # 0 = auto-scan
scanType  = "cls visualize"
constPars = ""          # "*" for stat-only
tsType    = XRF.xRooFit.TestStatistic.qmutilde
nSigmas   = [0, 1, 2, -1, -2, float('nan')]  # NaN = observed

w = XRF.xRooNode(fileName)
if poiName == "":
    poiName = w.poi()[0].GetName()
if constPars != "":
    w.pars().reduced(constPars).setAttribAll("Constant")
w.pars()[poiName].setVal(asimovVal)

hs = w[pdfName].reduced(channels).nll(dsName).hypoSpace(
    poiName, tsType
)
hs.scan(scanType, scanN, scanMin, scanMax, nSigmas)
limits = hs.limits()

print(limits)
hasNaN = False
for nSigma, lim in dict(limits).items():
    if ROOT.TMath.IsNaN(lim.value()):
        hasNaN = True
if hasNaN:
    hs.Print()  # inspect status codes

# Save result to workspace
outFile = "result.root"
w.Add(hs.result())
w.SaveAs(outFile)
w.Browse()  # hypoSpace appears under "scans" folder
```

### Key Parameters

- **`channels`**: comma-separated channel list; use `reduced()` to exclude
  validation regions
- **`nSigmas`**: `float('nan')` requests the observed limit; integers request
  expected ±N sigma limits
- **`limits()`**: returns `std::map` keyed by `"-2"`, `"-1"`, `"0"`, `"1"`,
  `"2"` (expected) and `"obs"` (observed); each value has `.value()` and
  `.error()`

## Toy-Based Limits

### Full scan with toys

Replace `scanType` to use toys instead of asymptotic formulae:

```python
scanType = "cls toys=1000.1"  # 1000 null + 100 alt toys per point
scanType = "cls toys=1000"    # 1000 null + 1000 alt toys per point
scanType = "cls toys"         # auto: 100-toy blocks until 2σ
                              # confidence on pCLs, max 10k
```

### Validating asymptotic limits with toys (single-point)

The recommended workflow for validating an asymptotic CLs result is:

1. Compute the asymptotic limit first (fast)
2. Create a single hypoPoint at the observed limit value
3. Generate null and alt toys at that point
4. Compare `pCLs_toys()` with `pCLs_asymp()` — they should agree if
   asymptotics are valid

```python
import ROOT
w = ROOT.xRooNode("workspace.root")

# Step 1: asymptotic limit
hs = w.nll("obsData").hypoSpace()
limits = hs.limits()
print(limits)

# Step 2: single hypoPoint at observed limit (alt_value=0 is bkg-only)
hp = w.nll("obsData").hypoPoint(value=limits["obs"].value(), alt_value=0)

# Step 3: generate toys (can be interrupted with Ctrl+C; toys generated
# so far are preserved and further calls can add more)
hp.addNullToys(2000)
hp.addAltToys(2000)

# Step 4: compare
print("Asymptotic pCLs:", hp.pCLs_asymp())
print("Toy-based pCLs:", hp.pCLs_toys())

# Draw test statistic distributions; asymptotic shown as dashed line
# If toys follow the asymptotic curve, the asymptotic result is likely valid
hp.Draw()
```

**Interpreting validation results:** At the asymptotic observed limit, the
asymptotic pCLs should be ~0.05. If the toy-based pCLs is slightly above 0.05,
the toy-based limit will be slightly larger than the asymptotic limit.

### hypoPoint test statistic auto-inference

`nll.hypoPoint(value, alt_value, pllType)` — if `pllType` is omitted, the
test statistic is inferred:

| Condition                 | Inferred test statistic | Typical use case |
| ------------------------- | ----------------------- | ---------------- |
| `value >= alt_value`      | OneSidedPositive (q̃_μ)  | Upper limits     |
| `value < alt_value`       | Uncapped (u₀)           | Discovery        |
| `alt_value` is NaN        | TwoSided (t_μ)          | General PLR      |

### Toy data structure

`hp.nullToys` and `hp.altToys` are `std::vector<std::tuple<int,double,double>>`.
Each tuple contains:

| Index (`std::get<N>`) | Content              | Notes                          |
| --------------------- | -------------------- | ------------------------------ |
| 0                     | Random seed          | Reproducible via `generateAlt` / `generateNull` |
| 1                     | Test statistic value | NaN indicates a failed ("bad") toy |
| 2                     | Weight               | Currently always 1             |

Access in Python:

```python
seed   = ROOT.std.get[0](hp.altToys[i])
ts_val = ROOT.std.get[1](hp.altToys[i])
weight = ROOT.std.get[2](hp.altToys[i])
```

### Inspecting toy distributions

```python
hs.Print()    # list hypoPoints
hs[4].Draw()  # draw test statistic distribution for 5th point
```

For capped test statistics (all except `u0`), check for toys with ts < 0. These
indicate unconditional fits that did not converge on the true minimum — often
caused by setting tolerance too high.

## Debugging Bad Toys

Bad toys are toys whose fits failed, resulting in a NaN test statistic value.
When `hp.Draw()` is called, the legend shows `N_bad/0=<count>` for the number
of bad toys. A high fraction of bad toys invalidates the toy-based result.

### Identifying bad toys

```python
import ROOT

# Find first bad toy in alt toys
i = 0
while not ROOT.std.isnan(ROOT.std.get[1](hp.altToys[i])):
    i += 1

# Count all bad toys
n_bad = sum(1 for j in range(len(hp.altToys))
            if ROOT.std.isnan(ROOT.std.get[1](hp.altToys[j])))
print(f"Bad alt toys: {n_bad}/{len(hp.altToys)}")
```

### Reproducing and inspecting a bad toy

Use the seed from a bad toy to regenerate it and inspect visually:

```python
seed = ROOT.std.get[0](hp.altToys[i])
badToy = hp.generateAlt(seed)  # returns a hypoPoint
# Similarly: hp.generateNull(seed) for null toys

# badToy.data() returns a pair: (RooAbsData, RooArgSet)
#   .first  = the toy dataset (includes global observables since ROOT 6.28)
#   .second = legacy global observables set (now embedded in the dataset)

# Draw the model PDF with the bad toy data overlaid
w["pdfs/simPdf"].Draw()
ROOT.xRooNode(badToy.data().first).Draw("same")

# Fit the bad toy directly to inspect the fit result
fr = w["pdfs/simPdf"].nll(badToy.data().first).minimize()
fr.Print()  # inspect status code history — look for "3" (EDM above threshold)
```

### Common cause: negative bin predictions

A frequent cause of bad toys is **zero events in signal-dominated bins**. When
a toy fluctuates to 0 events in a bin where signal is the dominant contribution,
the fit may try to drive the signal strength negative. If there is no protection
against negative total bin predictions, the NLL becomes non-double-differentiable
at the point where the prediction equals zero, causing Minuit's EDM calculation
to fail (status=3).

**Symptoms:**
- High fraction of bad toys (NaN test statistic values)
- "Warning: post-Hesse edm ... > 10xMaxEDM" messages in the terminal
- status=3 codes in the fit result history (`fr.Print()`)
- Bad toys cluster in bins where signal is dominant and the toy has 0 events

### Diagnosing the discontinuity

To see the NLL discontinuity caused by negative predictions, add the bad toy
dataset to the workspace and perform a PLR scan:

```python
w.Add(badToy.data().first)  # add toy dataset to workspace
# or equivalently: w.datasets().Add(badToy.data().first)
w.Browse()  # select the added toy dataset, then scan "plr"
```

The PLR scan will show a kink or discontinuity at the POI value where the
total bin prediction crosses zero.

### Fixing negative predictions

**Option 1 (partial): RooRealSumPdf global floor**

```python
ROOT.RooRealSumPdf.setFloorGlobal(True)
```

This prevents negative predictions, but introduces a **derivative discontinuity**
at the floor boundary, which can still cause EDM problems.

**Option 2 (recommended): POI lower bound**

Set a small negative lower bound on the POI to keep it above the discontinuity:

```python
w.poi()[0].setRange(-1e-5, float('inf'))
```

**Important considerations:**
- Do **not** set the minimum at exactly 0 — the fit needs to be able to converge
  onto 0 for background-only (null hypothesis) fits
- Remove any finite upper bounds on the POI (e.g. user-set ranges like [-3, +3]).
  Finite boundaries cause Minuit to use parameter transformations internally
  (`lower - 1 + sqrt(x² + 1)`), which can introduce hidden distortions in the
  fit landscape
- Use `float('inf')` for no upper bound

### Complete toy validation with fixes applied

```python
import ROOT
w = ROOT.xRooNode("workspace.root")

# Fix POI range: small negative lower bound, no upper bound
w.poi()[0].setRange(-1e-5, float('inf'))

# Use Strategy(2) to avoid post-Hesse EDM warnings
nll_opts = [ROOT.RooFit.Strategy(2)]

# Asymptotic limit first
hs = w.nll("obsData", nll_opts).hypoSpace()
limits = hs.limits()
print(limits)

# Validate at observed limit with toys
hp = w.nll("obsData", nll_opts).hypoPoint(
    value=limits["obs"].value(), alt_value=0
)
hp.addAltToys(2000)
hp.addNullToys(2000)
print("Toy-based pCLs:", hp.pCLs_toys())
hp.Draw()  # compare toy distribution with asymptotic (dashed line)
```

## Limit-Setting Troubleshooting

### Common Failure Modes

1. **Scan range too large**: hypoPoints far from the data cause fits to fail
   (status=1, covariance forced positive-definite). Narrow the scan range.

2. **Problematic nuisance parameter**: use `constPars` to hold groups constant
   and isolate which NP causes failures. Use `w.pars().Print()` and
   `w.floats().Print()` to inspect.

3. **status=3 (EDM above threshold)**: increase Strategy to 2 when constructing
   the NLL: `w.nll("obsData", [ROOT.RooFit.Strategy(2)])`. If that alone is
   insufficient, also increase Tolerance (up to ~10).

4. **NaN limits**: print the hypoSpace (`hs.Print()`) to inspect which
   hypoPoints had non-zero status codes.

5. **High fraction of bad toys**: see [Debugging Bad Toys](#debugging-bad-toys)
   above. Usually caused by negative bin predictions in signal-dominated bins.
   Fix with a small negative POI lower bound and Strategy(2).

6. **POI range distortions**: avoid setting finite upper/lower bounds on the
   POI unless necessary. Minuit applies parameter transformations at boundaries
   which can distort the fit landscape. If a lower bound is needed (e.g. to
   prevent negative predictions), use a very small value like `-1e-5`.

### Limit-Setting Checklist

Before running limits, answer these questions:

- What are the hypoSpace parameters and their values/axes?
- What hypoPoints are being tested?
- What p-value type: pNull (CLs+b) or pCLs (CLs)?
- How are p-values interpolated across the hypoSpace? (xRooFit uses log-linear
  along POI axis)
- What PLR test statistic variant?
- Toys or asymptotic formulae?
- What is the uncertainty on each p-value?
- Did any fits fail?
- Does the POI have appropriate range bounds? (avoid tight finite bounds)
- Is Strategy(2) needed for toy stability?

## Discovery Significance

Scan a single point at the background-only hypothesis:

```python
hs = nll.hypoSpace("mu_sig", XRF.xRooFit.TestStatistic.u0)
hs.scan("pnull", 1, 0, 0)  # single point at mu=0

print("Observed p0:", hs[0].pNull_asymp())
print("Expected p0:", hs[0].pNull_asymp(0))   # under mu=1
print("Expected +1σ:", hs[0].pNull_asymp(1))
print("Expected -1σ:", hs[0].pNull_asymp(-1))

# Convert to significance
sig = ROOT.Math.gaussian_quantile_c(hs[0].pNull_asymp().value(), 1)
```
