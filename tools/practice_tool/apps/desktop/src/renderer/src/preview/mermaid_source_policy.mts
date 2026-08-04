const MERMAID_CONFIGURATION_DIRECTIVE_PATTERN = /%%\s*\{/u;
const MERMAID_FRONTMATTER_PATTERN = /^\s*---[\t ]*\r?\n[\s\S]*?\r?\n---[\t ]*(?:\r?\n|$)/u;

export function mermaid_source_has_document_configuration(source: string): boolean {
  if (MERMAID_CONFIGURATION_DIRECTIVE_PATTERN.test(source)) return true;
  return MERMAID_FRONTMATTER_PATTERN.test(source);
}
