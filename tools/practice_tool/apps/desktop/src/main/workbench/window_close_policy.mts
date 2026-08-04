export function requires_close_confirmation(has_workspace: boolean, dirty_count: number): boolean {
  return has_workspace || dirty_count > 0;
}
