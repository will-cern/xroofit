## Variation Decomposition

In this example, we will take a systematic variation on a component (sample) and decompose it, meaning the sample will go from being a function of one nuisance parameter, theta, to being a function of two: theta_1 and theta_2. 

The transformation converts `s(theta) = s_nom + theta*delta_s` into `s(theta_1,theta_2) = s_nom + theta_1*f*delta_s + theta_2*sqrt(1-f^2)*delta_s` - the input to the problem is `f` which we will take to have the same observable-dependence as `s` (i.e. if `s` is binned in observable x then so is `f`)

### Problem setup
To demonstrate this we will start with a one-channel model with a single sample with two bins. The sample has a systematic variation on it, parameterized by "theta" nuisance parameter.

```python
ws = ROOT.RooWorkspace("w","w")
w = ROOT.xRooNode(ws)
w["simPdf/chan1"].SetXaxis("x","my obs",2,0,2) # two bin channel
w["simPdf/chan1/s"].SetBinContent(1,3)
w["simPdf/chan1/s"].SetBinContent(2,4)
w["simPdf/chan1/s"].SetBinContent(1,4,"theta")
w["simPdf/chan1/s"].SetBinContent(2,4.5,"theta")
w["simPdf"].pars()["theta"].Constrain("normal")
```

### Simplest solution: adding another variation

The most unsophisticated approach to the modification is to add a new variation to the sample, and then update the values of the variations accordingly. 

Suppose that f=0.5 in the first bin, and 0.1 in the second bin, then we could do:

```python
import math
f_vals = [0.5,0.1]
w["simPdf/chan1/s"].Vary("theta_2=1") # add a new variation
for i in range(0,w["simPdf/chan1"].GetXaxis().GetNbins()):
    nom = w["simPdf/chan1/s/nominal"].GetBinContent(i+1) # returns nominal value
    up = w["simPdf/chan1/s/theta=1"].GetBinContent(i+1) - nom
    w["simPdf/chan1/s/theta=1"].SetBinContent(i+1,nom + up*f_vals[i]) # modify existing variation
    w["simPdf/chan1/s/theta_2=1"].SetBinContent(i+1,nom + up*math.sqrt(1.-f_vals[i]*f_vals[i]))
w["simPdf"].pars()["theta_2"].Constrain("normal")
```

### More sophisticated solution - using RooFit methods:

We start off similarly by extending our sample with another variation and constraining the parameter:

```python
w["simPdf/chan1/s"].Vary("theta_2=1") # add a new variation
w["simPdf"].pars()["theta_2"].Constrain("normal")
```

Printing the sample node reveals it's a `PiecewiseInterpolation` object type:

```python
w["simPdf/chan1/s"].Print()

w/simPdf/channelCat=chan1/samples/s: PiecewiseInterpolation::simPdf_chan1_samples_s
0) nominal : RooHistFunc::simPdf_chan1_samples_s_nominal
1) theta=1 : RooHistFunc::simPdf_chan1_samples_s_nominal_theta_up
2) theta=-1 : RooHistFunc::simPdf_chan1_samples_s_nominal_theta_down
3) theta_2=1 : RooHistFunc::simPdf_chan1_samples_s_nominal_theta_2_up
4) theta_2=-1 : RooHistFunc::simPdf_chan1_samples_s_nominal_theta_2_down

```
Now we do some trickery. We start by scaling one of variations by a new histo factor we call 'f'

```python
w["simPdf/chan1/s/theta=1"].Multiply("f","histo")
```

That's created a histo factor with the same binning. Now we make some expressions:
```python
l = ROOT.RooArgList();
l.add( w.factory("expr::simPdf_chan1_samples_s_theta_up_func('@0 + (@1 - @0)*@2',simPdf_chan1_samples_s_nominal, simPdf_chan1_samples_s_nominal_theta_up, f)") )
l.add( w.factory("expr::simPdf_chan1_samples_s_theta_down_func('@0 + (@1 - @0)*@2',simPdf_chan1_samples_s_nominal, simPdf_chan1_samples_s_nominal_theta_down, f)") )
l.add( w.factory("expr::simPdf_chan1_samples_s_theta2_up_func('@0 + (@1 - @0)*sqrt(1.-@2*@2)',simPdf_chan1_samples_s_nominal, simPdf_chan1_samples_s_nominal_theta_up, f)") )
l.add( w.factory("expr::simPdf_chan1_samples_s_theta2_down_func('@0 + (@1 - @0)*sqrt(1.-@2*@2)',simPdf_chan1_samples_s_nominal, simPdf_chan1_samples_s_nominal_theta_down, f)") )
```

And now we substitute these expressions in place of the functions already there.
To do this you decoracte the name of the thing they are replacing as an attribute:

```python
l[0].setAttribute("ORIGNAME:prod_simPdf_chan1_samples_s_nominal_theta_up")
l[1].setAttribute("ORIGNAME:simPdf_chan1_samples_s_nominal_theta_down")
l[2].setAttribute("ORIGNAME:simPdf_chan1_samples_s_nominal_theta_2_up")
l[3].setAttribute("ORIGNAME:simPdf_chan1_samples_s_nominal_theta_2_down")
w["simPdf/chan1/s"].redirectServers(l,False,True)
```

Now everything is replaced and has become a function of "f". You can find "f" inside any of the variation nodes, and can modify its bin contents as usual:

```python
w["simPdf/chan1/s/theta=1/f"].SetBinContent(1,0.5)
w["simPdf/chan1/s/theta=1"].GetBinContent(1) # see the effect of changing f value
```



