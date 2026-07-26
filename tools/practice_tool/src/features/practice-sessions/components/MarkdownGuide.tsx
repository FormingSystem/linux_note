import Markdown from "react-markdown";
import { isValidElement, type ReactElement, type ReactNode } from "react";
import { toString } from "mdast-util-to-string";
import rehypeHighlight from "rehype-highlight";
import remarkParse from "remark-parse";
import remarkGfm from "remark-gfm";
import { unified } from "unified";
import { visit } from "unist-util-visit";
import type { Heading } from "mdast";
import MermaidDiagram from "./MermaidDiagram";

export type MarkdownHeading = {
  id: string;
  level: 2 | 3 | 4;
  title: string;
  line: number;
};

export function markdownPlainText(value: string) {
  return value
    .replace(/\\([\\`*{}[\]()#+.!_>-])/g, "$1")
    .replace(/[*`[\]]/g, "")
    .trim();
}

export function markdownHeadings(markdown: string): MarkdownHeading[] {
  const used = new Map<string, number>();
  const headings: MarkdownHeading[] = [];
  const visibleMarkdown = markdown.replace(/^\uFEFF?---\s*\r?\n[\s\S]*?\r?\n---\s*(?:\r?\n|$)/, "");
  const tree = unified().use(remarkParse).parse(visibleMarkdown);
  visit(tree, "heading", (node: Heading) => {
    if (node.depth < 2 || node.depth > 4) return;
    const title = markdownPlainText(toString(node));
    const base = title
      .toLocaleLowerCase()
      .replace(/[^\p{Letter}\p{Number}_-]+/gu, "-")
      .replace(/^-+|-+$/g, "") || "section";
    const count = used.get(base) ?? 0;
    used.set(base, count + 1);
    headings.push({
      id: count === 0 ? base : `${base}-${count + 1}`,
      level: node.depth as 2 | 3 | 4,
      title,
      line: node.position?.start.line ?? 0,
    });
  });
  return headings;
}

export default function MarkdownGuide({ markdown, headings = markdownHeadings(markdown) }: { markdown: string; headings?: MarkdownHeading[] }) {
  const visibleMarkdown = markdown.replace(/^\uFEFF?---\s*\r?\n[\s\S]*?\r?\n---\s*(?:\r?\n|$)/, "");
  const headingIds = new Map(headings.map((entry) => [`${entry.level}:${entry.line}`, entry.id]));
  const heading = (level: 2 | 3 | 4) => ({ children, node }: { children?: React.ReactNode; node?: { position?: { start: { line: number } } } }) => {
    const id = headingIds.get(`${level}:${node?.position?.start.line ?? 0}`);
    const Tag = `h${level}` as const;
    return <Tag id={id}>{children}</Tag>;
  };

  return (
    <article className="learning-document">
      <Markdown
        remarkPlugins={[remarkGfm]}
        rehypePlugins={[[rehypeHighlight, { detect: false, ignoreMissing: true, plainText: ["mermaid"] }]]}
        components={{
          h2: heading(2),
          h3: heading(3),
          h4: heading(4),
          code: ({ className, children }) => className === "language-mermaid"
            ? <MermaidDiagram source={String(children).replace(/\n$/, "")} />
            : <code className={className}>{children}</code>,
          pre: ({ children }) => isMermaidElement(children) ? <>{children}</> : <pre>{children}</pre>,
        }}
      >
        {visibleMarkdown}
      </Markdown>
    </article>
  );
}

function isMermaidElement(children: ReactNode) {
  if (!isValidElement(children)) return false;
  return (children as ReactElement<{ className?: string }>).props.className === "language-mermaid"
    || (children as ReactElement).type === MermaidDiagram;
}
