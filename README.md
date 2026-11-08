# xRooFit

Extra tools for RooFit projects. Home of the xRooBrowser.

xRooFit provides a high-level Python and C++ API for ROOT's RooFit statistical analysis toolkit, simplifying model building, workspace manipulation, fitting, diagnostics, and hypothesis testing.

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.15095380.svg)](https://doi.org/10.5281/zenodo.15095380)

---

## AI Assistant Setup

This repository includes an AI assistant skill to equip coding assistants (such as Claude) with full awareness of the xRooFit API, workflows, and documentation.

### Claude.ai Chats

To use xRooFit within Claude.ai:

1. Download a copy of the [`.agents/skills/xroofit/SKILL.md`](.agents/skills/xroofit/SKILL.md) file from this repository.
2. In the Claude application, open **Customize**.
3. Select **Skills**, then click **+ Add** and **Upload skill**.
4. Locate and upload the downloaded `SKILL.md` file.
5. In your chat, ask questions about xRooFit using the `/xroofit` prefix (for example: `/xroofit How do I add a systematic variation to a sample?`).

### Other AI Environments

- **Antigravity / Agent CLI**: Agent rules and skills are configured in the [`.agents/`](.agents/) directory.
- **Plugins**: A manifest is available at [`.agents/plugin.json`](.agents/plugin.json) for plugin-compatible agent workflows.

---

## Documentation

Full documentation, guides, and tutorials are located in the [`docs/`](docs/) directory:

- [**Overview & Quickstart**](docs/README.md) — Core concepts, `xRooNode` navigation, canonical patterns, gotchas, and interop.
- [**Installation & Setup**](docs/installation.md) — StatAnalysis setup, Docker containers, compiling from source, and external CMake integration.
- [**Interactive Inspection & Browsing**](docs/inspection-and-browsing.md) — `xRooBrowser`, GUI exploration, node inspection, and visualization options.
- [**Workspace Building**](docs/workspace-building.md) — Building models from scratch, factor types, variations, constraints, and MC statistics.
- [**Advanced Fitting & Diagnostics**](docs/fitting-diagnostics.md) — NLL construction, minimization, MINOS/xMINOS, fit configuration, covariance quality, and impact/ranking.
- [**Advanced Hypothesis Testing**](docs/hypothesis-testing.md) — Profile likelihood scans, asymptotic and toy-based CLs limits, and discovery significance.
- [**Variation Decomposition**](docs/variationDecomposition.md) — Decomposing systematic variations across multiple nuisance parameters.
- [**External Documentation**](https://xroofit.readthedocs.io/) — ReadTheDocs reference.
