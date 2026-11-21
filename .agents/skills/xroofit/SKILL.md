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

Use this version (commit hash or tag) to look up the documentation for that specific commit hash/tag in the repository. If the version is not specified or latest, use the master branch.

### Finding the documentation

The upstream canonical repository is `will-cern/xroofit`. The primary entry
point for all documentation is `docs/README.md`.

#### GitHub Integration Plugin

Use a **GitHub Integration plugin or MCP tool** (e.g. the
[GitHub plugin](https://claude.com/marketplace/plugins/github) installed in
Claude.ai, or any MCP server that exposes `get_file_contents`, `search_code`,
`sync_fork`, or equivalent GitHub tools) to read all repository content.

**Determining which repository to use:**

1. Use the GitHub plugin to inspect which repositories the authenticated user
   has access to. Look for a repository named `xroofit` (or a fork of
   `will-cern/xroofit`) owned by the authenticated user or their organisation.
2. If such a fork exists, use it as `<owner>/xroofit`. If no fork exists, fall
   back to the upstream `will-cern/xroofit`.

**Syncing the fork before reading docs:**

If you are using a fork (not the upstream), always sync it with upstream before
reading any documentation or source files. Use the plugin's sync/merge tool,
e.g.:
```
sync_fork(owner="<owner>", repo="xroofit", branch="master")
```
(or the equivalent `merge_upstream` / "Sync fork" call exposed by the plugin).
Only proceed to read files after the sync succeeds.

**Reading content:**

- **Primary Documentation Index**: `docs/README.md` at the appropriate ref
- **Topic Guides**: `docs/<filename>` as referenced in `docs/README.md`
- **Source / Tests**: `src/...` and `test/...` for implementation details

Concretely:
```
get_file_contents(owner="<owner>", repo="xroofit", path="docs/README.md", ref="<ref>")
```
When fetching topic guides or source files, substitute the appropriate `path`.
Use `search_code` (or equivalent) to locate implementation details when the
documentation is insufficient.

### Workflow

1. Determine the xRooFit version if it is relevant and not already known (you can ask the user which specific version of xRooFit they are using from `xRooFit::GetVersion()` or `ROOT::Experimental::XRooFit::xRooFit::GetVersion()`). Do not interrupt an otherwise answerable question just to ask for the version. If unspecified, assume `<ref>` is `master`.
2. Use the GitHub Integration plugin to identify the repository to use:
   - Check whether the authenticated user has a fork of `will-cern/xroofit`.
   - If yes: sync the fork with upstream (see above), then use the fork.
   - If no: use `will-cern/xroofit` directly.
3. Retrieve `docs/README.md` for the appropriate `<ref>` from the chosen repository.
4. Read the documentation index to discover the relevant documentation and topic guides for the question.
5. Fetch and read the corresponding topic guide (e.g. `docs/<filename>`) via the plugin.
6. If the documentation does not answer the question, inspect the implementation and tests in the repository (e.g. `src/...`) via the plugin.
7. Base code examples on documented APIs and existing examples.
8. Do not invent methods, arguments, classes or behaviour.
9. If the documentation and implementation disagree, explain
   the discrepancy rather than silently choosing one.

### Documentation access failures

If GitHub cannot be accessed, explicitly tell the user that
you could not verify the latest documentation.

Do not claim that an answer is based on the latest documentation
unless you actually accessed it during the current task.

You may still provide a tentative answer from existing knowledge,
but clearly distinguish this from a documentation-verified answer.
