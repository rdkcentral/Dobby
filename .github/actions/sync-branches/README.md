# Sync Branches Action

Automated branch synchronization that keeps a target branch (for example `main`) in sync with a source branch (for example `develop`).

## Features

- **Automatic triggers**: Runs on every push to source branch or manual dispatch
- **Clean merges**: Direct push to target branch when no conflicts exist
- **Conflict handling**: Opens or updates fallback PR and fallback issue artifacts for manual resolution
- **Idempotent**: Skips sync if source is already in target (no unnecessary merges)
- **Safer token policy**: Push-triggered runs use `github.token`; optional PAT is limited to manual dispatch
- **Policy-compliant fallback titles**: Fallback PR titles include a Jira key (derived or override)
- **Customizable**: Configure branches, labels, reviewers, and commit identity

## Consumption Modes

This automation is intentionally distributed in two modes so internal (`rdk-e`) and OSS (`rdkcentral`) repositories can consume the same behavior using different delivery models.

### Mode A: By Reference (RDK-E/Internal)

How it works:
- Consumer workflow references the shared action directly: `uses: rdk-e/app-gateway-automation/actions/sync-branches@actions-v1`.
- The workflow file lives in the consumer repo, while action logic is executed from the shared release tag.

Why this mode exists:
- Centralized ownership and rollout from one repository.
- No vendored action files in consumer repositories.
- Best fit for internal repositories that can access `rdk-e` and internal runners.

Tradeoffs:
- Requires cross-repository access to `rdk-e/app-gateway-automation`.
- Behavior follows the moved major tag (`actions-v1`) release lifecycle.

### Mode B: Install Mode (OSS/Vendored)

How it works:
- Installer/wrapper copies managed files into the consumer repository.
- Consumer workflow calls the local action path: `uses: ./.github/actions/sync-branches`.

Why this mode exists:
- Works when OSS repositories cannot consume private `rdk-e` references.
- Keeps automation self-contained and auditable in the target repository.
- Enables explicit, versioned upgrades via release bundle + lock file.

Tradeoffs:
- Consumer repositories carry managed automation files.
- Upgrades require running installer/wrapper and merging the resulting change.

## Mode A Quick Start (By Reference)

### 1. Copy The Consumer Template

Copy `actions/sync-branches/consumer-template.yml` into your consumer repo as `.github/workflows/sync-develop-to-main.yml`.

The default template uses:
- `rdk-e/app-gateway-automation/actions/sync-branches@actions-v1`
- `runs-on: comcast-ubuntu-latest`
- `source_branch: develop`
- `target_branch: main`

### 2. Optional: Add PAT Secret For Manual Dispatch

If you want manual dispatch runs to use a PAT (for protected branch scenarios), add `SEMANTIC_RELEASE_TOKEN` in the consumer repo.

Required PAT scopes:
- `contents:write`
- `pull-requests:write`
- `issues:write`

Push-triggered runs still use `github.token` by design.

### 3. Example Consumer Workflow

```yaml
name: Sync develop to main

on:
  push:
    branches: [develop]
  workflow_dispatch:

permissions:
  contents: write
  pull-requests: write

jobs:
  sync:
    runs-on: comcast-ubuntu-latest
    steps:
      - uses: rdk-e/app-gateway-automation/actions/sync-branches@actions-v1
        with:
          source_branch: ${{ github.event.inputs.source_branch || 'develop' }}
          target_branch: ${{ github.event.inputs.target_branch || 'main' }}
          token: ${{ github.event_name == 'workflow_dispatch' && (secrets.SEMANTIC_RELEASE_TOKEN || github.token) || github.token }}
```

### 4. Optional: Generate From Installer

You can generate a repo-specific workflow from `tools/install-workflow.sh`:

```bash
./tools/install-workflow.sh \
  --workflow sync-develop-to-main \
  --repo-dir ../firebolt-entos-apis \
  --source-branch develop \
  --target-branch main \
  --secret-name SEMANTIC_RELEASE_TOKEN \
  --runner comcast-ubuntu-latest \
  --open-pr
```

This writes `.github/workflows/sync-develop-to-main.yml` in the target repo.

## Mode B Quick Start (Install Mode)

For OSS repositories that cannot directly consume private reusable actions, use the bundle installer flow:

The bundle installer renders the workflow to use the local action path:

- `uses: ./.github/actions/sync-branches`

1. Download release assets from `app-gateway-automation`:
- `sync-branches-<version>.tar.gz`
- `sync-branches-<version>.manifest.yaml`
- `sync-branches-<version>.sha256`

2. Install into the OSS repo:

```bash
./tools/install-automation-bundle.sh install \
  --repo-dir /path/to/oss-repo \
  --bundle /path/to/sync-branches-<version>.tar.gz
```

3. Check current state:

```bash
./tools/install-automation-bundle.sh status \
  --repo-dir /path/to/oss-repo \
  --bundle /path/to/sync-branches-<version>.tar.gz
```

4. Update to a newer bundle:

```bash
./tools/install-automation-bundle.sh update \
  --repo-dir /path/to/oss-repo \
  --bundle /path/to/sync-branches-<new-version>.tar.gz
```

5. Optional: One-command OSS onboarding wrapper (install/update + branch + commit + PR):

```bash
./tools/onboard-oss-sync.sh onboard \
  --repo-dir /path/to/oss-repo \
  --bundle /path/to/sync-branches-<version>.tar.gz
```

Managed files:
- `.github/actions/sync-branches/`
- `.github/workflows/sync-develop-to-main.yml`
- `.github/automation-install.lock.json`

