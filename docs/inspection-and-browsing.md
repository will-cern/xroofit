# Interactive Inspection and Workspace Browsing

xRooFit provides extensive tools for inspecting, navigating, visualizing, and interactively browsing RooFit workspaces.

## Interactive Browsing: xRooBrowser

The fastest way to explore workspaces is with the interactive `xRooBrowser` GUI.

### In Python or ROOT Prompt

```python
import ROOT

# Option 1: Browse a wrapped workspace node
w = ROOT.xRooNode(ws)
w.Browse()

# Option 2: Browse a workspace file directly
w = ROOT.xRooNode("workspace.root")
w.Browse()

# Option 3: Launch xRooBrowser across all open ROOT files
f = ROOT.TFile.Open("workspace.root")
b = ROOT.xRooBrowser()
```

### Running xRooBrowser via Docker

If running without a local ROOT installation, define the following shell function:

```bash
xRooBrowser() {
    xhost +${hostname}
    docker run --rm --pull always -v "$1":/"$(basename "$1")" \
        -e DISPLAY=host.docker.internal:0 -it \
        gitlab-registry.cern.ch/atlas/statanalysis:0-2 \
        root -e 'xRooBrowser b; b.Open("'$(basename "$1")'")'
}
```

Then open any `.root` or `.json` workspace file:

```bash
xRooBrowser myWorkspace.root
```

---

## Command-Line Inspection and Navigation

`xRooNode` acts as a smart wrapper around RooFit objects, providing intuitive path-based navigation and inspection methods.

### Printing Node Hierarchies

```python
w.Print()                    # List all children of the workspace
w["simPdf"].Print()          # Access child 'simPdf' and print its children
w["simPdf/chan1"].Print()    # Access child 'chan1' inside 'simPdf'

# Control inspection depth with depth=X
w.Print("depth=2")
```

> **Note on Folder Paths**: In the GUI browser, objects are categorized into virtual folders (such as `pdfs`, `samples`, `factors`). While xRooFit supports both `w["pdfs/simPdf/SR/samples/bkg"]` and `w["simPdf/SR/bkg"]`, virtual folders are organizational aids—child objects can be accessed directly by name within their parent node.

### Finding and Filtering Nodes

- **`find("name")` or `operator[]("name")`**: Return the child with the given name or path.
- **`reduced("regexp1,regexp2")`**: Returns a shallow-copy subset node containing only matching children:
  ```python
  # Only background samples in the SR channel
  w["simPdf/SR/samples"].reduced("bkg*").Print()
  ```

---

## Node Visualization (`Draw`)

The `Draw()` method visualizes pdfs, channels, and samples. Passing options customizes the layout:

```python
# Basic visualization of channel or model
w["simPdf/SR"].Draw()

# Overlay observed data
w["simPdf/SR"].datasets()["obsData"].Draw("same")
```

### Draw Options

| Option | Description |
| ------ | ----------- |
| `E` | Adds error bars based on parameter uncertainties (or currently loaded fit result) |
| `RATIO` | Adds a ratio pad comparing data to model prediction |
| `SIGNIFICANCE` | Adds a pad displaying per-bin statistical significance of data deviations |
| `PULL` | Adds an interactive pull plot to investigate parameter pulls and dependencies |

```python
w["simPdf/SR"].Draw("E RATIO")
```

---

## Complete `xRooNode` API Reference

Methods of `xRooNode` are categorized by their functionality:

### 1. Graph Modifiers
Modify the computational graph representing the probability density and likelihood:
- `Add(child [, type])`: Add a model, channel, sample, or factor.
- `Multiply(name, type)`: Multiply a sample or node by a factor (`norm`, `overall`, `histo`, `shape`).
- `Vary(variation)`: Add an interpolation variation (e.g. `Vary("alpha=1")` or add a channel).
- `Constrain(pdfType)`: Add a parameter constraint (e.g. `gaussian(x,y)`, `normal`, `poisson(x)`).
- `Remove(child)`: Remove a child node from the graph.
- `Combine(otherNode)`: Combine independent channels or models.

### 2. Object Modifiers
Directly modify properties, binning, or contents of the underlying wrapped object:
- `SetXaxis(name, title, nBins, low, high)`: Set fixed-width observable binning.
- `SetXaxis(name, title, nBins, binsArray)`: Set variable-width observable binning.
- `SetBinContent(bin, value [, parName, parVal])`: Set nominal or varied bin content.
- `SetBinError(bin, value)`: Set bin statistical uncertainty (triggers MC-stat ShapeSys if non-zero).
- `SetBinData(bin, value [, dsName])`: Set bin count for an observed dataset (defaults to `"obsData"`).

### 3. Related Nodes
Query collections of related nodes in the workspace:
- `components()`: Nodes that add together to form this node.
- `factors()`: Nodes that multiply together to form this node.
- `variations()`: Variation nodes interpolated between to form this node.
- `constraints()`: Constraint pdfs applied to this parameter node.
- `datasets()`: Datasets corresponding to this pdf/model node.
- `vars()`: All leaf nodes that this node depends on (observables + parameters).
- `obs()`: All observable leaf nodes (`robs()` + `globs()`).
- `robs()`: Regular observable leaf nodes (columns in the dataset).
- `globs()`: Global observable leaf nodes (auxiliary measurements / nominal constraint values).
- `pars()`: All parameter leaf nodes (non-observables).
- `floats()`: Currently floating parameter subset.
- `consts()` (or `args()`): Currently constant parameter subset.
- `coefs()`: Coefficients that scale this sample within its parent channel.
- `coords()`: Coordinate observables defining this channel (e.g., `channelCat` category value).

### 4. Query and Extraction Methods
Extract numerical values, errors, and integrals:
- `GetBinContent(bin)`: Bin value of this node.
- `GetBinData(bin [, dsName])`: Bin value in dataset (equivalent to `datasets()[dsName].GetBinContent(bin)`).
- `GetBinError(bin [, fitResult])`: Bin uncertainty using parameter errors or covariance from `fitResult`.
- `IntegralAndError([fitResult])`: Return `(integral, error)` pair for the node, propagated through `fitResult` covariance.
- `GetXaxis()`: Return the `TAxis` representing the observable coordinate.

### 5. Fitting and Likelihood Methods
- `nll("dataset" [, options])`: Construct an `xRooNLLVar` likelihood function.
- `SetFitResult(fitResult)`: Load a fit result into the model, updating parameter values and setting the covariance matrix for error propagation.

