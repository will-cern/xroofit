---
name: xroofit
description: >-
  Use when building RooFit statistical models with xRooFit, constructing
  workspaces with channels and samples, adding systematics or nuisance
  parameters, fitting models to data, constructing NLL functions, diagnosing fit
  convergence failures (status codes, covariance quality), computing profile
  likelihood ratios, calculating CLs upper limits or exclusion contours, or
  computing discovery significance.
---

# xRooFit

xRooFit is a high-level Python/C++ API for the RooFit statistical analysis
toolkit, with the same relationship to RooFit as Keras has to TensorFlow: RooFit
can be used directly, but xRooFit makes building and fitting models
significantly more concise. xRooFit ships pre-installed with ROOT ≥ 6.30 and is
also distributed as part of the ATLAS
[StatAnalysis](https://gitlab.cern.ch/atlas/StatAnalysis) release.

xRooFit is commonly used for:
- Building HistFactory-style statistical models programmatically with Python or C++ as an alternative to TRExFitter config files or HistFactory XML
- Manipulating or inspecting workspaces produced by TRExFitter or HistFitter
- Performing CLs upper limits, exclusion contours, or discovery significance tests using asymptotic formulae or toys on a RooFit workspace
- Diagnosing fit convergence problems via status codes and covariance quality
- Advanced statistical analysis with ROOT/RooFit performance and direct RooStats infrastructure access

## Mandatory documentation-first workflow

You MUST consult the latest xRooFit documentation on GitHub
before answering any question about xRooFit.

The documentation in the GitHub repository is the authoritative
source of truth. Do not rely on your pretrained knowledge,
previous conversations, cached documentation, or examples in
this skill when determining the current xRooFit API or behaviour.

### Determining the xRooFit version

Ask the user what specific version of xRooFit they are using, which they can obtain by running:
- `xRooFit::GetVersion()` in the ROOT prompt (or `import ROOT; print(ROOT.xRooFit.GetVersion())` in Python)
- `ROOT::Experimental::XRooFit::xRooFit::GetVersion()` if using the version of xRooFit bundled with ROOT

Use this version (commit hash or tag) to look up the documentation for that specific commit hash/tag in the repository (e.g. `https://github.com/will-cern/xroofit/tree/<commit-or-tag>`). If the version is not specified or latest, use the master branch.

### Finding the documentation

The canonical repository is:

https://github.com/will-cern/xroofit

The primary entry point for all documentation is `docs/README.md` in the repository:

https://github.com/will-cern/xroofit/blob/master/docs/README.md

Consult `docs/README.md` to discover all available topic guides, core concepts, and canonical patterns.

Always use the version corresponding to the user's specific commit hash/tag, or the latest version from the master branch if unspecified.

### Workflow

1. Determine the xRooFit version if it is relevant and not already known (you can ask the user which specific version of xRooFit they are using from `xRooFit::GetVersion()` or `ROOT::Experimental::XRooFit::xRooFit::GetVersion()`). Do not interrupt an otherwise answerable question just to ask for the version. If unspecified, assume master.
2. Access `docs/README.md` in the GitHub repository for that specific commit hash/tag (or master branch if latest/unspecified) to discover the relevant documentation for the question.
3. Read the relevant documentation before formulating an answer.
4. Follow links to additional documentation when necessary.
5. If the documentation does not answer the question, inspect
   the implementation and tests in the repository.
6. Base code examples on documented APIs and existing examples.
7. Do not invent methods, arguments, classes or behaviour.
8. If the documentation and implementation disagree, explain
   the discrepancy rather than silently choosing one.

### Documentation access failures

If GitHub cannot be accessed, explicitly tell the user that
you could not verify the latest documentation.

Do not claim that an answer is based on the latest documentation
unless you actually accessed it during the current task.

You may still provide a tentative answer from existing knowledge,
but clearly distinguish this from a documentation-verified answer.
