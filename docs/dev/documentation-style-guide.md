# Documentation Style Guide

## Diátaxis Directory Layout

| Directory | Purpose | Style |
|-----------|---------|-------|
| `docs/tutorials/` | Learning-oriented | Second person ("you will…"). Guarantee success. State expected outcome at every step. |
| `docs/how-to/` | Task-oriented | Imperative mood. Titles always start with "How to". Assume the reader knows the basics; do not re-explain concepts. |
| `docs/reference/` | Lookup-oriented | Tables and lists only. No prose narrative. Completeness over readability. |
| `docs/explanation/` | Understanding-oriented | Discursive, opinionated allowed. Link to ADRs for specific decision history. |
| `docs/adr/` | Architecture Decision Records | Numbered (0001, 0002, …), immutable, append-only. Once Accepted, never edit; write a new ADR to supersede. |
| `docs/dev/` | Contributor docs only | Setup, testing, release. Never mix with user docs. |

**Language**: American English. Use *behavior*, *color*, *organize*, *categorized* — not behaviour, colour, organise, categorised.

Allowed root-level markdown: `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `LICENSE`, `SECURITY.md`, `CODE_OF_CONDUCT.md`.

## Routing — Where Does New Content Go?

Apply in order; stop at the first match:

1. Records a past architectural decision → `docs/adr/NNNN-<slug>.md`
2. Needed to set up dev environment or contribute → `docs/dev/`
3. Reader follows step-by-step to learn the system → `docs/tutorials/`
4. Reader is trying to accomplish a specific named task → `docs/how-to/`
5. Reader scans for a config key, API field, or CLI flag → `docs/reference/`
6. Explains why something works the way it does → `docs/explanation/`
7. One-minute pitch + minimal run command → `README.md`

If content seems to belong in two places, it is two documents — split it.

## When Code Changes Require Doc Updates (Same Commit)

| Change | Action |
|--------|--------|
| Public API, CLI flag, or config key changed | Update `docs/reference/` |
| New user-discoverable feature added | Add a how-to guide in `docs/how-to/` |
| Install, build, or run steps changed | Update `README.md` quick start + `docs/tutorials/01-getting-started.md` |
| Dev environment or test commands changed | Update `docs/dev/` |
| Architectural decision made | Write a new ADR in `docs/adr/` |
| User-visible behavior changed | Add entry under "Unreleased" in `CHANGELOG.md` |
| Pure internal refactor with no user-visible effect | No doc change needed |

## Required Documents

These files must exist and stay current. A PR that invalidates one without
updating it should not merge.

### `docs/dev/` — contributor onboarding

| File | Purpose |
|------|---------|
| `docs/dev/setup.md` | Build prerequisites, dependency layout, build/run commands |
| `docs/dev/testing.md` | How to run the suite, sanitizer builds, test ownership |
| `docs/dev/code-style.md` | Language versions, formatting, naming, design preferences |
| `docs/dev/documentation-style-guide.md` | This file |
| `docs/dev/maintenance.md` | Long-lived maintenance invariants and checklists |

### `docs/reference/` — lookup tables

| File | Purpose |
|------|---------|
| `docs/reference/configuration.md` | Every config key, defaults, reload behavior |
| `docs/reference/ipc-protocol.md` | TIP v1 protocol: methods, events, error codes |
| `docs/reference/engine-discovery.md` | Search path, naming rules, icon lookup |
| `docs/reference/glossary.md` | Canonical project terms with one-line definitions |

### `docs/adr/` — decision log

| File | Purpose |
|------|---------|
| `docs/adr/index.md` | Numbered table of all ADRs with status |
| `docs/adr/template.md` | Boilerplate for new ADRs |

### `docs/explanation/` — design context

| File | Purpose |
|------|---------|
| `docs/explanation/wayland-input-method.md` | How the host uses `zwp_input_method_v2` |
| `docs/explanation/panel-architecture.md` | Panel structure, layer boundaries, owner arbitration, anchors |
| `docs/explanation/frontend-graphics.md` | Render pipeline, flux independence argument |
| `docs/explanation/timing-model.md` | Keyboard timing, epochs, shortcut policy |
| `docs/explanation/lifecycle-resilience.md` | Crash recovery, state repair |
| `docs/explanation/control-surfaces.md` | Tray, D-Bus status, future settings UI |

## Writing Conventions

### Voice and tone

- **Second person** for tutorials and how-to guides. **Imperative mood** for
  how-to steps ("Run `ninja`", not "You should run `ninja`").
- **Third person** for explanation and reference ("the daemon", not "we" or
  "I").
- Be direct. Avoid hedging ("this might", "it could be said"). If something
  depends on conditions, name the conditions.
- Do not anthropomorphise: "the daemon forwards the key", not "the daemon
  decides to forward the key".

### Headings

- Exactly one `H1` per document — the page title.
- `H2` for sections, `H3` for subsections. Do not skip levels.
- **Descriptive noun phrases**: "Position Anchors", not "What About Position?"
  or "Understanding Anchors". How-to titles are the sole exception ("How to
  …").

### Inline formatting

| Element | Format |
|---------|--------|
| File paths, CLI flags, config keys, symbol names | `` `backtick` `` |
| Commands to run | Fenced code block with language tag |
| New term on first use | **Bold** + definition in the same sentence |
| Emphasis (rare) | *Italics* — prefer restructuring the sentence instead |

### Code blocks

- Always specify the language tag (`bash`, `c`, `toml`, `text`).
- Use `text` for pseudo-output or protocol examples.
- Split with prose if a block exceeds ~15 lines.

### Terminology

- Use canonical terms from `docs/reference/glossary.md`. No synonyms.
- First use: full term. Subsequent: short form acceptable if defined on first
  use ("Panel Coordinator (the Coordinator)").
- Code symbols (`TypioPanel`, `typio_panel_present`): always backticked, never
  abbreviated — they must stay greppable.
- Protocol names: official spelling (`zwp_input_popup_surface_v2`), no informal
  shortenings.

### Lists and tables

- **Numbered lists**: sequential steps.
- **Bullet lists**: unordered collections.
- **Tables**: comparison across consistent attributes.
- Every table must have a header row.

### Links

- Link text describes the target: "[Configuration Reference](…)", not
  "[here](…)".
- Relative paths for intra-repo links.
- Prefer `[topic](…)` over `[see topic](…)`.

## Cross-Reference Rules

- First mention of a concept in a document: link to its explanation or glossary
  entry. Subsequent mentions: no link needed.
- Explanation docs link to ADRs for decision history. ADRs link to explanation
  docs for background.
- How-to guides link to `docs/reference/` for option tables — never inline them.
- The glossary links back to the primary explanation doc or ADR for each term.
- Do not link from user-facing docs into `docs/dev/`.

## ADR Workflow

1. Copy `docs/adr/template.md` to `docs/adr/NNNN-<slug>.md` (next number from
   `docs/adr/index.md`).
2. Fill in Context, Decision, Alternatives, Consequences.
3. Add an entry to `docs/adr/index.md`.
4. Set status to `Accepted` when the decision is final.
5. To change a past decision, write a new ADR and set the old one's status to
   `Superseded by ADR-NNNN`.
6. If an ADR introduces or renames a term, update `docs/reference/glossary.md`
   in the same commit.

## Review Process

### PR structure

- Doc updates may be in the same PR as code or separate. Small config-key
  additions belong with their code; large new explanation docs are easier to
  review alone.
- If a PR touches code with a corresponding doc (see [Required
  Documents](#required-documents)), the description must state whether docs were
  updated — or why not ("pure internal refactor, no user-visible effect").

### Reviewer checklist

Any maintainer may review. Check before approving:

**Structure**

- [ ] File is in the correct Diátaxis category ([Routing](#routing--where-does-new-content-go)).
- [ ] No mixed user/contributor content.

**Accuracy**

- [ ] Statements match the current code.
- [ ] Symbol names, config keys, CLI flags, and protocol names are exact.
- [ ] Outdated information removed.

**Writing conventions**

- [ ] Headings: one H1, no skipped levels, noun phrases.
- [ ] Terminology canonical per glossary.
- [ ] Code blocks have language tags. Inline code is backticked.
- [ ] Link text is descriptive. No dangling references.

**Completeness**

- [ ] New terms added to glossary.
- [ ] New ADRs registered in `docs/adr/index.md`.
- [ ] Config/API changes reflected in `docs/reference/`.
- [ ] User-visible changes have a `CHANGELOG.md` "Unreleased" entry.

## Prohibitions

Quick-reference of hard rules defined in the sections above:

- No monolithic docs — split by Diátaxis category.
- No `README.md` quick-start duplication inside `docs/`.
- No design rationale in reference pages — move to explanation or ADR.
- No option tables in tutorials — link to reference.
- No editing Accepted ADRs — write a new one to supersede.
- No mixing user/contributor docs — `docs/dev/` is the firewall.
- No directory without an `index.md`.
- No new code term without a glossary entry.
- No dangling cross-references.
