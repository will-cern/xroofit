# Workspace Building Guide

This guide covers building HistFactory-style statistical models from scratch using xRooFit in Python or C++, including channels, samples, factors, systematic variations, constraints, and datasets.

## Building from Scratch: Complete Walkthrough

Here is a complete example creating a workspace with two channels (`CR` and `SR`), two samples (`sig` and `bkg`), a floating signal normalization factor (`mu`), a systematic variation constrained by a nuisance parameter (`alpha`), and observed data:

```python
import ROOT

# 1. Create a RooWorkspace and wrap it in an xRooNode
ws = ROOT.RooWorkspace("w", "w")
w = ROOT.xRooNode(ws)

# 2. Define channels and observable binning with SetXaxis
# (xRooNode automatically creates the top-level model "simPdf" and channels)
w["simPdf/CR"].SetXaxis("myObs", 2, 0, 2)       # 2 bins in range [0, 2]
w["simPdf/SR"].SetXaxis("anotherObs", 1, 0, 5)  # 1 bin in range [0, 5]

# 3. Set background sample contents
w["simPdf/CR/bkg"].SetBinContent(1, 4)
w["simPdf/CR/bkg"].SetBinContent(2, 5)
w["simPdf/SR/bkg"].SetBinContent(1, 3)

# 4. Add a systematic variation on bkg in SR (sets content=4 when alpha=1)
w["simPdf/SR/bkg"].SetBinContent(1, 4, "alpha", 1)

# 5. Add signal sample and multiply by a floating norm factor (mu)
w["simPdf/SR/sig"].SetBinContent(1, 1)
w["simPdf/SR/sig"].Multiply("mu", "norm")

# 6. Add a Gaussian constraint for the nuisance parameter alpha
w["simPdf"].pars()["alpha"].Constrain("gaussian(0,1)")

# 7. Add observed data to SR
w["simPdf/SR"].SetBinData(1, 4)

# 8. Inspect the resulting workspace structure
w.Browse()
```

### Understanding the Model Architecture

Behind the scenes, xRooFit constructs standard RooFit/HistFactory classes:

1. **Top-Level Model (`w.Add("simPdf", "model")`)**: Creates a `RooSimultaneous` PDF. It defines a category observable (`channelCat`) whose states correspond to the individual channels (`w["simPdf"].obs().Print()`).
2. **Channels (`w["simPdf"].Vary("CR")`)**: Each channel is represented by a `RooProdPdf`.
3. **Observables (`SetXaxis`)**: Sets the coordinate axis for the channel. Calling `SetXaxis` on a non-existent path automatically creates the parent model and channel!
4. **Samples (`w["simPdf/CR"].Add("bkg", "sample")`)**: Samples are placed inside a `samples` node, which is a `RooRealSumPdf` summing the yields of each sample.
5. **Observed Data (`SetBinData`)**: Adds observed counts to the channel's dataset (`obsData`).

```python
# Equivalent ways to set and draw observed data:
w["simPdf/CR"].SetBinData(1, 2)
w["simPdf/CR"].datasets()["obsData"].SetBinContent(1, 2)
w["simPdf/CR"].datasets()["obsData"].Draw("same")
```

---

## Declaring Observables with SetXaxis

Instead of using a histogram to declare a channel's observable, use `SetXaxis`
directly:

```python
# Fixed bin widths
w["pdfs/simPdf/SR"].SetXaxis("obsName", "obs title", nBins, low, high)

# Variable bin widths (using ROOT.std.vector or list of bin edges)
w["pdfs/simPdf/SR"].SetXaxis("obsName", "obs title", nBins, binEdgesArray)
```

Both approaches create an initial `SimpleDensity` factor for the sample.

---

## Factor Types

Factors are the building blocks of samples. Two categories: observable-dependent
and observable-independent. Conventionally, observable-independent factors
become the **coefficients** of a sample (`coefs()`), while observable-dependent
factors remain inside the sample itself.

### Basic Factor Types

| Type    | Obs-dependent | Parameterized | RooFit class          | Description                                    |
| ------- | ------------- | ------------- | --------------------- | ---------------------------------------------- |
| Const   | No            | No            | `RooConstVar`         | Pre-specified constant                         |
| Norm    | No            | Yes           | `RooRealVar`          | Floatable normalization parameter              |
| Simple  | Yes           | No            | `RooHistFunc`         | Histogram of bin yields                        |
| Density | Yes           | No            | `RooBinWidthFunction` | Special Simple factor where value = 1/binWidth |
| Shape   | Yes           | Yes           | `ParamHistFunc`       | Per-bin individual norm factors                |

### Varied Factor Types

Factors can be made parameter-dependent by defining **variations** at points in
a variation space with interpolation/extrapolation rules.

| Type    | Description                                       | RooFit class                                    |
| ------- | ------------------------------------------------- | ----------------------------------------------- |
| Varied  | General parameterized factor with variations      | `PiecewiseInterpolation`                        |
| Overall | Varied factor where variations are const factors  | `FlexibleInterpVar` or `PiecewiseInterpolation` |
| Histo   | Varied factor where variations are simple factors | `PiecewiseInterpolation`                        |
| Func    | Generic parametric function                       | `RooFormulaVar`                                 |

### Defining Parameterized Variations

Variations can be defined on `"histo"` and `"overall"` factors in two equivalent ways:

