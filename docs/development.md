# Development and version control

The original assignment and its EBNF appendix are preserved at the repository root.
Commits record implementation milestones. The assignment calls for at least ten
meaningful commits distributed throughout the development period, one continuing
private remote repository, reviewer access, and recommends a `phase1-final` tag.

Language specification:
[Douglas Thain, Appendix B: The B-Minor Language](https://dthain.github.io/books/compiler/chapterB.pdf),
second edition, revision July 28, 2026. This is the textbook dialect rather than
the separately extended 2023 or 2025 course dialects.

## Continuing the repository

Inspect the development history and the tagged phase-one version:

```sh
git log --oneline --reverse
git show phase1-final
```

Keep this repository for later phases. Configure your own Git identity for future
work and preserve the development history across phases.

The account owner can create an empty **private** repository on GitHub or
GitLab, then connect this local repository (replace the example placeholder):

```sh
git remote add origin <PRIVATE_REPOSITORY_URL>
git push -u origin main
git push origin phase1-final
```

Do not initialize the remote with an unrelated README/history; keep the same
repository through all phases. Grant the actual course reviewer the required
access in the hosting service. Neither an account identity, remote URL nor
reviewer account was provided during this implementation, so no upload or
access change was performed. The CI workflow is prepared but has not been
run on a hosting service.
