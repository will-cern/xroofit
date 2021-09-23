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

