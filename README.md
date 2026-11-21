# xRooFit

Extra tools for RooFit projects. Home of the xRooBrowser.

xRooFit provides a high-level Python and C++ API for ROOT's RooFit statistical analysis toolkit, simplifying model building, workspace manipulation, fitting, diagnostics, and hypothesis testing.

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.15095380.svg)](https://doi.org/10.5281/zenodo.15095380)

---

## AI Technical Support (Claude.ai)

This repository is set up for **AI-powered technical support** via Claude.ai.
When the repository is connected to a Claude.ai chat session through the GitHub
Integration, Claude will automatically load the xRooFit documentation and skill
definitions from this repository and act as an informed xRooFit assistant.

### Setup instructions for Claude.ai

Follow these steps once to enable full AI technical support in any new chat:

#### Step 1 — Install the GitHub Integration

1. In Claude.ai, open the **Plugins** (or **Integrations**) panel.
2. Find and install the **GitHub Integration** plugin.
3. When prompted to add a GitHub connector and asked about OAuth credentials,
   use **Claude's own published identity** (the default OAuth app provided by
   Anthropic) — do not create or paste your own GitHub OAuth credentials.
4. Authorise the integration to access your GitHub account when prompted.

#### Step 2 — Fork this repository

1. Go to [github.com/will-cern/xroofit](https://github.com/will-cern/xroofit).
2. Click **Fork** (top-right) and create a fork under your own GitHub account.

#### Step 3 — Add the fork to your Claude.ai chat

1. In a Claude.ai chat window, click the **+** button in the message input area.
2. Select **Add from GitHub**.
3. Choose your fork of `xroofit` from the list and add it to the chat.

Claude will now automatically read the agent instructions and skill definitions
from your fork and greet you as an xRooFit technical support assistant.

> **Tip — ensure write access for automatic updates:**
> To allow Claude to sync your fork with the latest upstream documentation
> automatically, grant the GitHub Integration **write access** to your fork.
> You can do this in one of two ways:
> - In Claude.ai: open **Settings** → **Integrations** → **GitHub** and review
>   the repository permissions.
> - On GitHub: go to **Settings** → **Integrations** → **Applications**, find
>   the Claude application, and check which repositories it has access to,
>   adding your fork if it is missing.

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
