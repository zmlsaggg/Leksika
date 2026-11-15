#!/usr/bin/env bash
# Future job: verify release downloads, generate manifests, submit a PR to an external repo.
set -euo pipefail
kind=${1:?scoop или homebrew}; version=${2:?version}; repository=${3:?owner/repo}
[[ "$version" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]] || { echo 'Только стабильные версии' >&2; exit 1; }
[[ "$repository" =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$ ]] || exit 1
case "$kind" in scoop|homebrew) ;; *) exit 1 ;; esac
root=$PWD
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
# Только нужные архивы, но проверяем каждый по опубликованной сумме.
gh release download "v$version" -R "$GITHUB_REPOSITORY" -D "$tmp" -p zz-checksums.txt
if [[ "$kind" == scoop ]]; then
  files=(leksika-native-windows-x64.zip)
else
  files=(leksika-native-macos-arm64.tar.gz leksika-native-macos-x64.tar.gz leksika-native-linux-x64.tar.gz)
fi
for f in "${files[@]}"; do
  gh release download "v$version" -R "$GITHUB_REPOSITORY" -D "$tmp" -p "$f"
  grep -F "  $f" "$tmp/zz-checksums.txt" > "$tmp/one.sum"
  (cd "$tmp" && sha256sum -c one.sum)
done
python3 "$root/packaging/update_templates.py" "$version" "$tmp/zz-checksums.txt" "$tmp/generated"
gh auth setup-git
gh repo clone "$repository" "$tmp/repo"
cd "$tmp/repo"
git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git switch -c "leksus-core-$version"
if [[ "$kind" == scoop ]]; then
  mkdir -p bucket; cp "$tmp/generated/leksus-core.json" bucket/leksus-core.json
else
  mkdir -p Formula; cp "$tmp/generated/leksus-core.rb" Formula/leksus-core.rb
fi
git add .
git commit -m "Обновить Лексус до $version"
git push -u origin "leksus-core-$version"
gh pr create -R "$repository" --title "Лексус $version" --body "Проверены SHA-256 архивов релиза v$version." --head "leksus-core-$version"
