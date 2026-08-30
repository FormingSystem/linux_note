import fs from "node:fs";
import path from "node:path";

const typora_root = path.resolve("..");
const deployment_files = [
  "configure_windows.cmd",
  "configure_windows.ps1",
  "check_configuration_windows.ps1",
  "restore_configuration_windows.ps1",
  "configure.sh",
  "check_configuration.sh",
  "restore_configuration.sh",
  "scripts/lib/typora_environment.ps1",
  "scripts/lib/typora_environment.sh",
  "enhancements/scripts/install_windows.ps1",
  "enhancements/scripts/restore_windows.ps1",
];

const sources = new Map(deployment_files.map((relative_path) => {
  const file_path = path.join(typora_root, relative_path);
  if (!fs.existsSync(file_path)) throw new Error(`deployment file is missing: ${relative_path}`);
  return [relative_path, fs.readFileSync(file_path, "utf8")];
}));

const forbidden_install_locations = [
  /[a-z]:\\program files(?: \(x86\))?\\typora/iu,
  /[a-z]:\\users\\[^\s"']+\\.*typora/iu,
  /[a-z]:\\typora(?:\\|["'])/iu,
  /\/usr\/share\/typora(?:\/|["'])/u,
  /\/opt\/typora(?:\/|["'])/u,
];
for (const [relative_path, source] of sources) {
  for (const forbidden_pattern of forbidden_install_locations) {
    if (forbidden_pattern.test(source)) {
      throw new Error(`hard-coded Typora installation path in ${relative_path}: ${forbidden_pattern}`);
    }
  }
}

const powershell_environment = sources.get("scripts/lib/typora_environment.ps1");
for (const marker of ["TYPORA_ROOT", "Get-Process Typora", "Registry::", "Read-Host", "^/mnt/", "^/([A-Za-z])"]) {
  if (!powershell_environment.includes(marker)) throw new Error(`PowerShell discovery marker is missing: ${marker}`);
}

const bash_environment = sources.get("scripts/lib/typora_environment.sh");
for (const marker of ["UCRT64", "Linux", "cygpath", "TYPORA_ROOT", "/dev/tty", "typora_root_from_candidate"]) {
  if (!bash_environment.includes(marker)) throw new Error(`Bash discovery marker is missing: ${marker}`);
}

console.log(`validated ${deployment_files.length} cross-platform deployment files without fixed Typora locations`);
