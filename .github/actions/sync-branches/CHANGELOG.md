## [Unreleased]

## [1.1.3] - 2026-10-15

### Fixed
- Fallback PR lookup (existing-PR match and stale-PR cleanup) now excludes fork PRs via `isCrossRepository`, so a cross-repo PR sharing the same head branch name is no longer mistaken for this action's own fallback PR.
- Stale fallback PR cleanup now requires the branch suffix to be exactly 12 hex characters, so a different source/target branch pair whose prefix is a superset of this pair's prefix (e.g. `develop-to-main-hotfix -> main` vs `develop -> main`) can no longer have its fallback PR closed by mistake.
- PR-creation permission-denial detection no longer matches on the bare string `createPullRequest`, which could misclassify unrelated GraphQL errors (e.g. "No commits between ... (createPullRequest)") as a token-policy denial.

## [1.1.2] - 2026-10-13

### Fixed
- Label-ensure step no longer passes `--force` to `gh label create`, so re-running a fallback no longer overwrites an existing label's color/description; non-existence is still treated as non-fatal via `|| true`.

## [1.1.1] - 2026-10-12

### Fixed
- Fallback issue manual-merge checklist now instructs responders to merge `target_branch` into the fallback branch instead of `source_branch`, which was a no-op since the fallback branch is created directly from the source commit.

## [1.1.0] - 2026-10-12

### Added
- Branch input validation before sync operations.
- Stable temporary workspace flow for sync/merge/push operations.
- Optional Jira key override input `fallback_pr_jira_key` for fallback PR titles.
- New fallback issue controls: `issue_labels` and `escalation_mentions`.
- Fallback issue upsert on all human-intervention-required runs.

### Changed
- Fallback PR title format now includes Jira key (`<JIRA>: auto-sync ...`).
- Drift/concurrency push rejection path now distinguishes "already synced after push reject" from real push failures.
- Action result output values are now `merged`, `skipped`, and `human-intervention-required`.
- Fallback PR branches are now tied to the source commit; newer source states get a new PR, while retries for the same source commit preserve manual edits.
- Older fallback PRs are closed only after a replacement PR is available.
- Documented that `GITHUB_TOKEN` fallback does not trigger downstream Actions workflows; configured tokens are passed for push and manual runs.

### Outputs
- Added `issue_number` output.
- `pr_number` output now reflects fallback PR when present.

## [1.0.0] - 2026-06-17

### Added
- Initial reusable workflow `sync-branches.yml` for develop-to-main synchronization
- Support for direct push when possible, PR fallback on conflict or push failure
- Optional `sync_token` secret with fallback to `github.token`
- Optional per-repository reviewer assignment via `assign_reviewers` input
- Automatic label creation before PR creation via `pr_labels` input
- Configurable source/target branches (defaults to develop/main)
- Concurrency guard to prevent duplicate runs
- Comprehensive job outputs: `result` (`merged|pr-opened|skipped`) and `pr_number`

### Notes
- The reusable workflow treats `sync_token` as optional and falls back to `github.token` when no repo PAT is provided.
- The `result` output values are `merged`, `pr-opened`, and `skipped`.
