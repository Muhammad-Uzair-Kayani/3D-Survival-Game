# GitPush for Last Light

Copy **GitPush.bat** to the actual root of your destination Git checkout. The BAT
is self-contained: its Python implementation is embedded in the same file.
Git for Windows and Python 3.10+ are required; the launcher first uses the game's
local build venv if present. No extra pip packages are required.

Double-click it, or run `GitPush.bat` in Command Prompt. It asks for the GitHub URL,
destination branch, any missing local author identity, and your commit message.
Enter at the commit-message prompt cancels before committing. Review the staged
file summary and destination before entering a message. Existing unpublished
commits can be pushed without creating another commit.

## Your pasted error

The LF/CRLF notices are warnings. The fatal error came from staging loose Godot
source under `vendor/godot`. This tool enables long paths for its Git commands,
backs up the original index, removes only loose Godot **index entries**, and
records the verified `godot.lock.json` release as a submodule gitlink. It preserves
engine files on disk and other `.gitmodules` sections.

If the copied engine lacks valid Git metadata, you must confirm publishing the
locked upstream release rather than that unversioned folder. Changes inside such
a folder are not uploaded. A valid engine checkout with edits/missing files or a
different HEAD stops the upload for review. Engine changes need a published
engine repository/commit and an updated lock.

## History and failure handling

- The destination is entered by you; there is no hardcoded repository.
- HTTPS Git Credential Manager and GitHub SSH URLs are supported. Tokens are not
  accepted inside repository URLs. Git handles authentication normally.
- Missing `origin` can be added, or its URL changed. Separate/multiple push URLs
  and mirror remotes stop for review.
- Behind/divergent/unrelated remote history offers a NEW remote branch. Existing
  remote commits are preserved. The local branch is not renamed or switched;
  successful target choice is remembered for the next invocation.
- No automatic pull, merge, rebase, reset, stash, force-push, or engine checkout.
- Existing merge/rebase/conflict/lock states, tracked generated folders,
  additional submodules and common private-key/config filenames stop for review.
- `git add -A` includes additions, modifications, dotfiles and deletions. Ignore
  rules still apply. This is not a complete secret scanner: review the summary.
- Blob sizes are checked in both the index and unpublished history against
  GitHub's 100 MiB limit. LFS migration/history edits require manual review.
- Commit messages are UTF-8 file data passed to Git, not shell commands.
- Normal push rejects concurrent remote updates and branch protection failures.
  If a push fails, the local commit remains available. Rerun after addressing the
  displayed authentication, permission, hook or history error.
- The remote commit is checked after push. If another writer changes it before
  verification, the tool reports that instead of claiming verified success.

The staging backup is under the repository's Git metadata directory in
`lastlight-push-backups/`. No automatic rollback overwrites concurrent staging.
A cancellation/error can leave staged changes and local configuration updates;
working files remain on disk. Local Git hooks still run normally.

For immediate play by others, include the matching tested Windows ZIP at
`Deliverables/LastLight-Windows-x64.zip`. Keep `game/game-content.pck` tracked.
A source upload without the binary ZIP requires a build before playing.

A loose preserved engine folder may need separate setup repair for future source
builds. The simplest reproducible setup is a fresh short-path checkout with
`--recurse-submodules`. This uploader does not rebuild or test the game.

`GitPush.bat --check` checks only launcher/Git/root/operation readiness, with no
network, staging, commit or push. The optional `validation/` directory contains
independent local-bare-repository tests, including history divergence, a push race,
a copied engine, large files and shell-sensitive commit messages. Run
`python -m unittest discover -s validation -v` to execute them.