```python
# 1. Via Vary() then SetBinContent:
w["simPdf/CR/samples/bkg/factorName"].Vary("alpha=1").SetBinContent(1, 2)

# 2. Directly specifying the parameter and value in SetBinContent:
w["simPdf/CR/samples/bkg/factorName"].SetBinContent(1, 2, "alpha", 1)
```

This transforms the factor into a function of `alpha` (created on the fly if needed), taking on the specified value when `alpha = 1`. Nominal parameter values correspond to `alpha = 0`.

### Creating and Multiplying Factors

```python
# Observable-dependent factor
w["pdfs/simPdf/SR/samples/bkg"].Multiply("myFactor", "shape")

# Observable-independent (coefficient)
w["pdfs/simPdf/SR/samples/bkg"].coefs().Multiply("mu_bkg", "norm")
```

If a factor with the same name already exists in the workspace, the type
argument is ignored and the existing factor is reused. This enables factor
sharing across samples and channels.

## Interpolation and Extrapolation Codes

Varied factors use an interpolation code to determine how the factor is computed
between and beyond the defined variation points.

For a varied factor with nominal variation f₀(x) and up/down variations
f*{i+}(x)/f*{i−}(x) for parameter θᵢ:

- **Additive** codes: f(x|θ) = f₀(x) + Σᵢ I(θᵢ; f*{i−}, f₀, f*{i+})
- **Multiplicative** codes: f(x|θ) = f₀(x) · Πᵢ I(θᵢ; f*{i−}/f₀, 1, f*{i+}/f₀)

| Code | Name                                            | Recommended for                                         |
| ---- | ----------------------------------------------- | ------------------------------------------------------- |
| 0    | Additive piecewise linear                       | **Not recommended** (derivative discontinuities)        |
| 1    | Multiplicative piecewise exponential            | **Not recommended**                                     |
| 4    | Additive poly interp + linear extrap            | **Histo factors**                                       |
| 5    | Multiplicative poly interp + exponential extrap | **Overall factors** (interpCode=4 in FlexibleInterpVar) |
| 6    | Multiplicative poly interp + linear extrap      | Norm factors that must not have roots outside \|θ\|<1   |

Codes 4 and 5 use a 6th-order polynomial in the \|θ\|<1 region, matched to
0th/1st/2nd derivatives at the boundaries.

### Checking and Changing Interpolation Codes

```python
w["pdfs/simPdf/SR/samples/sampleName/sampleName"].printAllInterpCodes()
w["pdfs/simPdf/SR/samples/sampleName/sampleName"].setAllInterpCodes(4)
```

## Factor vs Sys

When any parameter of a parameter-dependent factor also has a **constraint
term**, the word "factor" is replaced by "sys":

- ShapeFactor → ShapeSys
- NormFactor with constraint → NormSys (effectively)

Promote a factor to a sys by adding a constraint:

```python
w["pdfs/simPdf"].pars()["npName"].Constrain("gaussian(0,1)")
w["pdfs/simPdf"].pars()["npName"].Constrain("normal")  # alias
```

### Supported Constraint Types

- `gaussian(x,y)`: `globs` value = `x`, standard deviation = `y`, mean = parameter
- `normal`: equivalent to `gaussian(0,1)`
- `poisson(x)`: `globs` value = `x`, mean = `parameter * x`

Inspect created variables and constraint PDFs:
```python
w["simPdf"].vars().Print()
w["simPdf/channelName"].Print()
```

### Log-normal Constraints

Replace the parameter with its exponentiated version, then apply a normal
constraint:

```python
w["pdfs/simPdf"].pars()["npName"].Replace(
    "expr::expo_npName('exp(npName)', npName)"
)
w["pdfs/simPdf"].pars()["npName"].Constrain("normal")
```

## MC Statistical Uncertainties

When a histogram with non-zero bin errors is added as a sample, xRooFit
automatically creates a **ShapeSys** with Poisson-constrained γ parameters for
MC statistical uncertainties. This ShapeSys is nominally shared between all
samples in the channel.

### Controlling MC Stat Behavior

- **Suppress**: zero out bin errors before adding the histogram
- **Control prefix**: `hBkg.SetOption("statPrefix=stat_SR_bkg")` (default
  prefix: `stat_<channelName>`)
- **Separate per sample**: any sample with its own NormFactor should have its
  own separate ShapeSys (or none) rather than sharing, because the normalization
  factor is not accounted for in the Poisson constraint calculation otherwise

### Variation Histograms

Variation histograms passed to `Vary()` **must have zero bin errors**. Non-zero
bin errors on variations create "errors-on-errors" which are not fully
supported.

## Creating Datasets

### From Histogram

```python
hData.SetName("obsData")
w["pdfs/simPdf/channelName"].datasets().Add(hData)
```

### Bin-by-Bin

```python
w["pdfs/simPdf/channelName"].SetBinData(binNumber, value, "obsData")
```

The dataset name defaults to `"obsData"` if unspecified.

### Generating Toy or Asimov Datasets from a PDF

Datasets can also be generated programmatically from the model itself, without
providing real data. See the **Generating Datasets** section in
[fitting-diagnostics.md](fitting-diagnostics.md) for the full API.

In brief:

```python
toyDS  = w["pdfs/simPdf"].generate(expected=False)  # toy (Poisson-fluctuated)
asiDS  = w["pdfs/simPdf"].generate(expected=True)   # Asimov (expected values)
nll    = w["pdfs/simPdf"].nll(asiDS)                # pass node directly
```

### Saving the workspace

The workspace can be saved to disk with:

```python
w.SaveAs("workspace.root")
```
