---
name: conventional-commits
description: Create clean, well-scoped git commits following the Conventional Commits 1.0.0 spec. Use whenever the user asks to commit, stage and commit, split changes into commits, write a commit message, or push work — especially when many files changed and the work should be grouped into logical commits.
---

# Conventional Commits

Produce commits whose messages follow Conventional Commits 1.0.0, and whose
*contents* are logically grouped — one coherent change per commit.

## Message format

```
<type>(<scope>)!: <description>

<body>

<footer>
```

- **type** — required, lowercase. See table below.
- **scope** — optional, lowercase noun in parentheses naming the affected area
  (module, subsystem, folder). Prefer the project's own vocabulary.
- **!** — optional, marks a breaking change. Also add a
  `BREAKING CHANGE: <what broke and how to migrate>` footer.
- **description** — required. Imperative mood ("add", not "added"/"adds"),
  no trailing period, ≤ 72 chars for the whole subject line, lowercase start.
- **body** — optional, blank line before it. Wrap at ~72 chars. Explain
  *why*, and what changed at a level a reviewer cares about; use `-` bullets
  when several distinct things changed. Never restate the diff line by line.
- **footer** — optional: `BREAKING CHANGE: ...`, `Refs: #123`, `Closes: #123`,
  trailers required by the repo or harness (e.g. `Co-Authored-By:`).

## Types

| type | use for |
|---|---|
| `feat` | a new user-visible capability |
| `fix` | a bug fix |
| `perf` | a change that improves performance |
| `refactor` | restructuring with no behavior change |
| `style` | formatting, whitespace, naming only |
| `docs` | documentation only |
| `test` | tests only |
| `build` | build system, dependencies, packaging |
| `ci` | CI configuration and pipelines |
| `chore` | maintenance that fits nothing above |
| `revert` | reverts a previous commit (body: `This reverts commit <sha>.`) |

## Workflow

1. **Survey** — `git status --porcelain` and `git diff --stat`. For anything
   non-obvious, read the actual diff (`git diff -- <path>`) before naming it.
   Never guess a type from a filename alone.
2. **Group** — partition the changes into commits, each one coherent change
   with a single type and scope. Good axes: subsystem, feature, fix-vs-refactor.
   A file touched for two reasons goes in the commit matching its dominant
   reason — do not split a file's hunks unless the user asked for it.
3. **Stage precisely** — `git add -- <paths>` per group. Never `git add -A`
   when you are making multiple commits. Re-check `git status` between commits.
4. **Commit** — one commit per group, message per the format above. Use a
   heredoc so multi-line bodies survive the shell:
   ```sh
   git commit -F - <<'MSG'
   feat(scope): short imperative description

   Why this change was needed.
   MSG
   ```
5. **Verify** — `git log --oneline` and `git status` afterwards; confirm
   nothing intended was left unstaged.

## Rules

- Never `--amend` an existing commit unless asked; make a new one.
- Never `--no-verify` or bypass signing unless asked. If a hook fails,
  fix the cause.
- Commit only what the user asked for. Do not commit unrelated stray files,
  build output, or secrets — call those out instead.
- Commit or push only when the user asked. If on the default branch (`main`
  / `master`), create a branch first.
- Branch names mirror the convention: `<type>/<short-kebab-description>`.
- Untracked new files still need a type — new source files are usually `feat`.
- If the whole change genuinely is one thing, one commit is correct. Do not
  manufacture splits.
