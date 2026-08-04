export type workbench_theme = "dark" | "light";

export function is_workbench_theme(value: unknown): value is workbench_theme {
  return value === "dark" || value === "light";
}

export function system_workbench_theme(): workbench_theme {
  return window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
}
