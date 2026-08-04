export interface identified_document_tab {
  snapshot: { document_id: string };
}

export type add_document_tab_result<tab_type> = {
  tabs: tab_type[];
  status: "added" | "existing" | "limit";
};

export function add_document_tab<tab_type extends identified_document_tab>(
  tabs: tab_type[],
  candidate: tab_type,
  maximum_tabs: number,
): add_document_tab_result<tab_type> {
  if (tabs.some((tab) => tab.snapshot.document_id === candidate.snapshot.document_id)) {
    return { tabs, status: "existing" };
  }
  if (tabs.length >= maximum_tabs) return { tabs, status: "limit" };
  return { tabs: [...tabs, candidate], status: "added" };
}
