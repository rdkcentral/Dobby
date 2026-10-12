## [Unreleased]

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
