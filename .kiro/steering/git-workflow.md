# Git workflow for MacTermWin

## Rules

- **Push directly to the default branch** (`feat/initial-skeleton`). No
  feature branches, no pull requests.
- One commit per logical change is fine; squash if iterating.
- Always use the sandbox `github_push_to_remote` tool, never `git push`
  via bash.

## Why no PRs

This is a solo pet-project repo. Branches and PRs add ceremony with no
review benefit. The user wants a tight loop: edit -> commit -> push ->
pull on the Windows box -> rebuild.
