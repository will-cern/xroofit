# xRooFit

Extra tools for RooFit projects.

## Setup

Ensure you have a recent ROOT release setup (e.g. 6.22 or 6.24). Also ensure you have cmake available. For ATLAS users you can get both of these with

```asm
lsetup "views LCG_100 x86_64-centos7-gcc8-opt"
```

checkout the project (using your favourite git clone method), and then compile it like this:

```asm
mkdir build; cd build; cmake ../xroofit
make -j
```

Then you just need to ensure the library is available in your environment path variables, a setup script is provided that you can source (or you can just run root from the build directory):

```asm
source setup.sh
```
xRooFit works in both c++ and python, with the experience in python being particularly pleasant as you don't have to think about object types.

### Using xRooNode

The `xRooNode` class is designed to wrap over an existing TObject and provide functionality to aid with interacting with that object. It is a smart pointer to the object, so you have access to all the methods of the object too.

As a quick example, suppose you wish to create a workspace with two channels, CR and SR, with two and one bin respectively, and two components, sig and bkg, where bkg has a systematic uncertainty on it that will be represented by a nuisance parameter (alpha) and sig is scaled by a floating normalization factor (mu), then here's some code to create that (in python):

```python
ws = ROOT.RooWorkspace("w","w") # create a new workspace
w = ROOT.xRooNode(ws) # wrap it in an xRooNode to get extra functionality

# name of the top-level pdf will be "simPdf"
w["simPdf/CR"].SetXaxis("myObs",2,0,2) # two bins in range 0-2
w["simPdf/SR"].SetXaxis("anotherObs",1,0,5) # 1 bin in range 0-5

# now set contents of components
w["simPdf/CR/bkg"].SetBinContent(1,4)
w["simPdf/CR/bkg"].SetBinContent(2,5)
w["simPdf/SR/bkg"].SetBinContent(1,3)
w["simPdf/SR/bkg"].SetBinContent(1,4,"alpha",1) # sets content=4 when alpha=1

w["simPdf/SR/sig"].SetBinContent(1,1)
w["simPdf/SR/sig"].Multiply("mu","norm") # multiplies by a normFactor called mu

w["simPdf"].pars()["alpha"].Constrain("gaussian(0,1)") # add gaus constraint for alpha

w["simPdf/SR"].SetBinData(1,4) # example of setting data

w.Browse() # explore what you've created
```

The methods of `xRooNode` can be split into the following categories:

  * Graph Modifiers: Methods that alter the 'graph' representing the likelihood function
    * Add(...)
    * Multiply(...)
    * Vary(...)
    * Constrain(...)
    * Remove(...)
    * Combine(...)
    * Reduce(...)
  * Object Modifiers: Modify the object that the node wraps (or potentially one of the objects of the child nodes)
    * SetBinContent(bin, value [,parName, parVal] )
    * SetBinError(bin, value)
    * SetBinData(bin, value [,dsName])
    * SetXaxis(...)
  * Related nodes: these methods return the collection of nodes related to this node in some way:
    * components(): the nodes that "add" together to make this node
    * factors(): the nodes that "multiply" together to make this node
    * variations(): the nodes that are "varied" (interpolated) between to make this node
    * constraints(): the nodes that "constrain" this node (relevant for parameter nodes)
    * datasets(): the nodes that represent data corresponding to this node (relevant for pdf nodes)
      <br><br>
    * deps(): the fundmanental (leaf) nodes that this node depends on (=obs()+pars())
    * obs(): the leaf nodes that are observables
    * globs(): the leaf nodes that are global observables (subset of observables)
    * pars(): the leaf nodes that are parameters (i.e. not observables)
    * vars(): the parameters that are not constant and so would float in a fit
    * args(): the parameters that are currently constant
  * Inspection methods: tell you about the node
    * Print(): lists the child nodes (components/factors/variations) of a node
    * Draw(): Visualize the node
    
Starting from an empty workspace, how can you start to build up a likelihood?

```python
ws = ROOT.RooWorkspace("w","w");w = ROOT.xRooNode(ws)
```

What can we do with a workspace object? We can `Add` things to it. We can `Add` a new model like this:

```python
w.Add("simPdf","model")
```

The node accessed by `w["simPdf"]` is a `RooSimultaneous`, which is the roofit object designed to handle pdfs that depend on a category observable. Essentially, it allows you to have different channels where the value of the category labels which channel you are in. You can see that we've created a category observable by doing:

```python
w["simPdf"].obs().Print()
```

You add a channel to the model by 'varying' it:

```python
w["simPdf"].Vary("CR")
```

You've now made a channel called CR, which is represented by a `RooProdPdf`, i.e. a channel is a product of PDF objects. (see `w["simPdf/CR"].Print()` to confirm it's a `RooProdPdf`).

At this point it's a good idea to declare what your observable is for this channel. You do this with `SetXaxis` method:

```python
w["simPdf/CR"].SetXaxis("my observable",5,0,5) # can use over TH1-like methods for binnings (e.g. variable bin widths)
```

We normally think about 'adding samples' to a channel. But this is a `RooProdPdf` ... which we would normally think of as being something that can get multiplied by a pdf node. But we can 'Add' to a RooProdPdf a sample:

```python
w["simPdf/CR"].Add("bkg","sample")
```

This gets added inside of a `samples` node (which is a `RooRealSumPdf`, so we've satisfied the requirement that `RooProdPdf`'s children are pdf objects). We can now modify its content:

```python
w["simPdf/CR/samples/bkg"].SetBinContent(1,2)
```

We can visualize what we have so far:

```python
w["simPdf"].Draw()
```

We can carry on adding samples to our channel, or add new channels and add samples to those channels. We can also add data to a channel with:

```python
w["simPdf/CR"].SetBinData(1,2)
```

Samples can be modified by multiplying them by various types of factor, or by varying them. The modifying factors are included with commands like:

```python
w["simPdf/CR/samples/bkg"].Multiply("factorName","type")
```

where `"factorName"` is any uniquely-identifying name for the factor (note that factors can be shared between samples, just give them the same name), and `"type"` is one of the following types:

  * "norm": floating scale factor
  * "overall": parameterized (in nuisance params) scale factor
  * "histo": histogram factor (parameterized in x-observable)
  * "shape": histogram factor with each bin having a floating scale factor

You should think of each sample as initially being a single factor of the `"histo"` type, and you can multiply it by other types of factor.

The `"overall"` (behaves like it only has 1 bin) and `"histo"` factor types can be Varied. There are two ways to do this:

```python
w["simPdf/CR/samples/bkg/factorName"].Vary("alpha=1").SetBinContent(1,2)
w["simPdf/CR/samples/bkg/factorName"].SetBinContent(1,2,"alpha",1)
```

This makes the factor become a function of the parameter (`alpha` in this case, which is created on-the-fly if necessary), taking on the given value in the given bin when the parameter value equals 1. The *nominal* parameter values are taken to be when the parameter equals 0. 

Once you have created parameterized variations you can decide to add a constraint term for that parameter. This introduces you to `Constrain` method:

```python
w["simPdf"].pars()["alpha"].Constrain("gaussian(0,1)")
```

This will create a gaussian pdf and add it to every channel that the `alpha` parameter appears in. You can see this pdf in:

```python
w["simPdf/CR"].Print()
```

The types of constraint you can have are:

  * gaussian(x,y): globs_value=x, std.dev=y, mean = par
  * normal: == gaussian(0,1)
  * poisson(x): globs_value = x, mean = par*x

We can see all the dependents we have created so far with:

```python
w["simPdf"].deps().Print()
```