Drift policy:
- Managed file drift is blocked by default.
- Use `--force` to overwrite managed drift.

## Release Then Onboard Consumers

For fastest rollout, use this order:

1. Run `.github/workflows/release-actions.yml` on `main` to publish `actions-vX.Y.Z`.
2. Confirm the floating major tag (`actions-v1`) moved to the new release.
3. Choose consumption mode per repo:
  - Mode A (RDK-E): install consumer workflow pinned to `@actions-v1`.
  - Mode B (OSS): install or update managed bundle files.
4. In each consumer repo, ensure permissions include `contents: write`, `pull-requests: write`, and `issues: write`.
5. Trigger `workflow_dispatch` once to validate token and branch settings.

## Usage Examples (Mode A By Reference)

### Example 1: Simple develop -> main sync

```yaml
jobs:
  sync:
    runs-on: comcast-ubuntu-latest
    steps:
      - uses: rdk-e/app-gateway-automation/actions/sync-branches@actions-v1
        with:
          token: ${{ github.token }}
```

### Example 2: Custom branches with token

```yaml
jobs:
  sync:
    runs-on: comcast-ubuntu-latest
    steps:
      - uses: rdk-e/app-gateway-automation/actions/sync-branches@actions-v1
        with:
          source_branch: staging
          target_branch: production
          token: ${{ secrets.MY_PAT }}
```

### Example 3: Custom PR behavior

```yaml
jobs:
  sync:
    runs-on: comcast-ubuntu-latest
    steps:
      - uses: rdk-e/app-gateway-automation/actions/sync-branches@actions-v1
        with:
          assign_reviewers: "@platform-team,@devops-team"
          pr_labels: "auto-merge,requires-review"
          git_user_name: "Release Bot"
          git_user_email: "releases@company.com"
          token: ${{ secrets.SEMANTIC_RELEASE_TOKEN }}
```

## Inputs

| Input | Default | Description |
|-------|---------|-------------|
| `source_branch` | `develop` | Branch to sync from |
| `target_branch` | `main` | Branch to sync to |
| `git_user_name` | `github-actions[bot]` | Committer name for merge commits |
| `git_user_email` | `github-actions[bot]@users.noreply.github.com` | Committer email |
| `pr_branch_prefix` | `auto-sync` | Prefix for fallback PR branches |
| `assign_reviewers` | `` | Comma-separated team/user handles for PR assignment |
| `fallback_pr_jira_key` | `` | Optional Jira key override for fallback PR title (e.g. `RDKEMW-12345`) |
| `pr_labels` | `auto-sync,needs-manual-merge` | Comma-separated labels for fallback PR |
| `issue_labels` | `auto-sync,needs-manual-merge,needs-human-attention` | Comma-separated labels for fallback issue |
| `escalation_mentions` | `` | Optional mentions added to fallback issue body |

## Token Input

| Input | Required | Description |
|--------|----------|-------------|
| `token` | No | Auth token for push/PR operations. Falls back to `github.token` if not provided by caller workflow. |

## Behavior

### Success Case: No Conflicts
```
push to develop
    ↓
workflow runs
    ↓
merge-base check: develop NOT in main ✓
    ↓
attempt merge: success ✓
    ↓
direct push to main: success ✓
    ↓
✅ main updated, no PR created
```

### Fallback Case: Conflict or Protected Branch
```
push to develop
    ↓
workflow runs
    ↓
merge-base check: develop NOT in main ✓
    ↓
attempt merge: CONFLICT ✗ (or direct push fails due to protection)
    ↓
create branch: auto-sync/develop-to-main
    ↓
open/update PR with labels & reviewers
  ↓
open/update issue with compare link + run link
    ↓
⚠️  Human intervention required via PR/issue artifacts
```

### Idempotent Case: Already Synced
```
push to develop (but develop already in main)
    ↓
workflow runs
    ↓
merge-base check: develop IS in main ✓
    ↓
✅ Skip sync (nothing to do)
```

## Token Scope Reference

For branch-protected targets, use a fine-grained PAT with:

```
Permissions:
  - Contents: Read & write
  - Pull requests: Read & write

Repository access:
  - All repositories (or specific repo)
```

[Create fine-grained PAT](https://github.com/settings/personal-access-tokens/new)

## Troubleshooting

**Q: Workflow runs but push fails to protected branch**
- Ensure your token has `contents:write`, `pull-requests:write`, and `issues:write`
- If you are using the template, verify `SEMANTIC_RELEASE_TOKEN` exists when you need PAT-backed dispatch
- Confirm the workflow expression passes `github.token` for push runs and optional PAT only for dispatch runs

**Q: PR opens but I don't want it**
- If it's just checking feature, use `workflow_dispatch` instead of automatic trigger
- Adjust branch protection rules if conflicts are expected

**Q: Why did an issue open even when a fallback PR exists?**
- The action always upserts a fallback issue on human-intervention runs for consistent triage tracking.
- The issue body includes compare and run links plus current PR status.

**Q: I need to sync multiple branch pairs**
- Create separate workflow files, each calling sync-branches with different `source_branch`/`target_branch`
- Or use a matrix job in your calling workflow

## File Location

This integration guide is stored in `actions/sync-branches/` for easy discovery.

The composite action is at `actions/sync-branches/action.yml`.

Versioned releases are published from `actions-vX.Y.Z` tags and include the action surface (`action.yml`, `consumer-template.yml`, `consumer-template-oss.yml`, `README.md`, `CHANGELOG.md`, and `scripts/`).

---

**Integration Branch**: `feat/reusable-sync-automation`

For the latest stable version, check the main branch or GitHub Releases.
